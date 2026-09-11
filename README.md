# ESP32

基于 ESP-IDF v6.0.2 的设备固件，实现 WiFi 联网、MQTT 双阶段注册、8 路 MOS 管控制、UART 传感器数据采集与 OTA 远程升级。

## 硬件规格

| 项目 | 规格 |
|------|------|
| MCU | ESP32 |
| Flash | 4MB |
| 无线 | WiFi 802.11 b/g/n |
| 输出 | 8 路 MOS 管 |
| 接口 | UART（传感器）、GPIO（按键、LED） |

## 开发环境

| 工具 | 版本 |
|------|------|
| ESP-IDF | v6.0.2 |
| 操作系统 | Windows / Linux |
| Python | 3.x（用于 HTTP 文件服务器） |

### 编译与烧录

```bash
# ESP-IDF PowerShell / cmd
idf.py build
idf.py -p COMx flash monitor
```

---

## 目录结构

```
ESP32_Smart_Home/
├── main/                           # 应用入口
│   └── main.c                      # app_main() 初始化所有模块
├── components/
│   ├── BSP/                        # 板级支持包（硬件抽象层）
│   │   ├── Device/                 # 设备上下文（MAC 地址、唯一标识）
│   │   ├── KEY/                    # 按键驱动（短按/长按检测）
│   │   ├── LED/                    # LED 状态指示驱动
│   │   ├── MOS/                    # 8 路 MOS 管控制
│   │   └── UART/                   # UART 驱动（与传感器通信）
│   ├── Protocol/                   # 通信协议层
│   │   ├── MOS_Protocol/           # MOS 控制协议（命令帧解析）
│   │   └── SEN_Protocol/           # 传感器协议（帧结构 + CRC + 数据解析）
│   ├── Middlewares/                # 中间件层
│   │   ├── HTTP_SERVER/            # HTTP 服务器
│   │   │   ├── http_server.c       # Web 配置页面 + OTA API
│   │   │   ├── index.html          # WiFi 配网 SPA 页面
│   │   │   ├── app.js              # 前端逻辑
│   │   │   └── style.css           # 样式
│   │   ├── MQTT/                   # MQTT 完整方案
│   │   │   ├── mqtt_manager.c      # 客户端生命周期管理（启动/停止/重连）
│   │   │   ├── mqtt_config.c       # 配置加载（NVS）+ 默认值
│   │   │   ├── mqtt_topic.c        # 所有 Topic 统一管理 + 宏定义
│   │   │   ├── mqtt_message.c      # JSON 消息构建（上行/下行）
│   │   │   ├── mqtt_provision.c    # 设备注册状态机（1884 端口）
│   │   │   └── mqtt_service.c      # 业务消息路由（控制/OTA/传感器）
│   │   ├── OTA/                    # OTA 升级引擎
│   │   │   └── ota.c               # 状态机 + HTTP 下载 + 分区切换 + 回滚
│   │   └── WIFI/                   # WiFi 管理
│   │       ├── WIFI_MANAGER/        # 连接/断开状态机 + IP 事件处理
│   │       ├── WIFI_MODE/          # STA / AP 模式切换
│   │       ├── WIFI_SCAN/          # WiFi 扫描
│   │       └── WIFI_CONFIG/        # WiFi 凭据持久化（NVS）
│   ├── Application/                # 应用层（业务逻辑）
│   │   ├── KEY_MANAGER/            # 按键事件 → LED/MOS/重置逻辑
│   │   ├── LED_STATUS/             # LED 状态机（连网/注册/OTA 状态指示）
│   │   ├── SENSOR_MANAGER/         # 传感器数据采集 + 环形缓冲
│   │   └── SENSOR_MQTT_BRIDGE/     # 传感器数据 → MQTT 批量上报
│   └── Common/                     # 通用工具
│       ├── JSON/                   # JSON 构建辅助
│       └── NVS/                    # NVS 封装（读写 WiFi/MQTT 配置）
├── partitions.csv                  # 自定义分区表
├── sdkconfig                       # ESP-IDF 配置
├── CMakeLists.txt
└── ota.bat                         # 一键 OTA 升级脚本（Windows）
```

---

## 分区表

```
# Name      Type    SubType   Offset     Size       Flags
nvs         data    nvs       0x9000     0x6000
otadata     data    ota       0xF000     0x2000
phy_init    data    phy       0x11000    0x1000
factory     app     factory   0x20000    0x120000   # 1.125MB
ota_0       app     ota_0     0x140000   0x120000   # 1.125MB
ota_1       app     ota_1     0x260000   0x120000   # 1.125MB
spiffs      data    spiffs    0x380000   0x80000    # 512KB
```

双 OTA 分区轮换，Bootloader 支持自动回滚（`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`）。

---

## 核心功能

### 1. WiFi 联网

- **STA 模式**：连接家庭路由器，IP 动态获取
- **AP 模式**：长按按键进入配网，浏览器访问 `http://192.168.4.1` 选择 WiFi
- **自动切换**：WiFi 凭据保存到 NVS，重启自动重连
- **事件驱动**：WiFi Manager 监听 `WIFI_EVENT_STA_CONNECTED` 和 `IP_EVENT_STA_GOT_IP`

### 2. MQTT 双阶段注册

设备首次启动时 `provisioned=0`，进入**注册阶段**：

```
┌─ 注册阶段 ─────────────────────────────────────────┐
│ Broker: mqtt://<host>:1884 （临时注册端口）          │
│                                                      │
│ 设备 → /provision/device/{MAC}/register              │
│        发注册请求                                     │
│                                                      │
│ 设备 ← /provision/device/{MAC}/config/response       │
│        等待配置响应（包含正式 broker 地址/凭据）       │
│                                                      │
│ 收到后：                                             │
│  1. 保存新配置到 NVS                                  │
│  2. 断开 broker 1884                                  │
│  3. 连接 broker 1883（正式）                          │
│  4. 标记 provisioned = true                           │
└──────────────────────────────────────────────────────┘
```

### 3. OTA 远程升级

支持两种触发方式，共用同一套 OTA 引擎：

| 触发方式 | 入口 | 适用场景 |
|----------|------|----------|
| **HTTP** | `POST http://<device-ip>/api/ota` | 局域网手动触发 |
| **MQTT** | 发布到 `device/{mac}/ota` | 云端批量下发 |

#### OTA 状态机

```
IDLE → CHECKING → DOWNLOADING → VERIFYING → [SUCCEEDED | FAILED]
                                              ↓
                                        重启切换分区
```

#### HTTP API

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/` | Web 配网页面 |
| GET | `/scan` | WiFi 扫描结果 |
| GET | `/wifi_status` | 当前 WiFi 状态 |
| POST | `/wifi_config` | 保存 WiFi 凭据 |
| POST | `/factory_reset` | 恢复出厂设置 |
| GET | `/ota_status` | 查询 OTA 状态 |
| POST | `/api/ota` | 触发 OTA 升级 |

#### MQTT Topic（正式阶段 broker 1883）

| 方向 | Topic | 说明 |
|------|-------|------|
| 设备订阅 | `device/{mac}/control` | MOS 控制命令 |
| 设备订阅 | `device/{mac}/ota` | OTA 升级命令 |
| 设备订阅 | `/provision/device/{mac}/config/response` | 配置更新 |
| 设备发布 | `device/{mac}/event` | 事件上报（状态/错误） |
| 设备发布 | `device/{mac}/heartbeat` | 心跳 |
| 设备发布 | `device/{mac}/sensor` | 传感器数据 |
| 设备发布 | `/provision/device/{mac}/register` | 注册请求 |

### 4. MOS 管控制

- 8 路独立控制，支持单路开关和全部开关
- MQTT 命令格式：`{"channel":1,"state":1}`
- HTTP 也提供控制接口

### 5. 传感器数据采集

- UART 与 GD32 传感器板通信
- 自定义帧格式（帧头 + 长度 + CRC + 数据）
- 批量上报：数据缓存满或超时（10 秒）自动 flush

### 6. LED 状态指示

| LED 状态 | 含义 |
|----------|------|
| 快闪 | WiFi 未连接 |
| 慢闪 | MQTT 连接中 |
| 常亮 | 正常运行 |
| 双闪 | OTA 升级中 |

### 7. 按键功能

| 操作 | 功能 |
|------|------|
| 短按 | 切换 LED |
| 长按 3s | WiFi 配网模式 |
| 长按 10s | 恢复出厂设置 |

---

## OTA 升级流程

### 方式一：一键脚本（推荐）

```bash
# Windows PowerShell / cmd
ota.bat 1.0.30
```

脚本自动完成：
1. 修改 `CMakeLists.txt` 版本号
2. 编译固件
3. 启动 Python HTTP 文件服务器
4. 向设备发送 OTA 触发请求

### 方式二：手动 HTTP

```bash
# 1. 在电脑上启动 HTTP 服务器（提供 bin 文件下载）
cd build
python -m http.server 8000 --bind 0.0.0.0

# 2. 向设备发送 OTA 请求
curl -X POST http://<device-ip>/api/ota \
  -H "Content-Type: application/json" \
  -d '{"url":"http://<your-pc-ip>:8000/sample_project.bin","version":"1.0.30"}'

# 3. 查看 OTA 状态
curl http://<device-ip>/ota_status
```

### 方式三：MQTT 触发

MQTTX 连接 **broker 1883**，发布：
```
Topic:   device/B4BFE90CDBA0/ota
QoS:     1
Payload: {"url":"http://<your-pc-ip>:8000/sample_project.bin","version":"1.0.30"}
```

### 流程示意

```
                    电脑（固件服务器）
                    python -m http.server 8000
                          │
                          │ HTTP GET /sample_project.bin
                          ▼
设备 ──触发OTA──→ 校验版本 ──下载固件──→ 校验签名 ──写入新分区 ──重启
     POST /api/ota                                                │
     或 MQTT                                                        ▼
                                                          Bootloader
                                                     校验新分区有效
                                                     ↓ 标记 boot
                                                     从 ota_1 启动
```

---

## MQTT 功能测试

项目提供 `mqtt_test.py` 一站式测试脚本，覆盖所有 MQTT 场景。

### 安装依赖

```bash
pip install paho-mqtt
```

### 交互菜单模式

```bash
python mqtt_test.py
```

会出现菜单：

```
╔══════════════════════════════════════════╗
║  ESP32 Smart Home - MQTT 测试菜单          ║
║  设备: B4BFE90CDBA0                        ║
║  Broker: 192.168.124.6:1883                ║
╠══════════════════════════════════════════╣
║  1. MOS 单路控制                           ║
║  2. MOS 全部开                              ║
║  3. MOS 全部关                              ║
║  4. MOS 状态查询                            ║
║  5. OTA 升级触发                            ║
║  6. 监听所有 topic (15s)                    ║
║  7. 模拟注册服务器 (broker 1884)            ║
║  0. 退出                                    ║
╚══════════════════════════════════════════╝
```

### 命令行直执模式

```bash
# MOS 单路控制：通道 1 开
python mqtt_test.py --mos 1 1

# MOS 全部开 / 关
python mqtt_test.py --mos-all 1
python mqtt_test.py --mos-all 0

# 查询 MOS 状态
python mqtt_test.py --mos-query

# OTA 升级
python mqtt_test.py --ota 1.0.30 http://192.168.124.6:8000/sample_project.bin

# 监听所有 topic（默认 15 秒）
python mqtt_test.py --listen
python mqtt_test.py --listen 30

# 模拟注册响应（broker 1884，等设备发 register 后自动回复 config）
python mqtt_test.py --provision

# 指定不同设备 / broker
python mqtt_test.py --dev A1B2C3D4E5F6 --host 10.0.0.5 --port 1883
```

### 手动测试（MQTTX / 其他工具）

#### MOS 控制

```
# 单路开
Topic:   device/B4BFE90CDBA0/control
Payload: {"cmd":"mos","channel":1,"state":1}

# 单路关
Payload: {"cmd":"mos","channel":1,"state":0}

# 全部开
Payload: {"cmd":"mos_all","state":1}

# 查询状态
Payload: {"cmd":"mos_query"}
```

#### OTA 升级

```
Topic:   device/B4BFE90CDBA0/ota
Payload: {"url":"http://192.168.124.6:8000/sample_project.bin","version":"1.0.30"}
```

#### 设备主动上报（需监听）

| Topic | 触发时机 | 内容示例 |
|-------|----------|----------|
| `device/{id}/status` | MQTT 连接成功 / LWT | `{"type":"state","data":{"state":"online"}}` |
| `device/{id}/event` | MOS 变化 / 错误 | `{"type":"event","data":{"event":"mos_change","channel":1,"state":1}}` |
| `device/{id}/event` | 错误响应 | `{"type":"error","data":{"code":2003,"message":"ota missing url field"}}` |
| `device/{id}/mos_state` | 查询 / 变化后 | `{"type":"state","data":{"mos0":1,"mos1":0,...}}` |
| `device/{id}/heart` | 每 30 秒 | `{"type":"heartbeat","data":{"uptime":1234}}` |
| `device/{id}/sensor` | 传感器批量上报 | `{"type":"sensor_batch","data":[...]}` |
| `device/{id}/will` | LWT 遗嘱（异常断开） | `{"type":"offline","data":{"reason":"mqtt_lwt"}}` |

#### 注册流程（broker 1884）

```
# 设备 → 服务器（注册请求）
Topic:  /provision/device/B4BFE90CDBA0/register
Payload: {"device_id":"B4BFE90CDBA0","firmware_version":"1.0.29","action":"register",...}

# 服务器 → 设备（配置响应）
Topic:  /provision/device/B4BFE90CDBA0/config/response
Payload: {"status":"success","config":{"broker_uri":"mqtt://192.168.124.6:1883","username":"MQTT1","password":"123456",...}}
```

---

## 开发备注

### 代码风格

- TAG 统一使用模块名全小写下划线格式：`mqtt_manager`、`ota`、`wifi_manager`
- 每个模块的常量集中在文件顶部用 `#define` 声明
- Include 顺序：本模块头 → 项目头 → IDF 头 → 系统头
- 函数签名 Allman 风格（左花括号另起一行）

### 线程安全

- MQTT：`MQTT_EVENT_CONNECTED` / `MQTT_EVENT_DISCONNECTED` 在 MQTT 任务上下文执行
- OTA：状态机通过 `xSemaphoreTake(s_ota_mutex, ...)` 互斥保护
- WiFi：事件回调在 `esp_netif` 任务上下文，实际逻辑 queue 到主任务

### 已知限制（待改进）

- OTA 使用 HTTP 明文下载（无 TLS），证书校验已跳过（`.cert_pem = NULL`）
- MQTT 默认凭据硬编码在 `mqtt_config.c` 中（测试方便，正式部署需改为加密存储）
- 暂无 SNTP 时间同步，传感器 timestamp 为设备 uptime
