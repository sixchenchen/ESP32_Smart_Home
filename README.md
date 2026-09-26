# ESP32

基于 ESP-IDF v6.0.2 的设备固件，实现 WiFi 联网、MQTT 双阶段注册、8 路 mos 管控制、UART 传感器数据采集与 OTA 远程升级。

## 硬件规格

| 项目 | 规格 |
|------|------|
| MCU | ESP32 |
| Flash | 4MB |
| 无线 | WiFi 802.11 b/g/n |
| 输出 | 8 路 mos 管 |
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
│   │   ├── mos/                    # 8 路 mos 管控制
│   │   └── UART/                   # UART 驱动（与传感器通信）
│   ├── protocol/                   # 通信协议层
│   │   ├── mos_protocol/           # mos 控制协议（命令帧解析）
│   │   └── SEN_protocol/           # 传感器协议（帧结构 + CRC + 数据解析）
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
│   │   ├── KEY_MANAGER/            # 按键事件 → LED/mos/重置逻辑
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
┌─ 注册阶段 ─────────────────────────────────────────────────────┐
│ Broker: mqtt://192.168.124.6:1884 （临时注册端口）             │
│                                                                │
│ 设备请求 → /provision/device/{MAC}/register                    │
│   发注册请求，格式:                                            │
│   {                                                            │
│     "device_id":        "xxx",                                 │
│     "product_id":       "xxx",                                 │
│     "hardware_version": "V1.0",                                │
│     "firmware_version": "1.0.0",                               │
│     "action":           "register",                            │
│     "timestamp":        1234567890                             │
│   }                                                            │
│                                                                │
│ 服务器响应 → 设备 ← /provision/device/{MAC}/config/response    │
│   等待配置响应（包含正式 broker 地址/凭据）:                   │
│   {                                                            │
│     "status": "success",                                       │
│     "config": {                                                │
│       "broker_uri": "mqtt://192.168.124.6:1883",               │
│       "client_id":  "B4BFE90CDBA0",                            │
│       "username":   "MQTT1",                                   │
│       "password":   "123456",                                  │
│       "will_topic": "device/B4BFE90CDBA0/will"                 │
│     }                                                          │
│   }                                                            │
│                                                                │
│ 收到后：                                                       │
│   1. 保存新配置到 NVS                                          │
│   2. 断开 broker 1884                                          │
│   3. 连接 broker 1883（正式）                                  │
│   4. 标记 provisioned = true                                   │
└────────────────────────────────────────────────────────────────┘
```

### 3. OTA 远程升级

支持两种触发方式，共用同一套 OTA 引擎：

| 触发方式 | 入口 | 适用场景 |
|----------|------|----------|
| HTTP | `POST http://192.168.124.7/api/ota` | 局域网手动触发 |
| MQTT | 发布到 `device/{mac}/ota` | 云端批量下发 |

**数据格式**

| 触发方式 | 数据格式 |
|----------|----------|
| HTTP | `{"url": "http://192.168.124.6:8000/sample_project.bin", "version": "1.0.30"}` |
| MQTT | `{"url": "http://192.168.124.6:8000/sample_project.bin", "version": "1.0.30"}` |

··· 注意：这里需要开启本地服务器(sample_project.bin所在的文件夹下面使用cmd命令启动服务器) ···
``` 
    cd D:\data\c_code\esp32s\espidf\project\ESP32_Smart_Home\build\
    python -m http.server 8000 --bind 0.0.0.0
```
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

#### MQTT Topic 完整协议（正式阶段 broker 1883）

**下行（服务器 → 设备）**

| Topic | QoS | 触发场景 | JSON 格式 |
|-------|-----|----------|-----------|
| `device/{mac}/control` | 1 | MOS 单路开关 | `{"cmd":"mos","channel":1,"state":1}` |
| `device/{mac}/control` | 1 | MOS 全部开关 | `{"cmd":"mos_all","state":1}` |
| `device/{mac}/control` | 1 | MOS 状态查询 | `{"cmd":"mos_query"}` |
| `device/{mac}/ota` | 1 | OTA 升级触发 | `{"url":"http://192.168.124.6:8000/sample_project.bin","version":"1.0.30"}` |
| `device/{mac}/config` | 1 | 远程配置下发 | `{"config":{"log_level":3,"xxx":"yyy"}}` |
| `/provision/device/{mac}/config/response` | 1 | 注册配置（broker 1884） | - |

**上行（设备 → 服务器）**

| Topic | QoS | 触发场景 | JSON 格式 |
|-------|-----|----------|-----------|
| `device/{mac}/status` | 1 | MQTT 连接成功上线 | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"state","data":{"state":"online"}}` |
| `device/{mac}/status` | 1 | 主动离线（恢复出厂等） | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"state","data":{"state":"offline","reason":"factory_reset"}}` |
| `device/{mac}/will` | 1 | 异常断开（LWT，broker 自动发） | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"offline","data":{"reason":"mqtt_lwt"}}` |
| `device/{mac}/heart` | 0 | 定时心跳（每 30 秒） | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"heartbeat","data":{"uptime":1234}}` |
| `device/{mac}/mos_state` | 1 | 上线后 / 查询后 / MOS 变化后 | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"state","data":{"mos0":0,"mos1":1,"mos2":0,"mos3":0,"mos4":0,"mos5":0,"mos6":0,"mos7":0}}` |
| `device/{mac}/event` | 1 | MOS 开关事件 | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"event","data":{"event":"mos_change","channel":1,"state":1,"success":true}}` |
| `device/{mac}/event` | 1 | 错误响应（JSON 解析失败/OTA 缺字段等） | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"error","data":{"code":2003,"message":"ota missing url field"}}` |
| `device/{mac}/event` | 1 | 恢复出厂事件 | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"event","data":{"event":"factory_reset"}}` |
| `device/{mac}/state` | 0 | OTA 启动通知 | `{"type":"ota","state":"started"}` |
| `device/{mac}/state` | 0 | OTA 启动失败 | `{"type":"ota","state":"fail","code":2}` |
| `device/{mac}/sensor` | 0 | 传感器批量数据（满/超时 flush） | `{"device":"B4BFE90CDBA0","product":"SmartHome-v1","type":"sensor_batch","timestamp":12345,"data":[{"sensor_id":1,"timestamp":12340,"count":100}]}` |
| `/provision/device/{mac}/register` | 1 | 注册请求（broker 1884） | - |

### 4. mos 管控制

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
║  1. mos 单路控制                           ║
║  2. mos 全部开                              ║
║  3. mos 全部关                              ║
║  4. mos 状态查询                            ║
║  5. OTA 升级触发                            ║
║  6. 监听所有 topic (15s)                    ║
║  7. 模拟注册服务器 (broker 1884)            ║
║  0. 退出                                    ║
╚══════════════════════════════════════════╝
```

### 命令行直执模式

```bash
# mos 单路控制：通道 1 开
python mqtt_test.py --mos 1 1

# mos 全部开 / 关
python mqtt_test.py --mos-all 1
python mqtt_test.py --mos-all 0

# 查询 mos 状态
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

#### mos 控制

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

#### 设备主动上报（MQTTX 监听示例）

在 MQTTX 里订阅 `device/B4BFE90CDBA0/#` 可一次性看到所有消息。

| Topic | 字段结构 | 说明 |
|-------|----------|------|
| `device/{id}/status` | `{"device":"...","product":"...","type":"state","data":{"state":"online"}}` | 上线消息（retain=true，重连也能看到） |
| `device/{id}/will` | `{"device":"...","product":"...","type":"offline","data":{"reason":"mqtt_lwt"}}` | LWT 遗嘱（retain=true，broker 自动发布） |
| `device/{id}/mos_state` | `{"device":"...","product":"...","type":"state","data":{"mos0":0,"mos1":1,...,"mos7":0}}` | 8 路 MOS 状态位图 |
| `device/{id}/event` | `{"device":"...","product":"...","type":"event","data":{"event":"mos_change","channel":1,"state":1,"success":true}}` | MOS 变化事件 |
| `device/{id}/event` | `{"device":"...","product":"...","type":"error","data":{"code":2003,"message":"ota missing url field"}}` | 错误响应 |
| `device/{id}/state` | `{"type":"ota","state":"started"}` | OTA 启动通知 |
| `device/{id}/state` | `{"type":"ota","state":"fail","code":2}` | OTA 启动失败（code 见 ota.h） |
| `device/{id}/heart` | `{"device":"...","product":"...","type":"heartbeat","data":{"uptime":1234}}` | 心跳（每 30 秒，QoS 0） |
| `device/{id}/sensor` | `{"device":"...","product":"...","type":"sensor_batch","timestamp":12345,"data":[{"sensor_id":1,"timestamp":12340,"count":100}]}` | 传感器批量数据 |

#### 注册流程（broker 1884）

```
┌─ Step 1: 设备（broker 1884）─────────────────────────┐
│ Topic : /provision/device/B4BFE90CDBA0/register       │
│ QoS   : 1                                             │
│ Payload: {                                            │
│   "device_id": "B4BFE90CDBA0",                        │
│   "product_id": "SmartHome-v1",                       │
│   "hardware_version": "V1.0",                         │
│   "firmware_version": "1.0.29",                       │
│   "action": "register",                               │
│   "timestamp": 1234567890                             │
│ }                                                     │
└──────────────────────────────────────────────────────┘
                        ↓
┌─ Step 2: 服务器回复（broker 1884）───────────────────┐
│ Topic : /provision/device/B4BFE90CDBA0/config/response│
│ QoS   : 1                                             │
│ Payload: {                                            │
│   "status": "success",                                │
│   "config": {                                         │
│     "broker_uri": "mqtt://192.168.124.6:1883",        │
│     "client_id": "B4BFE90CDBA0",                      │
│     "username": "MQTT1",                               │
│     "password": "123456",                              │
│     "will_topic": "device/B4BFE90CDBA0/will"          │
│   }                                                   │
│ }                                                     │
└──────────────────────────────────────────────────────┘
                        ↓
┌─ Step 3: 设备内部动作 ───────────────────────────────┐
│ 1. 保存 broker_uri / client_id / username / password  │
│    / will_topic 到 NVS（mqtt_config namespace）       │
│ 2. 断开 broker 1884 的 MQTT 连接                      │
│ 3. 销毁旧 MQTT 客户端                                 │
│ 4. 重新 init → start，连接 broker 1883                 │
│ 5. MQTT_EVENT_CONNECTED → 订阅 control + ota + config │
│ 6. publish status=online + mos_state + 启动心跳       │
└──────────────────────────────────────────────────────┘
```

**注意：** `client_id` 和 `will_topic` 服务器可省略，设备会自动用 device_id 和默认值填充。`status != "success"` 时设备会重试 10 次，每次等待 30 秒。

#### 错误码速查（上行 event topic）

| code | 来源模块 | 含义 |
|------|----------|------|
| 1001 | mqtt_service | control 消息过长 / JSON 解析失败 |
| 1002 | mqtt_service | control 缺少 `cmd` 字段 |
| 1003 | mqtt_service | 未知 control 命令 |
| 1004 | mqtt_service | MOS 命令缺 `channel` 或 `state` |
| 1005 | mqtt_service | MOS channel/state 非数字 |
| 1006 | mqtt_service | MOS channel 越界（≥8） |
| 1007 | mqtt_service | MOS 控制失败 / mos_all 缺 state |
| 2001 | mqtt_service | OTA 消息长度异常 |
| 2002 | mqtt_service | OTA JSON 解析失败 |
| 2003 | mqtt_service | OTA 缺少 `url` 字段 |
| 2004 | mqtt_service | OTA url 提取失败 |
| 3001 | mqtt_service | config 消息过长 |
| 3002 | mqtt_service | config JSON 解析失败 |
| 3003 | mqtt_service | config 缺 `config` 对象 |
| 1xxx | ota.c | OTA_RESULT_* 错误码（见 ota.h） |

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
