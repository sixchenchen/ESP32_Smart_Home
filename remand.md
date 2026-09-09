# ESP32 Smart Home 问题清单

> 审查日期：2026-09-09
> ESP-IDF 版本：6.0.2，目标芯片 ESP32，Flash 4MB

***

## 一、分区表与构建配置

### P0-分区表无法 OTA + 浪费 1.5MB Flash

- **文件**：`partitions-4MiB.csv`

- **问题**：

  - 只有单个 `factory` app 分区（0x10000, 2MB），无 `ota_0` / `ota_1` / `otadata`，完全无法 OTA 升级

  - `SPIFFS` 分区 0x180000 = 1.5MB 从未被使用（代码无任何 `esp_spiffs_*` 调用，无 CMake embed spiffs image），纯浪费

  - 分区名拼写错误 `facotry`（少一个 t），虽不影响当前运行但 OTA 升级会因名字不匹配拒绝启动

- **建议**：factory 缩到 1.2MB，增加 `ota_0` / `ota_1` 各 1.2MB，NVS 扩到 24KB，砍掉 SPIFFS 或缩到 128KB

### P3-编译优化等级自相矛盾

- **文件**：`components/BSP/CMakeLists.txt` L56-L60 + sdkconfig `CONFIG_COMPILER_OPTIMIZATION_DEBUG`

- **问题**：BSP 组件强制 `-ffast-math -O3` 覆盖了全局 `-Og` 调试配置；`-ffast-math` 关闭 IEEE-754 浮点语义

- **建议**：发布版统一 `-O2`，去掉 `-ffast-math`（本工程无浮点计算，风险可忽略但配置具有误导性）

### P3-断言与日志等级

- **文件**：sdkconfig `CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_ENABLE=2`、`CONFIG_LOG_DEFAULT_LEVEL_INFO`

- **问题**：所有 `ESP_ERROR_CHECK` 失败都会 abort + 打印 backtrace + reboot（`CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT`），运行期任何小错误变成死机；INFO 级日志串口 115200 大量输出，吃 CPU 和 UART 带宽

- **建议**：发布版关断言、日志降到 WARN

### P2-main 任务栈过小

- **文件**：sdkconfig `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`

- **问题**：main 栈仅 3.5KB，main.c 里有 `strlen` + `sprintf` 组合，安全边界紧张

- **建议**：提到 4096 或 4608

***

## 二、WiFi Manager 状态机

### P1-WiFi 事件 handler 内阻塞 3 秒

- **文件**：`components/BSP/WIFI/WIFI_MANAGER/wifi_manager.c` L86

- **问题**：配网态重连时 `vTaskDelay(pdMS_TO_TICKS(3000))` 放在 WiFi 事件回调里执行，WiFi 事件回调在系统任务上下文，阻塞回调会阻塞后续事件派发

- **建议**：发信号量让独立任务延时

### P2-已连接态断线无延时重连

- **文件**：`wifi_manager.c` L97-L104

- **问题**：`WIFI_MANAGER_CONNECTED` 态断线立刻 `esp_wifi_connect()`，而配网态有 3 秒延时策略不一致。路由器瞬断时会打爆重连

- **建议**：统一使用指数退避（1s → 2s → 4s → 最大 60s）

### P2-IP\_EVENT\_STA\_GOT\_IP 内直接切换 APSTA → STA

- **文件**：`wifi_manager.c` L154

- **问题**：MQTT 正在连接时调用 `wifi_mode_switch_sta()` 转纯 STA，模式切换与 esp-mqtt 内部 socket 可能竞争

- **建议**：加状态保护或在 MQTT 启动完成后再切

### P3-初始化时无条件开 APSTA

- **文件**：`wifi_manager.c` L204

- **问题**：`esp_wifi_set_mode(WIFI_MODE_APSTA)` 在 init 时调用，即使 NVS 有 SSID 也先开 AP 再切模式，启动时间浪费

- **建议**：根据 NVS 有无配置决定初始模式

### P3-wifi\_list / count 全局死变量

- **文件**：`wifi_manager.c` L44-L45

- **问题**：`wifi_scan_result_t wifi_list[20]`、`uint16_t count` 声明为全局但从未被任何外部引用

- **建议**：删除

***

## 三、HTTP Server + 前端

### P0-/factory\_reset 无鉴权

- **文件**：`components/BSP/HTTP_SERVER/http_server.c` L134

- **问题**：局域网任何人 GET `/factory_reset` 就能重置设备

- **建议**：加一次性确认 token（如 POST + 确认字段或设备签名）

### P2-POST body 截断风险

- **文件**：`http_server.c` L52-L54

- **问题**：`httpd_req_recv` 只读一次 256 字节，大请求体被截断且不报错

- **建议**：循环读直到读完 body（esp\_http\_server 正确姿势）

### P2-SSID/密码 strcpy 无长度检查

- **文件**：`http_server.c` L76-L77

- **问题**：`strcpy(ssid, ssid_json->valuestring)` 无边界检查，SSID 超过 32 字节导致栈溢出；同理 password 超过 64 字节

- **建议**：改用 `strlcpy` 或 `snprintf` 带长度限制

### P3-恢复出厂在 handler 里创建任务重启 WiFi

- **文件**：`http_server.c` L124-L151

- **问题**：handler 里先 `httpd_resp_sendstr` 再 `xTaskCreate` 重启 WiFi，如果 httpd 同时在处理其他 URI，WiFi 重启可能让 httpd 异常（两者同属 esp-netif）

- **建议**：让恢复出厂流程在 httpd 完全退出后执行

### P3-wifi\_scan\_handler 栈上 720 字节大数组

- **文件**：`http_server.c` L88

- **问题**：`wifi_scan_result_t result[20]` 是栈上数组，约 720 字节；httpd handler 栈配置 8KB，虽然吃得起但属于大分配

- **建议**：改静态/全局

### P1-前端 togglePassword() 未实现

- **文件**：`components/BSP/HTTP_SERVER/web/index.html` L59 + `app.js`

- **问题**：index.html 绑定 `onclick="togglePassword()"` 但 app.js 根本没实现该函数，点击眼睛按钮抛 `ReferenceError`，密码框永远无法切换明文显示

- **建议**：在 app.js 里实现 togglePassword，切换 `<input type="password"/"text">`

### P1-前端 statusBar / scanningHint 死元素

- **文件**：`index.html` L26-L28, L42-L44 + `app.js`

- **问题**：`statusBar` / `statusText` / `scanningHint` 三个 UI 元素都存在，但 app.js 从未更新它们，"正在扫描..."提示永远不出现

- **建议**：在 scan\_wifi / connect\_wifi 里更新状态 UI

### P2-配网后无连接结果反馈

- **文件**：`app.js` L22-L25

- **问题**：`connect_wifi` 提交后立即显示 "wifi connecting..."，WiFi 异步连接需要几秒，用户不知道实际结果

- **建议**：后端增加 `/status` 轮询接口，前端配网后轮询实际连接结果

***

## 四、MQTT 层

### P0-Broker 配置硬编码

- **文件**：`components/BSP/MQTT/mqtt_manager.c` L60-L77

- **问题**：`mqtt://192.168.124.6:1883`、用户名 `MQTT1`、密码 `123456`、遗嘱 topic 全写死，无法量产

- **建议**：收敛到 NVS，首启注册时下发，支持 TLS

### P1-Topic 定义重复 + device\_id 写死

- **文件**：`mqtt_service.h` L4-L10

- **问题**：`MQTT_CONTROL_TOPIC` 和 `TOPIC_CONTROL` 值相同；`device001` 硬编码应从 `esp_efuse_get_mac` 派生

- **建议**：删除重复宏，device\_id 运行时从 MAC 生成

### P2-重连时重复订阅

- **文件**：`mqtt_manager.c` L26

- **问题**：`MQTT_EVENT_CONNECTED` 里 `esp_mqtt_client_subscribe`，重连时会重复调用（esp-mqtt 内部会处理但语义上不清晰）

- **建议**：init 时订阅一次

### P2-未知 cmd 静默丢弃不回复

- **文件**：`mqtt_service.c` L115

- **问题**：收到未知 `cmd` 时静默丢弃且不回复 reply，后端指令"石沉大海"

- **建议**：加 `else` 分支回复 `{"result":0,"err":"unknown_cmd"}`

### P2-reply 永远返回 success

- **文件**：`mqtt_service.c` L141-L142

- **问题**：reply 永远 `{"result":1}`，从不反映实际执行结果（channel 越界、GPIO 写失败等）

- **建议**：根据执行结果返回 result=0/1

### P2-心跳 5s QoS1 过密

- **文件**：`mqtt_service.c` L199

- **问题**：心跳 5s 间隔 + QoS 1，上云端场景 30\~60s 足够；QoS 1 会有重复投递，心跳用 QoS 0 更合适

- **建议**：心跳 QoS 0，间隔改 30\~60s

### P3-心跳不含最新 MOS 状态

- **文件**：`mqtt_service.c` L187-L191

- **问题**：心跳只带 uptime，不带 `mos_state` 快照；云端心跳收到后无法知道 MOS 实际状态

- **建议**：心跳里附带 `mos` 位图

### P3-mqtt\_service\_publish\_state() 无人调用

- **文件**：`mqtt_service.c` L221-L239

- **问题**：`mqtt_service_publish_state()` 定义了但没人调，MOS 状态变更不会自动上报

- **建议**：在 `MOS_Control` 成功后异步调用

***

## 五、MOS 模块

### P0-mos\_state 并发无锁

- **文件**：`components/BSP/MOS/mos.c` L45

- **问题**：`static uint8_t mos_state` 被 MQTT 任务和 UART 任务并发读写，`MOS_Control` 里的 read-modify-write 非原子（`mos_state |= (1U << channel)` 等价于 tmp=mos\_state; tmp|=...; mos\_state=tmp，三步）

- **建议**：加 mutex 或临界区保护；或改用任务间 queue 串行化控制指令

### P3-MOS\_Init 未读取外部上电状态

- **文件**：`mos.c` L51-L81

- **问题**：一次性配置 8 个 GPIO 输出并强制全部 OFF，没有读取外部电路实际电平——若外部 MOS 低电平有效，上电可能误触发

- **建议**：初始化后用 `gpio_get_level` 读回确认

### P3-runtime ESP\_ERROR\_CHECK(gpio\_set\_level)

- **文件**：`mos.c` L99

- **问题**：运行期间 GPIO 操作失败（如引脚被复用）会直接 abort

- **建议**：改返回值检查 + 日志

***

## 六、UART 驱动

### P1-逐字节 printf 阻塞接收

- **文件**：`components/BSP/USAR/uart_drv.c` L40-L55

- **问题**：`uart_task` 里每收到一字节就 `printf("%02X ")`，115200 下每字节 printf 占 \~0.2ms，MOS 协议帧最长 13 字节，printf 总耗时 > 2ms，远超过 UART 收发时间，造成 FIFO 溢出风险

- **建议**：删除 printf，发布版用 `ESP_LOGD`（debug 级别）

### P2-UART 任务优先级最高

- **文件**：`uart_drv.c` L117

- **问题**：`uart_task` 优先级 10，高于 WiFi/MQTT（默认 5\~7）——UART 接收是慢路径，不应抢占网络任务

- **建议**：降到 5 或 6

### P3-UART\_FIFO\_OVF 里 xQueueReset

- **文件**：`uart_drv.c` L60-L63

- **问题**：溢出时 `xQueueReset(uart_queue)` 清空队列，会丢失未处理帧；且 UART FIFO 溢出后 `uart_flush_input` 正确，但重置队列过于激进

- **建议**：改用逐条 drain queue 或记录统计后仅做告警

***

## 七、KEY 与 LED

### P2-长按检测用 while 轮询 + delay

- **文件**：`components/BSP/KEY/key.c` L88-L129

- **问题**：长按检测用 `while(gpio_get_level() == 0)` + 10ms `vTaskDelay`，若长按期间 WiFi 连接/MQTT 启动抢占 CPU，2s 检测可能超时不准

- **建议**：改用硬件定时器或 `esp_timer` API

### P3-恢复出厂与长按状态机无隔离

- **文件**：`key.c` L112

- **问题**：长按触发 `wifi_manager_factory_reset()` 后继续轮询按键，若 reset 流程较慢且用户此时松开又迅速按下，会不会被当成新的长按？（概率低但状态机无防护）

- **建议**：reset 后 disable KEY 任务或做状态隔离

### P2-LED 未用作状态指示

- **文件**：`components/BSP/LED/led.c`

- **问题**：只有 `LED(1)` 上电常亮，丢失了宝贵的硬件可观测性

- **建议**：用作状态机指示器：AP 配网 1s 闪烁 / STA 连接 0.5s 慢闪 / MQTT 就绪常亮 / MQTT 断开 2s 闪烁

### P3-GPIO\_OUTPUT\_STATE 枚举语义反直觉

- **文件**：`led.h` L8-L13

- **问题**：`PIN_RESET` / `PIN_SET` 命名不直观，不知道 Reset 是开还是关

- **建议**：改 `LED_OFF` / `LED_ON`

***

## 八、主入口 main.c

### P1-main 循环是测试代码

- **文件**：`main/main.c` L25-L35

- **问题**：每 5s 向 `esp32/test` 发布 `{"online":1}`，是调试残留，应清理

- **建议**：删除或替换为真正业务任务

### P2-nvs\_flash\_init() 返回值被忽略

- **文件**：`main.c` L17

- **问题**：首次擦分区失败或分区表错误时 NVS 读写会静默失败，后续 `wifi_config_load/save` 全部返回错误但无日志提示

- **建议**：检查返回值，失败时打印错误并处理（如 nvs\_flash\_erase）

### P2-mqtt\_manager\_init 无幂等保护

- **文件**：`mqtt_manager.c` L58-L87 + `main.c` L24

- **问题**：WiFi 事件 GOT\_IP 里调 `mqtt_manager_start`，但 main 里又调了 `mqtt_manager_init`（创建 client）；若 init 在 WiFi 连上之前被重复调用，`esp_mqtt_client_init` 内部是否安全？（esp-mqtt 文档说重复 init 行为未定义）

- **建议**：加静态 flag 防重复 init

***

## 九、头文件悬空与死接口

### P2-wifi\_mode.h 声明未实现

- **文件**：`components/BSP/WIFI/WIFI_MODE/wifi_mode.h` L44-L45 + `wifi_mode.c`

- **问题**：声明了 `wifi_mode_get_last_ssid()` / `wifi_mode_get_last_password()` 但从未实现（也没有外部引用）

- **建议**：删除声明或实现

***

## 十、跨模块关注点

### P1-三条控制通道最终写入同一 mos\_state 无保护

- **位置**：MOS 模块 + MQTT 服务 + UART 协议

- **问题**：HTTP（未来可能）、MQTT、UART 三条入口最终都调 `MOS_Control` 修改共享位图，无互斥。UART 设 MOS0=ON、MQTT 同时设 MOS0=OFF——结果取决于哪个先到，双方都认为成功

- **建议**：统一走事件队列串行化；所有控制带 msg\_id，回执返回实际执行位镜像

### P2-错误处理策略不统一

- **问题**：init 期用 `ESP_ERROR_CHECK`（可接受）；但 `wifi_scan_start` 运行期也用 `ESP_ERROR_CHECK(esp_wifi_scan_start(...))`，扫描失败直接 abort；其他运行期用 `ESP_FAIL` 返回

- **建议**：运行期统一返回错误码 + 日志，避免断言式 abort

***

## 硬伤优先级速查表

| 等级     | 项目                                 | 影响                   |
| ------ | ---------------------------------- | -------------------- |
| **P0** | 分区表无 OTA + SPIFFS 浪费 1.5MB         | 无法上云升级、Flash 利用差     |
| **P0** | `/factory_reset` 无鉴权               | 局域网攻击面               |
| **P0** | mos\_state 并发无锁                    | UART + MQTT 同时控制时状态错 |
| **P0** | Broker 配置硬编码                       | 无法量产                 |
| **P1** | WiFi 事件 handler 内阻塞 3s             | 系统事件派发被卡             |
| **P1** | 前端 togglePassword 未实现 + 状态 UI 死元素  | 前端功能缺失               |
| **P1** | 逐字节 printf 阻塞 UART 接收              | 吞吐/实时性               |
| **P1** | main 循环是测试代码                       | 发布不专业                |
| **P2** | LED 未作状态指示                         | 调试黑盒                 |
| **P2** | WiFi 重连策略不一致                       | 断线抖动                 |
| **P2** | SSID/密码 strcpy 无长度检查               | 栈溢出风险                |
| **P2** | 心跳 5s QoS1 过密                      | 上云成本                 |
| **P2** | 未知 cmd 静默丢弃不回复                     | 指令无回执                |
| **P2** | UART 任务优先级过高                       | 资源抢占                 |
| **P2** | NVS init 返回值忽略                     | 静默失败                 |
| **P2** | mqtt\_service\_publish\_state 无人调用 | 状态不自动上报              |
| **P2** | mqtt 重连时重复订阅                       | 语义不清晰                |
| **P2** | 长按检测用 while 轮询                     | 精度不准                 |
| **P3** | 编译 -O3 与全局 -Og 矛盾                  | 发布配置混乱               |
| **P3** | 断言/日志等级过高                          | 发布性能差                |
| **P3** | wifi\_mode.h 悬空声明                  | 接口死代码                |
| **P3** | LED 枚举语义反直觉                        | 可读性                  |
| **P3** | wifi\_list / count 全局死变量           | 冗余                   |
| **P3** | 心跳不含 MOS 状态                        | 云端不完整                |
| **P3** | main 任务栈过小                         | 安全边界                 |

<br />
