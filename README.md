````
# 物联网协议

基于 ESP-IDF v6.0.2 的 ESP32 设备固件，支持 WiFi 联网、MQTT 双阶段注册、8 路 MOS 管控制、UART 传感器数据采集以及 OTA 远程升级。


## 文档信息

| 项目 | 内容 |
| :--- | :--- |
| 协议版本 | v4.0 |
| 固件平台 | ESP32 |
| ESP-IDF | v6.0.2 |
| 运行 Broker | 1883 |
| 注册 Broker | 1884 |
| 下行入口 | `device/{mac}/command`、`$broadcast/command` |
| 上行 Topic | `online` / `offline` / `will` / `heartbeat` / `state` / `ack` / `event` / `sensor` |

> **设计原则**
>
> - 下行控制统一进入 `command`。
> - 上行按照语义拆分 Topic。
> - 所有需要关联指令的单播命令使用 `commandId`。
> - 广播命令由设备本地生成 `commandId`。
> - 设备状态使用 `state` 的 `full + targets` 模型，支持全量和增量上报。
> - OTA 触发使用 `command`，升级过程和最终结果使用 `state` 上报。


---

## 一、硬件规格

| 项目 | 规格 |
|------|------|
| MCU | ESP32 |
| Flash | 4MB |
| 无线 | WiFi 802.11 b/g/n |
| 输出 | 8 路 MOS 管 |
| 接口 | UART（传感器）、GPIO（按键、LED） |

---

## 二、开发环境

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
````


---

## 三、目录结构


```
ESP32_Smart_Home/
├── main/                           # 应用入口
│   └── main.c                      # app_main() 初始化所有模块
├── components/
│   ├── BSP/                        # 板级支持包（硬件抽象层）
│   │   ├── Device/                 # 设备上下文（MAC 地址、唯一标识）
│   │   ├── KEY/                    # 按键驱动（短按/长按检测）
│   │   ├── LED/                    # LED 状态指示驱动
│   │   ├── mos/                    # 8 路 MOS 管控制
│   │   └── UART/                   # UART 驱动（与传感器通信）
│   ├── protocol/                   # 通信协议层
│   │   ├── mos_protocol/           # MOS 控制协议（命令帧解析）
│   │   └── SEN_protocol/           # 传感器协议（帧结构 + CRC + 数据解析）
│   ├── Middlewares/                # 中间件层
│   │   ├── HTTP_SERVER/            # HTTP 服务器
│   │   │   ├── http_server.c       # Web 配置页面 + OTA API
│   │   │   ├── index.html          # WiFi 配网 SPA 页面
│   │   │   ├── app.js              # 前端逻辑
│   │   │   └── style.css           # 样式
│   │   ├── MQTT/                   # MQTT 完整方案
│   │   │   ├── mqtt_manager.c      # 客户端生命周期管理
│   │   │   ├── mqtt_config.c       # 配置加载（NVS）+ 默认值
│   │   │   ├── mqtt_topic.c        # 所有 Topic 统一管理 + 宏定义
│   │   │   ├── mqtt_message.c      # JSON 消息构建（上行/下行）
│   │   │   ├── mqtt_provision.c    # 设备注册状态机（1884 端口）
│   │   │   └── mqtt_service.c      # 业务消息路由
│   │   ├── OTA/                    # OTA 升级引擎
│   │   │   └── ota.c               # 状态机 + HTTP 下载 + 分区切换 + 回滚
│   │   └── WIFI/                   # WiFi 管理
│   │       ├── WIFI_MANAGER/       # 连接/断开状态机
│   │       ├── WIFI_MODE/          # STA / AP 模式切换
│   │       ├── WIFI_SCAN/          # WiFi 扫描
│   │       └── WIFI_CONFIG/        # WiFi 凭据持久化（NVS）
│   ├── Application/                # 应用层（业务逻辑）
│   │   ├── KEY_MANAGER/            # 按键事件 → LED/MOS/重置逻辑
│   │   ├── LED_STATUS/             # LED 状态机
│   │   ├── SENSOR_MANAGER/         # 传感器数据采集 + 环形缓冲
│   │   └── SENSOR_MQTT_BRIDGE/     # 传感器数据 → MQTT 批量上报
│   └── Common/                     # 通用工具
│       ├── JSON/                   # JSON 构建辅助
│       └── NVS/                    # NVS 封装
├── partitions.csv                  # 自定义分区表
├── sdkconfig                       # ESP-IDF 配置
├── CMakeLists.txt
└── ota.bat                         # 一键 OTA 升级脚本（Windows）
```


---

## 四、分区表


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

## 五、核心功能

### 5.1 WiFi 联网

| **模式** | **说明** |
| :------ | :------------------------------------------ |
| **STA** | 连接家庭路由器，IP 动态获取                             |
| **AP**  | 长按按键进入配网，浏览器访问 `http://192.168.4.1` 选择 WiFi |

- **自动切换**：WiFi 凭据保存到 NVS，重启自动重连
- **事件驱动**：监听 `WIFI_EVENT_STA_CONNECTED` 和 `IP_EVENT_STA_GOT_IP`

### 5.2 MQTT 双阶段注册

设备首次启动时 `provisioned=0`，进入**注册阶段**,本地没有任何 MQTT 凭据，需要先向服务器"报到"，换取正式 Broker 的凭据：


| 项 | 说明 |
|---|---|
| 触发时机 | 首次上电 / 恢复出厂 / MQTT 凭据失效 |
| Broker | 1884（注册专用，与运行时 1883 隔离） |
| Topic | `/provision/device/{mac}/register`（设备→服务器）、`/provision/device/{mac}/config`（服务器→设备） |
| QoS | 1 |
| 安全 | 非对称加密（ECDSA）+ nonce 防重放 + timestamp 时间窗口 |

### 5.3 安全设计

注册流程采用**四重安全机制**，防止伪造和重放：

| 机制 | 作用 | 位置 |
|---|---|---|
| **pubkey（公钥）** | 设备身份标识，服务器用它验签 | 设备首启生成，随请求发送 |
| **signature（签名）** | 证明"pubkey 属于这台设备" | 设备用私钥签名，随请求发送 |
| **nonce（随机数）** | 防重放，每次注册唯一 | 设备每次生成 UUID，服务器 Redis 记录 5 分钟 |
| **timestamp（时间戳）** | 防过期请求 | 服务器校验 ±5 分钟窗口 |

**四者的关系**：

```
┌──────────────────────────────────────────────────┐
│ 设备首次启动                                       │
│   1. 生成 ECDSA P-256 密钥对                       │
│      ├─ 私钥 → NVS/eFuse（永不出设备）             │
│      └─ 公钥 → 待用                                │
└──────────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────────┐
│ 每次注册                                           │
│   1. 生成 nonce（随机 UUID）                       │
│   2. 拼待签数据：                                  │
│      signData = {device}|{timestamp}|{nonce}      │
│   3. 用私钥签名：                                  │
│      signature = ECDSA_Sign(私钥, signData)       │
│   4. 发请求：{..., nonce, pubkey, signature}      │
└──────────────────────────────────────────────────┘
                    ↓
┌──────────────────────────────────────────────────┐
│ 服务器验证                                         │
│   1. 校验 device 格式（12 位大写十六进制）         │
│   2. 校验 timestamp（±5 分钟）                     │
│   3. 校验 nonce（Redis，防重放）                   │
│   4. 重拼 signData，用 pubkey 验 signature         │
│   5. 校验 pubkey 与数据库记录一致（防冒充）         │
│   6. 通过 → 分配 MQTT 凭据                         │
└──────────────────────────────────────────────────┘
```

**关键认识**：

- **私钥不出设备** → 身份不可伪造
- **nonce 唯一** → 请求不可重放
- **pubkey 绑定数据库** → 已注册设备不可被覆盖

### 1.3 注册请求

**Topic**：`/provision/device/{mac}/register`

**Payload**：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "register",
  "timestamp": 1791444733387,
  "data": {
    "firmware": "1.0.29",
    "chip": "ESP32",
    "hardware_version": "V1.0",
    "nonce": "900593f8-3137-4739-a3b5-67ebb74c6071",
    "pubkey": "-----BEGIN PUBLIC KEY-----\nMFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEuZS7IU8D6F+ImzIQlx9vfwEuUn63\nTaPRSMIQE7OTVT22F2CKjF7HGJkV7F1svV6CWoWHW/dKbJ3VVWpcW0e6gA==\n-----END PUBLIC KEY-----\n",
    "signature": "MEUCIQDeiopSexr8PL+speElv24ePAPGmryds9AK+rhg0DkbkwIgSEmrJygUA3I84v4ENj65+NuvhAXYtwXoKknL2HcbxVA="
  }
}
```

**字段说明**：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `device` | string | 是 | 设备 MAC（12 位大写十六进制，无冒号） |
| `product` | string | 是 | 产品型号 |
| `type` | string | 是 | 固定 `"register"` |
| `timestamp` | long | 是 | 毫秒时间戳（服务器校验 ±5 分钟） |
| `data.firmware` | string | 是 | 固件版本 |
| `data.chip` | string | 否 | 芯片型号（ESP32） |
| `data.hardware_version` | string | 否 | 硬件版本 |
| `data.nonce` | string | 是 | 随机 UUID，每次注册唯一 |
| `data.pubkey` | string | 是 | PEM 格式公钥（ECDSA P-256） |
| `data.signature` | string | 是 | Base64 编码的签名 |

**三个关键字段的生成方式**：

| 字段 | 谁生成 | 何时生成 | 用什么 |
|---|---|---|---|
| `nonce` | 设备 | 每次注册 | `esp_random()` 生成 UUID |
| `pubkey` | 设备 | **首次启动** | mbedTLS 生成 P-256 密钥对 |
| `signature` | 设备 | 每次注册 | mbedTLS 用私钥签名 `{device}\|{timestamp}\|{nonce}` |

### 1.4 注册响应

**Topic**：`/provision/device/{mac}/config`

**成功响应**：

```json
{
  "device": "B4BFE90CDBA1",
  "success": true,
  "mqtt": {
    "host": "192.168.6.6",
    "port": 1883,
    "clientId": "device-B4BFE90CDBA1",
    "username": "dev_B4BFE90CDBA1",
    "password": "e2e1c28ef5d9489b",
    "keepAlive": 60
  },
  "will": {
    "topic": "device/B4BFE90CDBA1/will",
    "qos": 1,
    "retain": true,
    "payload": {
      "device": "B4BFE90CDBA1",
      "product": "SmartHome-v1",
      "type": "offline",
      "timestamp": 0,
      "data": { "reason": "mqtt_lwt" }
    }
  },
  "config": {
    "heartbeat_interval": 30,
    "sensor_batch_size": 32
  },
  "timestamp": 1791444746640
}
```

**失败响应**：

```json
{
  "device": "B4BFE90CDBA1",
  "success": false,
  "error": "INVALID_SIGNATURE",
  "message": "签名验证失败",
  "timestamp": 1791444746640
}
```

**响应字段说明**：

| 字段 | 说明 |
|---|---|
| `mqtt.host` / `port` | **运行时 Broker** 地址（1883） |
| `mqtt.clientId` | 设备客户端 ID（`device-{mac}`） |
| `mqtt.username` | 用户名（`dev_{mac}`） |
| `mqtt.password` | 密码（16 位随机字符串） |
| `mqtt.keepAlive` | 保活间隔（秒） |
| `will.topic` / `qos` / `retain` | LWT 遗嘱配置 |
| `will.payload` | 遗嘱内容（Broker 在设备异常断开时自动发布） |
| `config.heartbeat_interval` | 心跳间隔（秒） |
| `config.sensor_batch_size` | 传感器批量上报阈值 |

### 1.5 设备切换流程

设备收到成功响应后：

```
1. 保存 MQTT 凭据到 NVS
   ├─ broker 地址：192.168.6.6:1883
   ├─ clientId / username / password
   ├─ will 遗嘱配置
   └─ config 配置

2. 断开 Broker 1884

3. 用新凭据连接 Broker 1883

4. 连接成功 → 发 device/{mac}/online

5. 标记 provisioned = true
```

**注意**：

- **注册成功设备不需要回 ACK**（注册是"请求-响应"模式）
- `success != true` 时设备重试 10 次，每次等待 30 秒
- 连续失败进入错误状态（LED 快闪提示）

### 1.6 重复注册策略

同一台设备可能因重启、网络抖动、固件升级等原因**多次注册**。

**处理策略**：

| 场景 | pubkey 是否一致 | 服务器行为 |
|---|---|---|
| 首次注册 | — | 保存 pubkey，分配新凭据 |
| 同设备重启 | 一致 | 允许，换新密码（旧密码失效） |
| 网络抖动重试 | 一致 | 允许，nonce 不同即可 |
| **攻击者冒充** | **不一致** | **拒绝（DEVICE_ALREADY_REGISTERED）** |
| 设备换密钥对 | 不一致 | 拒绝（需管理员介入清空 pubkey） |


**没有 pubkey 校验的后果**：

攻击者用自己的密钥对注册同一 MAC → 验签通过（用攻击者自己的 pubkey）→ 覆盖数据库 → 拿到 MQTT 凭据 → **冒充成功**。

**加了 pubkey 校验**：数据库里已有 pubkey 且与请求不一致 → 直接拒绝 → **无法冒充**。

### 1.8 错误码

| 错误码 | 含义 | 设备处理 |
|---|---|---|
| `INVALID_DEVICE_ID` | MAC 格式不对 | 检查固件 |
| `PARAM_MISSING` | product 为空 | 检查固件 |
| `INVALID_TIMESTAMP` | 时间戳超窗口 | **同步 NTP 后重试** |
| `REPLAY_ATTACK` | nonce 重复 | **换新 nonce 重试** |
| `INVALID_SIGNATURE` | 签名验证失败 | 检查密钥对 |
| `DEVICE_ALREADY_REGISTERED` | pubkey 不匹配（被冒充） | 停止重试，人工介入 |
| `SERVER_ERROR` | 服务器异常 | 等 30 秒重试 |

### 1.9 完整时序图

```
┌─────────────┐                              ┌─────────────┐
│   ESP32 设备  │                              │   服务器     │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │ ① 首次启动，检查本地 provisioned              │
       │    未注册 → 生成 ECDSA 密钥对                 │
       │    私钥存 NVS，公钥待用                       │
       │                                            │
       │ ② 连接 Broker 1884                          │
       │ ──────────────────────────────────────────►│
       │                                            │
       │ ③ 生成 nonce，拼 signData，用私钥签名          │
       │    发布注册请求                              │
       │    topic: /provision/device/{mac}/register │
       │ ──────────────────────────────────────────►│
       │                                            │
       │                                      ④ 服务器处理：
       │                                        a. 校验 MAC 格式
       │                                        b. 校验 timestamp
       │                                        c. 校验 nonce（Redis）
       │                                        d. 用 pubkey 验签名
       │                                        e. 校验 pubkey 一致性 ★
       │                                        f. 保存设备 + pubkey
       │                                        g. 生成 MQTT 凭据
       │                                            │
       │ ⑤ 收到响应                                   │
       │    topic: /provision/device/{mac}/config   │
       │    {success, mqtt:{...}, will:{...},        │
       │     config:{...}}                           │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ ⑥ 保存凭据到 NVS                             │
       │    断开 1884                                │
       │                                            │
       │ ⑦ 连接运行时 Broker 1883                     │
       │ ──────────────────────────────────────────►│
       │                                            │
       │ ⑧ 发 online，开始心跳                         │
       │ ──────────────────────────────────────────►│
       │                                            │
```

---

## 二、OTA 远程升级

### 2.1 业务概述

OTA（Over-The-Air）通过 MQTT 远程触发设备升级固件。

| 项 | 说明 |
|---|---|
| 触发方式 | MQTT 点对点 / MQTT 广播 / HTTP（局域网备用） |
| 下发 Topic | `device/{mac}/command`（`target=ota`） |
| ACK Topic | `device/{mac}/ack` |
| 进度 Topic | `device/{mac}/state`（`targets.ota`） |
| 数据库 | 复用 `device_command` + `device_info.ota_*` |

**核心设计**：**OTA 就是一条特殊的指令（`action=start` + `target=ota`）**，复用现有的指令下发、ACK 匹配、超时重试机制。

### 2.2 Topic 与 QoS

| 用途 | Topic | QoS | Retained |
|---|---|---|---|
| 下发 OTA 触发 | `device/{mac}/command` | 1 | false |
| 广播 OTA 触发 | `$broadcast/command` | 1 | false |
| 设备回 ACK | `device/{mac}/ack` | 1 | false |
| 设备上报进度/结果 | `device/{mac}/state` | 1 | true |

### 2.3 OTA 触发指令

**Topic**：`device/{mac}/command`

**Payload**：

```json
{
  "commandId": "550e8400-e29b-41d4-a716-446655440000",
  "action": "start",
  "target": "ota",
  "params": {
    "url": "http://192.168.6.6:8000/firmware_1.0.30.bin",
    "version": "1.0.30",
    "md5": "abc123def456",
    "size": 1048576
  },
  "timestamp": 1791444733387
}
```

**字段说明**：

| 字段 | 必填 | 说明 |
|---|---|---|
| `commandId` | 是 | UUID，用于 ACK 匹配 |
| `action` | 是 | 固定 `"start"` |
| `target` | 是 | 固定 `"ota"` |
| `params.url` | 是 | 固件下载地址 |
| `params.version` | 是 | 目标版本号 |
| `params.md5` | 否 | 文件 MD5（校验用） |
| `params.size` | 否 | 文件大小（字节） |
| `timestamp` | 是 | 毫秒时间戳 |

**注意**：OTA 指令**无 `channel` 字段**（不需要通道概念）。

### 2.4 服务器处理流程

```
┌──────────────────────────────────────────────────────┐
│ 管理员调 HTTP: POST /api/ota/start                    │
│   {deviceId, url, version, md5, size, operator}      │
└──────────────────────┬───────────────────────────────┘
                       ▼
┌──────────────────────────────────────────────────────┐
│ DeviceOtaService.startOta()                          │
│   1. 校验设备存在                                     │
│   2. 组装 params {url, version, md5, size}           │
│   3. 调 deviceCommandService.prepareCommand()        │
└──────────────────────┬───────────────────────────────┘
                       ▼
┌──────────────────────────────────────────────────────┐
│ DeviceCommandServiceImpl.prepareCommand()            │
│   1. 生成 commandId                                   │
│   2. 入库：device_command.status = PENDING            │
│   3. 立即尝试 MQTT 下发（即时下发）                    │
│      ├─ 成功 → status = SENT                          │
│      └─ 失败 → 保持 PENDING，等定时器重试              │
└──────────────────────┬───────────────────────────────┘
                       ▼
                  MQTT Broker 1883
                       │
                       │ device/{mac}/command
                       ▼
                    设备端
```

**两个关键点**：

1. **即时下发**：`prepareCommand` 里立即尝试发 MQTT，延迟毫秒级
2. **定时器兜底**：`CommandScheduler` 每 5 秒扫描 `status=PENDING` 的指令重试

**OTA 指令的 `expireTime`**：

```java
// OTA 是长流程，过期时间要设长
int expireSeconds = 600;   // 默认 10 分钟
cmd.setExpireTime(now.plusSeconds(expireSeconds));
```

### 2.5 设备回 ACK

**Topic**：`device/{mac}/ack`

**Payload**：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "ack",
  "timestamp": 1791444734000,
  "data": {
    "commandId": "550e8400-e29b-41d4-a716-446655440000",
    "success": true,
    "action": "start",
    "target": "ota",
    "result": {
      "state": "accepted"
    }
  }
}
```

**关键认识**：**OTA 的 ACK 只表示"收到触发，准备开始"，不代表"升级完成"。**

**升级结果通过 `state` 上报。**

**服务器处理**：

```
AckMessageHandler 收到：
  1. 解析 commandId
  2. 调 deviceCommandService.handleAck(commandId, success, ...)
  3. 更新 device_command.status = SUCCESS / FAILED
```

### 2.6 设备上报进度

**Topic**：`device/{mac}/state`

**下载中**：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1791444735000,
  "data": {
    "full": false,
    "targets": {
      "ota": [
        {
          "params": {
            "state": "downloading",
            "progress": 45,
            "version": "1.0.30"
          }
        }
      ]
    }
  }
}
```

**刷写中**：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1791444740000,
  "data": {
    "full": false,
    "targets": {
      "ota": [
        { "params": { "state": "flashing", "progress": 80 } }
      ]
    }
  }
}
```

**成功**（重启后上报）：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1791444800000,
  "data": {
    "full": false,
    "targets": {
      "ota": [
        { "params": { "state": "success", "version": "1.0.30" } }
      ]
    }
  }
}
```

**失败**：

```json
{
  "device": "B4BFE90CDBA1",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1791444742000,
  "data": {
    "full": false,
    "targets": {
      "ota": [
        {
          "params": {
            "state": "fail",
            "code": "MD5_MISMATCH",
            "message": "固件校验失败"
          }
        }
      ]
    }
  }
}
```

**服务器处理**：

```
StateMessageHandler 收到：
  1. 解析 data.targets.ota[0].params
  2. 提取 state / progress / version
  3. 调 deviceInfoService.updateOtaState(deviceId, state, progress, version)
  4. 更新 device_info 表：
     - ota_state    = "downloading" / "success" / "fail"
     - ota_progress = 45 / 100
     - ota_version  = "1.0.30"
```

### 2.7 OTA 状态机

**设备端状态**：

```
IDLE → CHECKING → DOWNLOADING → VERIFYING → FLASHING → [SUCCESS | FAIL]
                                                         ↓
                                                    重启切换分区
```

**上报给服务器的状态**：

| state | 含义 | 附带字段 |
|---|---|---|
| `accepted` | 已接受 | `version` |
| `downloading` | 下载中 | `progress`, `version` |
| `verifying` | 校验中 | `version` |
| `flashing` | 刷写中 | `progress` |
| `success` | 成功 | `version` |
| `fail` | 失败 | `code`, `message` |
| `canceled` | 取消 | `reason` |

**完整状态流转**：

```
                  null（初始）
                    ↓ 管理员触发
                  accepted
                    ↓ 开始下载
                  downloading (progress: 0~100)
                    ↓ 下载完
                  verifying
                    ↓ 校验通过
                  flashing (progress: 0~100)
                    ↓ 刷写完，重启
                  success

        任意阶段失败 → fail（带 code）
```

### 2.8 数据库落地

**`device_command` 表**：

| 字段 | 值 |
|---|---|
| `command_id` | 生成的 UUID |
| `device_id` | 设备 MAC |
| `action` | `"start"` |
| `target` | `"ota"` |
| `params` | `{url, version, md5, size}` |
| `status` | PENDING → SENT → SUCCESS/FAILED/TIMEOUT |
| `expire_time` | 当前时间 + 600 秒 |

**`device_info` 表**：

| 字段 | 值 | 更新时机 |
|---|---|---|
| `ota_state` | `downloading` / `success` / `fail` | StateMessageHandler |
| `ota_progress` | 0~100 | StateMessageHandler |
| `ota_version` | `"1.0.30"` | StateMessageHandler |

**关键分工**：

| 表 | 存什么 |
|---|---|
| `device_command` | 指令的生命周期（发送/ACK/超时） |
| `device_info` | OTA 的业务进度和结果 |

### 2.9 超时与重试

**多级超时策略**：

| 阶段 | 超时时间 | 处理 |
|---|---|---|
| 等待 ACK | 30 秒 | `CommandScheduler` 重试 |
| 下载 | 10 分钟 | 标记 TIMEOUT |
| 刷写 | 5 分钟 | 标记 TIMEOUT |
| 重启后未上报 | 3 分钟 | 标记 TIMEOUT |

**OTA 的特殊之处**：

- **过期时间设长**（600 秒），避免下载中被误标超时
- **重试要幂等**：设备必须用 `commandId` 去重，同 ID 只执行一次

### 2.10 广播 OTA

**Topic**：`$broadcast/command`

**Payload**：

```json
{
  "action": "start",
  "target": "ota",
  "params": {
    "url": "http://192.168.6.6:8000/firmware_1.0.30.bin",
    "version": "1.0.30",
    "md5": "abc123def456",
    "size": 1048576,
    "rollout": {
      "percent": 10
    }
  },
  "timestamp": 1791444733387
}
```

**与点对点的区别**：

| 项 | 点对点 | 广播 |
|---|---|---|
| Topic | `device/{mac}/command` | `$broadcast/command` |
| commandId | 服务器生成 | **设备自己生成** |
| rollout | 无 | 支持灰度百分比 |
| 精确追踪 | 有（commandId） | 无（每台设备自己的 ID） |

**防雪崩策略**：

1. **灰度百分比**（`rollout.percent`）：设备根据 MAC 哈希决定是否升级
2. **随机延迟**：设备收到广播后随机延迟 0~N 秒再开始下载
3. **分批推送**：不做"全网广播"，按 10% 分批推

### 2.11 完整时序图

```
┌─────────────┐                              ┌─────────────┐
│   服务器     │                              │    设备      │
└──────┬──────┘                              └──────┬──────┘
       │                                            │
       │ ① HTTP 接口触发 OTA                          │
       │    POST /api/ota/start                      │
       │                                            │
       │ ② prepareCommand 入库 + 立即下发             │
       │    topic: device/{mac}/command              │
       │    {commandId, action:start, target:ota,    │
       │     params:{url, version, md5, size}}       │
       │ ──────────────────────────────────────────►│
       │                                            │
       │                                      ③ 设备解析：
       │                                        a. 校验 commandId 幂等
       │                                        b. 校验 url / version
       │                                        c. 准备开始
       │                                            │
       │ ④ 收到 ACK                                   │
       │    topic: device/{mac}/ack                  │
       │    {commandId, success:true,                │
       │     result:{state:"accepted"}}              │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ AckMessageHandler:                          │
       │   - 匹配 commandId                          │
       │   - device_command.status = SUCCESS         │
       │                                            │
       │ ⑤ 进度上报                                   │
       │    topic: device/{mac}/state                │
       │    {targets:{ota:[{params:                  │
       │     {state:"downloading",progress:0}}]}}    │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ ⑥ ... 下载进度 45% ... 80% ...              │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ ⑦ 校验、刷写                                 │
       │    {state:"verifying"} / {state:"flashing"} │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │         ⑧ 设备重启，断线重连                  │
       │                                            │
       │ ⑨ 重启后发 online                            │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ ⑩ 上报成功                                   │
       │    {targets:{ota:[{params:                  │
       │     {state:"success",version:"1.0.30"}}]}}  │
       │ ◄──────────────────────────────────────────│
       │                                            │
       │ StateMessageHandler:                        │
       │   - device_info.ota_state = "success"       │
       │   - device_info.ota_version = "1.0.30"      │
       │                                            │
```

### 2.12 HTTP 接口

**触发 OTA**：

```bash
POST /api/ota/start
Content-Type: application/json

{
  "deviceId": "B4BFE90CDBA1",
  "url": "http://192.168.6.6:8000/firmware_1.0.30.bin",
  "version": "1.0.30",
  "md5": "abc123def456",
  "size": 1048576,
  "operator": "admin",
  "expireSeconds": 600
}
```

**响应**：

```json
{
  "code": 200,
  "message": "success",
  "data": {
    "total": 1,
    "success": 1,
    "failed": 0,
    "failedDevices": [],
    "commandIds": ["550e8400-e29b-41d4-a716-446655440000"]
  }
}
```

**查询进度**：

```bash
GET /api/ota/{deviceId}/progress
```

**响应**：

```json
{
  "code": 200,
  "data": {
    "deviceId": "B4BFE90CDBA1",
    "otaState": "downloading",
    "otaProgress": 45,
    "otaVersion": "1.0.30",
    "lastUpdateTime": "2026-10-08 15:30:00"
  }
}
```

### 2.13 错误码

**OTA 专用错误码**：

| code | 含义 |
|---|---|
| `URL_UNREACHABLE` | 固件 URL 不可达 |
| `MD5_MISMATCH` | MD5 校验失败 |
| `NO_SPACE` | Flash 空间不足 |
| `VERSION_INCOMPATIBLE` | 版本不兼容 |
| `FLASH_FAILED` | 刷写失败 |
| `OTA_TIMEOUT` | 下载/刷写超时 |

**错误上报方式**：

- **ACK 阶段失败**：走 `device/{mac}/ack`（`success:false, error:xxx`）
- **升级过程失败**：走 `device/{mac}/state`（`targets.ota[0].params.state="fail"`）

---

## 三、注册与 OTA 的对比

| 维度 | 注册 | OTA |
|---|---|---|
| Broker | 1884 | 1883 |
| 方向 | 设备 → 服务器（请求） | 服务器 → 设备（指令） |
| Topic | `/provision/device/{mac}/register` | `device/{mac}/command`（`target=ota`） |
| 需要 ACK 吗 | ❌ 不需要 | ✅ 需要 |
| 需要 commandId 吗 | ❌ 不需要 | ✅ 需要 |
| 需要 nonce 吗 | ✅ 需要（防重放） | ❌ 不需要 |
| 需要签名吗 | ✅ 需要（身份认证） | ❌ 不需要（服务器主动发起） |
| 数据库 | `device_info`（首次写入） | `device_command` + `device_info.ota_*` |
| 超时 | 30 秒重试 | 10 分钟（长流程） |

**核心区别**：

- **注册是"设备主动报到"** → 需要身份认证（签名）
- **OTA 是"服务器主动下令"** → 需要 ACK 回执

### 5.4 OTA 远程升级

支持两种触发方式，共用同一套 OTA 引擎：

| **触发方式** | **入口**                                | **适用场景** |
| :------- | :------------------------------------ | :------- |
| HTTP     | `POST http://<device-ip>/api/ota`     | 局域网手动触发  |
| MQTT     | `device/{mac}/command` (`target=ota`) | 云端批量下发   |

**本地固件服务器**：

```bash

cd build
python -m http.server 8000 --bind 0.0.0.0
```


#### OTA 状态机


```
IDLE → CHECKING → DOWNLOADING → VERIFYING → [SUCCEEDED | FAILED]
                                              ↓
                                        重启切换分区
```


#### HTTP API

| **方法** | **路径**           | **说明**     |
| :----- | :--------------- | :--------- |
| GET    | `/`              | Web 配网页面   |
| GET    | `/scan`          | WiFi 扫描结果  |
| GET    | `/wifi_status`   | 当前 WiFi 状态 |
| POST   | `/wifi_config`   | 保存 WiFi 凭据 |
| POST   | `/factory_reset` | 恢复出厂设置     |
| GET    | `/ota_status`    | 查询 OTA 状态  |
| POST   | `/api/ota`       | 触发 OTA 升级  |

### 5.4 MOS 管控制

- 8 路独立控制，支持单路开关和全部开关
- MQTT 命令：`{"action":"set","target":"mos","channel":1,"params":{"state":1}}`
- HTTP 也提供控制接口

### 5.5 传感器数据采集

#### 硬件链路


```
GD32 传感器板 ──UART──→ ESP32
  (发送传感器原始数据帧)    (解析帧 → 环形缓冲 → MQTT 批量上报)
```


#### UART 自定义帧格式（GD32 → ESP32）


```
┌────────┬──────┬──────┬──────┬──────────┬────────────────────┬──────┐
│ HEAD   │ ADDR │ CMD  │ SEQ  │ LEN(2B)  │ DATA(N × 9 字节)   │ CRC  │
│ 0x55   │ 0x01 │ 0x01 │ 0x01 │ 大端序    │ ...                │ 0xA5 │
└────────┴──────┴──────┴──────┴──────────┴────────────────────┴──────┘

DATA 区域每条子项 9 字节：
┌───────────┬───────────────────┬───────────────┐
│ sensor_id │ timestamp_ms(4B)  │ value(4B)     │
│ uint8     │ uint32 小端序      │ uint32 小端序  │
└───────────┴───────────────────┴───────────────┘
```


#### 内部处理链路


```
UART 接收帧
  │
  ▼
sen_protocol（帧校验 + CRC）
  │ 解析出 SEN_CMD_SENSOR_DATA (0x01)
  ▼
sensor_manager（环形缓冲，256 条容量）
  │ 回调 sensor_data_callback
  ▼
sensor_mqtt_bridge（批量聚合）
  │ 触发 flush 条件：
  │   ① 积累满 32 条（BATCH_MAX_COUNT）
  │   ② 第一批数据到达后超过 100ms（BATCH_TIMEOUT_MS）
  ▼
MQTT 发布到 device/{mac}/sensor（QoS 0）
```


#### 批量上报参数

| **参数**            | **值**                     | **说明**               |
| :---------------- | :------------------------ | :------------------- |
| BATCH_MAX_COUNT   | 32                        | 一批最多聚合多少条就 flush     |
| BATCH_TIMEOUT_MS  | 100                       | 第一批到达后多久不管满不满都 flush |
| SENSOR_CACHE_SIZE | 256                       | 环形缓冲容量               |
| 发布 QoS            | 0                         | 不做 PUBACK 确认         |
| 发布条件              | MQTT 在正式 broker 且 RUNNING | provisioning 阶段丢弃    |

### 5.6 LED 状态指示

| **LED 状态** | **含义**   |
| :--------- | :------- |
| 快闪         | WiFi 未连接 |
| 慢闪         | MQTT 连接中 |
| 常亮         | 正常运行     |
| 双闪         | OTA 升级中  |

### 5.7 按键功能

| **操作** | **功能**    |
| :----- | :-------- |
| 短按     | 切换 LED    |
| 长按 3s  | WiFi 配网模式 |
| 长按 10s | 恢复出厂设置    |

---

# MQTT 协议 v4.0

> Broker 1884（注册）+ Broker 1883（运行时）
>
> **核心设计：一个方向一个入口。下行只有** `command`**，上行只有 8 个语义化 topic。**

---

## 六、Topic 全景

### 6.1 Broker 1884（注册专用）

| **Topic**                          | **方向**   | **QoS** | **Retained** |
| :--------------------------------- | :------- | :------ | :----------- |
| `/provision/device/{mac}/register` | 设备 → 服务器 | 1       | false        |
| `/provision/device/{mac}/config`   | 服务器 → 设备 | 1       | false        |

### 6.2 Broker 1883（运行时）

**下行（2 个）**

| **Topic**              | **QoS** | **Retained** | **用途**      |
| :--------------------- | :------ | :----------- | :---------- |
| `device/{mac}/command` | 1       | false        | **所有**点对点指令 |
| `$broadcast/command`   | 1       | false        | **所有**广播指令  |

**上行（8 个）**

| **Topic**                | **QoS** | **Retained** | **用途**   |
| :----------------------- | :------ | :----------- | :------- |
| `device/{mac}/online`    | 1       | true         | 上线       |
| `device/{mac}/offline`   | 1       | true         | 主动离线     |
| `device/{mac}/will`      | 1       | true         | LWT 异常断开 |
| `device/{mac}/heartbeat` | 0       | false        | 心跳       |
| `device/{mac}/state`     | 1       | true         | 状态上报     |
| `device/{mac}/ack`       | 1       | false        | 指令回执     |
| `device/{mac}/event`     | 1       | false        | 事件/错误    |
| `device/{mac}/sensor`    | 0       | false        | 传感器数据    |

**总计：下行 2 个 + 上行 8 个 = 10 个 topic。**

---

## 七、下行协议（统一 `command`）

### 7.1 通用结构

```json

{
  "commandId": "550e8400-e29b-41d4-a716-446655440000",
  "action": "set",
  "target": "mos",
  "channel": 1,
  "params": { "state": 1 },
  "timestamp": 1710000000000
}
```


| **字段**      | **类型** | **必填** | **说明**                                                       |
| :---------- | :----- | :----- | :----------------------------------------------------------- |
| `commandId` | string | 是      | UUID，用于 ACK 匹配                                               |
| `action`    | string | 是      | `set` / `get` / `toggle` / `start` / `reset` / `cancel`      |
| `target`    | string | 是      | `mos` / `led` / `servo` / `relay` / `ota` / `config` / 任意自定义 |
| `channel`   | int    | 否      | 通道号，`0`=全部，省略=无通道概念                                          |
| `params`    | object | 否      | `set` / `start` 时必填                                          |
| `timestamp` | long   | 是      | 毫秒时间戳                                                        |

### 7.2 `action` 取值

| **action** | **含义** | **适用 target**                      |
| :--------- | :----- | :--------------------------------- |
| `set`      | 设置     | mos / led / servo / relay / config |
| `get`      | 查询     | mos / led / servo / relay / config |
| `toggle`   | 翻转     | mos / led / relay                  |
| `start`    | 启动     | ota                                |
| `reset`    | 重置     | config / system                    |
| `cancel`   | 取消     | ota                                |

### 7.3 `target` 取值

| **target** | **说明** | **扩展方式** |
| :--------- | :----- | :------- |
| `mos`      | MOS 管  | 加新外设时加新值 |
| `led`      | LED    | 同上       |
| `servo`    | 舵机     | 同上       |
| `relay`    | 继电器    | 同上       |
| `ota`      | 固件升级   | 保留       |
| `config`   | 配置下发   | 保留       |
| `system`   | 系统操作   | 保留       |
| 自定义        | 任意新外设  | 直接加值     |

### 7.4 常用场景示例

**MOS 单路开关**

```json

{
  "commandId": "550e8400-e29b-41d4-a716-446655440000",
  "action": "set",
  "target": "mos",
  "channel": 1,
  "params": { "state": 1 },
  "timestamp": 1710000000000
}
```


**MOS 全部开关**

```json

{
  "commandId": "550e8400-...",
  "action": "set",
  "target": "mos",
  "channel": 0,
  "params": { "state": 1 },
  "timestamp": 1710000000001
}
```


**MOS 状态查询**

```json

{
  "commandId": "550e8400-...",
  "action": "get",
  "target": "mos",
  "channel": 1,
  "timestamp": 1710000000002
}
```


**LED 开关（带亮度）**

```json

{
  "commandId": "550e8400-...",
  "action": "set",
  "target": "led",
  "channel": 1,
  "params": { "state": 1, "brightness": 80 },
  "timestamp": 1710000000003
}
```


**舵机角度**

```json

{
  "commandId": "550e8400-...",
  "action": "set",
  "target": "servo",
  "channel": 1,
  "params": { "angle": 90 },
  "timestamp": 1710000000004
}
```


**继电器**

```json

{
  "commandId": "550e8400-...",
  "action": "set",
  "target": "relay",
  "channel": 1,
  "params": { "state": 0 },
  "timestamp": 1710000000005
}
```


**OTA 升级（不再独立 topic）**

```json

{
  "commandId": "550e8400-...",
  "action": "start",
  "target": "ota",
  "params": {
    "url": "http://192.168.124.6:8000/sample_project.bin",
    "version": "1.0.30",
    "md5": "abc123def456",
    "size": 1048576
  },
  "timestamp": 1710000000006
}
```


**配置下发（不再独立 topic）**

```json

{
  "commandId": "550e8400-...",
  "action": "set",
  "target": "config",
  "params": {
    "config": { "log_level": 3, "heartbeat_interval": 30 },
    "configVersion": 6,
    "persist": true
  },
  "timestamp": 1710000000007
}
```


**查询当前配置**

```json

{
  "commandId": "550e8400-...",
  "action": "get",
  "target": "config",
  "timestamp": 1710000000008
}
```


**重置配置**

```json

{
  "commandId": "550e8400-...",
  "action": "reset",
  "target": "config",
  "timestamp": 1710000000009
}
```


### 7.5 广播指令 `$broadcast/command`

```json

{
  "action": "start",
  "target": "ota",
  "params": {
    "url": "http://192.168.124.6:8000/sample_project.bin",
    "version": "1.0.30",
    "rollout": { "percent": 10 }
  },
  "timestamp": 1710000000010
}
```


**广播无** `commandId`，设备收到后自己生成，用于本地 ACK。

支持 `rollout.percent` 灰度：设备根据 MAC 哈希决定是否升级。

---

## 八、上行协议（8 个 topic）

### 8.1 通用包装

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "<消息类型>",
  "timestamp": 1710000000000,
  "data": { ... }
}
```


| **字段**      | **类型** | **必填** | **说明**       |
| :---------- | :----- | :----- | :----------- |
| `device`    | string | 是      | 设备 MAC（无冒号）  |
| `product`   | string | 是      | 产品型号         |
| `type`      | string | 是      | 消息类型         |
| `timestamp` | long   | 是      | 毫秒时间戳        |
| `data`      | object | 是      | 内容，随 type 变化 |

### 8.2 `device/{mac}/online`

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "online",
  "timestamp": 1710000000000,
  "data": {
    "firmware": "1.0.29",
    "capabilities": {
      "mos": 8,
      "led": 3,
      "servo": 2
    }
  }
}
```


`capabilities` 是能力清单，服务器据此知道设备有哪些外设。

### 8.3 `device/{mac}/offline`

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "offline",
  "timestamp": 1710000000001,
  "data": { "reason": "shutdown" }
}
```


`reason` 取值：

| **reason**      | **含义** |
| :-------------- | :----- |
| `shutdown`      | 正常关机   |
| `factory_reset` | 恢复出厂   |
| `manual`        | 手动下线   |

### 8.4 `device/{mac}/will`（LWT，Broker 自动发）

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "offline",
  "timestamp": 0,
  "data": { "reason": "mqtt_lwt" }
}
```


设备连接时注册 LWT，Broker 在异常断开时代为发布，设备本身不发送。

### 8.5 `device/{mac}/heartbeat`

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "heartbeat",
  "timestamp": 1710000000002,
  "data": {
    "uptime": 1234,
    "rssi": -65
  }
}
```


### 8.6 `device/{mac}/state`

**统一结构**：`{type:"state", data:{full, targets}}`

**上线全量**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1710000000004,
  "data": {
    "full": true,
    "targets": {
      "mos": [
        { "channel": 0, "params": { "state": 0 } },
        { "channel": 1, "params": { "state": 1 } },
        { "channel": 2, "params": { "state": 0 } },
        { "channel": 3, "params": { "state": 0 } },
        { "channel": 4, "params": { "state": 0 } },
        { "channel": 5, "params": { "state": 0 } },
        { "channel": 6, "params": { "state": 0 } },
        { "channel": 7, "params": { "state": 0 } }
      ],
      "led": [
        { "channel": 1, "params": { "state": 1, "brightness": 80 } }
      ],
      "servo": [
        { "channel": 1, "params": { "angle": 90 } }
      ]
    }
  }
}
```


**增量上报**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1710000000005,
  "data": {
    "full": false,
    "targets": {
      "mos": [
        { "channel": 1, "params": { "state": 0 } }
      ]
    }
  }
}
```


**OTA 状态**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1710000000006,
  "data": {
    "full": false,
    "targets": {
      "ota": [
        { "params": { "state": "downloading", "progress": 45, "version": "1.0.30" } }
      ]
    }
  }
}
```


OTA `state` 取值：

| **state**     | **含义** |
| :------------ | :----- |
| `accepted`    | 已接受    |
| `downloading` | 下载中    |
| `verifying`   | 校验中    |
| `flashing`    | 刷写中    |
| `success`     | 成功     |
| `fail`        | 失败     |
| `canceled`    | 取消     |

**配置状态（可选上报）**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "state",
  "timestamp": 1710000000007,
  "data": {
    "full": false,
    "targets": {
      "config": [
        { "params": { "configVersion": 6, "log_level": 3, "heartbeat_interval": 30 } }
      ]
    }
  }
}
```


### 8.7 `device/{mac}/ack`

**所有下行指令的回执**（含控制、OTA、config）。

**成功**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "ack",
  "timestamp": 1710000000008,
  "data": {
    "commandId": "550e8400-...",
    "success": true,
    "action": "set",
    "target": "mos",
    "channel": 1,
    "result": { "state": 1 }
  }
}
```


**失败**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "ack",
  "timestamp": 1710000000009,
  "data": {
    "commandId": "550e8400-...",
    "success": false,
    "error": "CHANNEL_NOT_FOUND",
    "message": "通道 1 不存在"
  }
}
```


**OTA 触发 ACK**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "ack",
  "timestamp": 1710000000010,
  "data": {
    "commandId": "550e8400-...",
    "success": true,
    "action": "start",
    "target": "ota",
    "result": { "state": "accepted" }
  }
}
```


> **注意**：OTA 的 ACK 只表示"收到触发，准备开始"，不代表"升级完成"。升级结果通过 `state` 上报。

### 8.8 `device/{mac}/event`

**正常事件**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "event",
  "timestamp": 1710000000011,
  "data": {
    "event": "mos_change",
    "channel": 1,
    "state": 1,
    "trigger": "local_button"
  }
}
```


**错误事件**

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "error",
  "timestamp": 1710000000012,
  "data": {
    "code": "MOS_OVER_CURRENT",
    "message": "MOS 过流保护",
    "context": "mos"
  }
}
```


| **字段**    | **类型** | **说明**                                     |
| :-------- | :----- | :----------------------------------------- |
| `event`   | string | 事件名，如 `mos_change` / `factory_reset`       |
| `trigger` | string | 触发源：`local_button` / `remote` / `schedule` |
| `code`    | string | 错误码（字符串，支持 `MOS_OVER_CURRENT` 等）           |
| `message` | string | 错误描述                                       |
| `context` | string | 错误发生的上下文模块                                 |

### 8.9 `device/{mac}/sensor`

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "sensor_batch",
  "timestamp": 1710000000013,
  "data": [
    {
      "sensor_id": 1,
      "sensor_type": "temperature",
      "timestamp": 1710000000010,
      "value": 25.5,
      "unit": "°C"
    },
    {
      "sensor_id": 2,
      "sensor_type": "grating_counter",
      "timestamp": 1710000000011,
      "value": 100,
      "unit": "count"
    }
  ]
}
```


| **字段**        | **类型** | **说明**                                                           |
| :------------ | :----- | :--------------------------------------------------------------- |
| `sensor_id`   | int    | 传感器通道号                                                           |
| `sensor_type` | string | `temperature` / `grating_counter` / `humidity` / `pressure` / 任意 |
| `timestamp`   | long   | 采集时间（毫秒）                                                         |
| `value`       | number | 数值                                                               |
| `unit`        | string | 单位                                                               |

---

## 九、注册流程（Broker 1884）

### 9.1 第一步：设备发注册请求

**Topic**：`/provision/device/{mac}/register`

```json

{
  "device": "B4BFE90CDBA0",
  "product": "SmartHome-v1",
  "type": "register",
  "timestamp": 1710000000700,
  "data": {
    "firmware": "1.0.29",
    "chip": "ESP32",
    "hardware_version": "V1.0",
    "nonce": "550e8400e29b41d4a716446655440000",
    "pubkey": "-----BEGIN PUBLIC KEY-----\nMFkwEwYH...\n-----END PUBLIC KEY-----"
  }
}
```


### 9.2 第二步：服务器回配置

**Topic**：`/provision/device/{mac}/config`

```json
{
  "device": "B4BFE90CDBA0",
  "success": true,
  "mqtt": {
    "host": "192.168.124.6",
    "port": 1883,
    "clientId": "device-B4BFE90CDBA0",
    "username": "dev_B4BFE90CDBA0",
    "password": "xxxxx",
    "keepAlive": 60
  },
  "will": {
    "topic": "device/B4BFE90CDBA0/will",
    "qos": 1,
    "retain": true,
    "payload": {
      "device": "B4BFE90CDBA0",
      "product": "SmartHome-v1",
      "type": "offline",
      "data": { "reason": "mqtt_lwt" }
    }
  },
  "config": {
    "heartbeat_interval": 30,
    "sensor_batch_size": 32
  },
  "timestamp": 1710000000701
}
```


### 9.3 第三步：设备切换

1. 保存配置到 NVS
2. 断开 Broker 1884
3. 连接 Broker 1883
4. 发 `device/{mac}/online`

> `clientId` 和 `will_topic` 服务器可省略，设备会自动用 device_id 和默认值填充。
>
> `success != true` 时设备重试 10 次，每次等待 30 秒。

---

## 十、OTA 完整流程


```
后端                                       设备
  │                                          │
  │ 1. device/{mac}/command                  │
  │    {commandId, action:"start",           │
  │     target:"ota", params:{url, ...}}     │
  │ ────────────────────────────────────────►│
  │                                          │
  │ 2. device/{mac}/ack                      │
  │    {commandId, success:true,             │
  │     result:{state:"accepted"}}           │
  │ ◄────────────────────────────────────────│
  │                                          │
  │ 3. device/{mac}/state                    │
  │    {targets:{ota:[{params:               │
  │     {state:"downloading",progress:0}}]}} │
  │ ◄────────────────────────────────────────│
  │                                          │
  │ 4. ... 下载进度 ...                       │
  │                                          │
  │ 5. 设备重启                               │
  │                                          │
  │ 6. device/{mac}/online                   │
  │ ◄────────────────────────────────────────│
  │                                          │
  │ 7. device/{mac}/state                    │
  │    {targets:{ota:[{params:               │
  │     {state:"success",version:"1.0.30"}}]}}│
  │ ◄────────────────────────────────────────│
```


**广播 OTA**：走 `$broadcast/command`，无 `commandId`，设备自生成。

**防雪崩**：

- 分批广播（`rollout.percent` 灰度）
- 设备随机延迟 0\~N 秒再下载
- 设备根据 MAC 哈希决定是否升级

---

## 十一、字段说明

### 11.1 下行指令字段

| **字段**      | **类型** | **必填** | **说明**                                                          |
| :---------- | :----- | :----- | :-------------------------------------------------------------- |
| `commandId` | string | 是      | UUID，用于 ACK 匹配                                                  |
| `action`    | string | 是      | `set` / `get` / `toggle` / `start` / `reset` / `cancel`         |
| `target`    | string | 是      | `mos` / `led` / `servo` / `relay` / `ota` / `config` / `system` |
| `channel`   | int    | 否      | 通道号，`0`=全部                                                      |
| `params`    | object | 否      | 动作参数                                                            |
| `timestamp` | long   | 是      | 毫秒时间戳                                                           |

### 11.2 上行通用字段

| **字段**      | **类型** | **必填** | **说明**                                                                                    |
| :---------- | :----- | :----- | :---------------------------------------------------------------------------------------- |
| `device`    | string | 是      | 设备 MAC                                                                                    |
| `product`   | string | 是      | 产品型号                                                                                      |
| `type`      | string | 是      | `online` / `offline` / `heartbeat` / `state` / `ack` / `event` / `error` / `sensor_batch` |
| `timestamp` | long   | 是      | 毫秒时间戳                                                                                     |
| `data`      | object | 是      | 具体内容                                                                                      |

### 11.3 state 内部字段

| **字段**                     | **类型**  | **说明**               |
| :------------------------- | :------ | :------------------- |
| `full`                     | boolean | `true`=全量，`false`=增量 |
| `targets`                  | object  | key 为部件名，value 为通道数组 |
| `targets.{name}[].channel` | int     | 通道号                  |
| `targets.{name}[].params`  | object  | 状态参数                 |

### 11.4 ack 内部字段

| **字段**                          | **类型**  | **说明**            |
| :------------------------------ | :------ | :---------------- |
| `commandId`                     | string  | 对应下行指令的 commandId |
| `success`                       | boolean | 是否成功              |
| `action` / `target` / `channel` | -       | 回显指令信息            |
| `result`                        | object  | 成功时的结果            |
| `error` / `message`             | string  | 失败时的错误码和描述        |

### 11.5 event 内部字段

| **字段**             | **类型** | **说明**             |
| :----------------- | :----- | :----------------- |
| `event`            | string | 事件名                |
| `trigger`          | string | 触发源                |
| `code` / `message` | string | `type=error` 时的错误码 |
| `context`          | string | 错误上下文模块            |

### 11.6 sensor 内部字段

| **字段**        | **类型** | **说明** |
| :------------ | :----- | :----- |
| `sensor_id`   | int    | 传感器通道号 |
| `sensor_type` | string | 传感器类型  |
| `timestamp`   | long   | 采集时间   |
| `value`       | number | 数值     |
| `unit`        | string | 单位     |

---

## 十二、错误码规范

### 12.1 通用错误码

| **code**               | **含义**         |
| :--------------------- | :------------- |
| `INVALID_PAYLOAD`      | JSON 格式错误或字段缺失 |
| `UNKNOWN_ACTION`       | action 不支持     |
| `UNKNOWN_TARGET`       | target 不支持     |
| `CHANNEL_NOT_FOUND`    | 通道不存在          |
| `CHANNEL_OUT_OF_RANGE` | 通道号超出范围        |
| `PARAM_MISSING`        | 缺少必要参数         |
| `PARAM_INVALID`        | 参数值非法          |
| `DEVICE_BUSY`          | 设备忙，稍后重试       |
| `EXECUTE_FAILED`       | 执行失败（硬件层）      |
| `TIMEOUT`              | 执行超时           |

### 12.2 OTA 错误码

| **code**               | **含义**   |
| :--------------------- | :------- |
| `URL_UNREACHABLE`      | URL 不可达  |
| `MD5_MISMATCH`         | MD5 校验失败 |
| `NO_SPACE`             | 空间不足     |
| `VERSION_INCOMPATIBLE` | 版本不兼容    |
| `FLASH_FAILED`         | 刷写失败     |
| `OTA_TIMEOUT`          | 超时       |

### 12.3 错误码速查（上行 event topic）

| **code**               | **来源模块**     | **含义**                   |
| :--------------------- | :----------- | :----------------------- |
| `INVALID_PAYLOAD`      | mqtt_service | control 消息过长 / JSON 解析失败 |
| `UNKNOWN_ACTION`       | mqtt_service | control 缺少 `action` 字段   |
| `UNKNOWN_TARGET`       | mqtt_service | 未知 `target`              |
| `PARAM_MISSING`        | mqtt_service | 缺少 `channel` 或 `params`  |
| `PARAM_INVALID`        | mqtt_service | 参数非数字或非法                 |
| `CHANNEL_OUT_OF_RANGE` | mqtt_service | channel 越界（≥8）           |
| `EXECUTE_FAILED`       | mqtt_service | MOS 控制失败                 |
| `URL_UNREACHABLE`      | ota.c        | OTA URL 不可达              |
| `MD5_MISMATCH`         | ota.c        | OTA 校验失败                 |

---

## 十三、QoS 与 Retained 策略

| **Topic**                          | **QoS** | **Retained** | **原因**                          |
| :--------------------------------- | :------ | :----------- | :------------------------------ |
| `device/{mac}/command`             | 1       | false        | 指令必须送达；retained 会导致设备重连收到旧指令，危险 |
| `$broadcast/command`               | 1       | false        | 同上                              |
| `device/{mac}/online`              | 1       | true         | 在线状态需快速恢复                       |
| `device/{mac}/offline`             | 1       | true         | 离线状态需快速恢复                       |
| `device/{mac}/will`                | 1       | true         | LWT，需快速恢复                       |
| `device/{mac}/heartbeat`           | 0       | false        | 高频，丢一两条无所谓                      |
| `device/{mac}/state`               | 1       | true         | 状态需快速恢复                         |
| `device/{mac}/ack`                 | 1       | false        | 回执即时消费                          |
| `device/{mac}/event`               | 1       | false        | 事件即时消费                          |
| `device/{mac}/sensor`              | 0       | false        | 批量数据，高频                         |
| `/provision/device/{mac}/register` | 1       | false        | 一次性                             |
| `/provision/device/{mac}/config`   | 1       | false        | 一次性                             |

---

## 十四、设备在线/离线判断

### 14.1 四种判断机制

| **机制** | **Topic**                | **触发方**   | **延迟** | **说明**                            |
| :----- | :----------------------- | :-------- | :----- | :-------------------------------- |
| 上线通知   | `device/{mac}/online`    | 设备主动      | 秒级     | 连接成功后上报                           |
| 主动离线   | `device/{mac}/offline`   | 设备主动      | 秒级     | 正常关机/重启/恢复出厂                      |
| LWT 遗嘱 | `device/{mac}/will`      | Broker 自动 | 秒级     | 异常断开                              |
| 心跳超时   | `device/{mac}/heartbeat` | 后端扫描      | 分钟级    | 兜底，`now - lastHeartbeat > 3 × 间隔` |

### 14.2 离线原因

| **reason**          | **含义** | **来源**     |
| :------------------ | :----- | :--------- |
| `shutdown`          | 正常关机   | 设备主动       |
| `factory_reset`     | 恢复出厂   | 设备主动       |
| `manual`            | 手动下线   | 设备主动       |
| `mqtt_lwt`          | 异常断开   | Broker LWT |
| `heartbeat_timeout` | 心跳超时   | 后端扫描       |

### 14.3 状态机


```
                  ┌─────────────┐
                  │   UNKNOWN   │  初始状态（从未上线）
                  └──────┬──────┘
                         │ 收到 online
                         ▼
                  ┌─────────────┐
        ┌────────►│   ONLINE    │
        │         └──────┬──────┘
        │                │
   收到 online      ┌────┼────┬────────────┐
        │           │    │    │            │
        │      收到 offline will    心跳超时
        │           │    │    │            │
        │           ▼    ▼    ▼            ▼
        │         ┌──────────────────────┐
        └─────────│      OFFLINE         │
                  └──────────────────────┘
```


---

## 十五、指令生命周期状态机


```
┌─────────────┐
│   PENDING   │  后端入库
└──────┬──────┘
       │ MQTT 发布
       ▼
┌─────────────┐
│    SENT     │  已发送，等待 ACK
└──────┬──────┘
       │
  ┌────┼────┐
  │    │    │
收到  收到  超时
成功  失败  未收
ACK   ACK   ACK
  │    │    │
  ▼    ▼    ▼
┌────┐┌────┐┌────────┐
│SUC ││FAIL││TIMEOUT │
│CESS││    ││        │
└────┘└────┘└───┬────┘
                │
           retryCount < maxRetry?
                │
           ┌────┴────┐
           │ 是      │ 否
           ▼         ▼
       重新 SENT   标记 FAILED
```


**幂等要求**：设备必须用 `commandId` 去重，同 ID 只执行一次。

---

## 十六、后端订阅配置

```yaml

mqtt:
  broker: 192.168.124.6
  port: 1883
  username: MQTT1
  password: 123456
  client-id: spring-boot-server
  topics:
    - device/+/online
    - device/+/offline
    - device/+/will
    - device/+/heartbeat
    - device/+/state
    - device/+/ack
    - device/+/event
    - device/+/sensor
  qos:
    - 1
    - 1
    - 1
    - 0
    - 1
    - 1
    - 1
    - 0
```


---

## 十七、Handler 划分

| **Handler**               | **匹配 Topic**                         | **职责**              |
| :------------------------ | :----------------------------------- | :------------------ |
| `OnlineMessageHandler`    | `device/+/online`                    | 标记在线，记录上线时间         |
| `OfflineMessageHandler`   | `device/+/offline` 和 `device/+/will` | 标记离线，记录原因           |
| `HeartbeatMessageHandler` | `device/+/heartbeat`                 | 更新最后心跳时间            |
| `StateMessageHandler`     | `device/+/state`                     | 更新设备状态（全量/增量）       |
| `AckMessageHandler`       | `device/+/ack`                       | 匹配 commandId，更新指令状态 |
| `EventMessageHandler`     | `device/+/event`                     | 处理事件和错误             |
| `SensorMessageHandler`    | `device/+/sensor`                    | 存储传感器数据             |

---

## 十八、MQTT 功能测试

项目提供 `mqtt_test.py` 一站式测试脚本。

### 18.1 安装依赖

```bash

pip install paho-mqtt
```


### 18.2 交互菜单模式

```bash

python mqtt_test.py
```


### 18.3 命令行直执模式

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

# 模拟注册响应
python mqtt_test.py --provision

# 指定不同设备 / broker
python mqtt_test.py --dev A1B2C3D4E5F6 --host 10.0.0.5 --port 1883
```


### 18.4 手动测试（MQTTX）

**MOS 控制**


```
Topic:   device/B4BFE90CDBA0/command
Payload: {"commandId":"uuid-1","action":"set","target":"mos","channel":1,"params":{"state":1},"timestamp":1710000000000}
```


**OTA 升级**


```
Topic:   device/B4BFE90CDBA0/command
Payload: {"commandId":"uuid-2","action":"start","target":"ota","params":{"url":"http://192.168.124.6:8000/sample_project.bin","version":"1.0.30"},"timestamp":1710000000001}
```


**监听设备上报**

订阅 `device/B4BFE90CDBA0/#` 可一次性看到所有消息。

---

## 十九、OTA 升级流程

### 19.1 方式一：一键脚本

```bash

ota.bat 1.0.30
```


脚本自动完成：

1. 修改 `CMakeLists.txt` 版本号
2. 编译固件
3. 启动 Python HTTP 文件服务器
4. 向设备发送 OTA 触发请求

### 19.2 方式二：手动 HTTP

```bash

# 1. 启动 HTTP 服务器
cd build
python -m http.server 8000 --bind 0.0.0.0

# 2. 向设备发送 OTA 请求
curl -X POST http://<device-ip>/api/ota \
  -H "Content-Type: application/json" \
  -d '{"url":"http://<your-pc-ip>:8000/sample_project.bin","version":"1.0.30"}'

# 3. 查看 OTA 状态
curl http://<device-ip>/ota_status
```


### 19.3 方式三：MQTT 点对点

MQTTX 连接 broker 1883，发布到目标设备：


```
Topic:   device/B4BFE90CDBA0/command
QoS:     1
Payload: {"commandId":"550e8400-...","action":"start","target":"ota","params":{"url":"http://<your-pc-ip>:8000/sample_project.bin","version":"1.0.30"},"timestamp":1710000000000}
```


### 19.4 方式四：MQTT 广播（所有设备）


```
Topic:   $broadcast/command
QoS:     1
Payload: {"action":"start","target":"ota","params":{"url":"http://<your-pc-ip>:8000/sample_project.bin","version":"1.0.30","rollout":{"percent":10}},"timestamp":1710000000000}
```


### 19.5 流程示意


```
┌─ 点对点（HTTP / MQTT 单设备）───────────────────────┐
│                                                       │
│  服务器 → device/B4BFE90CDBA0/command                │
│                                                       │
│  B4BFE90CDBA0 ──触发OTA──→ 校验版本 ──下载──→ 重启  │
│                                                       │
└───────────────────────────────────────────────────────┘

┌─ 广播（MQTT 全设备统一）──────────────────────────┐
│                                                       │
│  服务器 → $broadcast/command                         │
│                                                       │
│  B4BFE90CDBA0 ──触发OTA──→ 校验版本 ──下载──→ 重启  │
│  C8D7A9B6E5F4 ──触发OTA──→ 校验版本 ──下载──→ 重启  │
│  8A9B0C1D2E3F ──触发OTA──→ 校验版本 ──下载──→ 重启  │
│  ... 所有订阅了 $broadcast/command 的设备同时升级   │
│                                                       │
└───────────────────────────────────────────────────────┘
```


---

## 二十、扩展指南

### 20.1 新增外设（比如继电器）

**设备端**：

1. 上线时 `capabilities` 加 `"relay": 4`
2. 状态上报时 `targets.relay` 加数据
3. 处理 `target=relay` 的 command
4. 回 ACK

**服务器端**：零改动。

**数据库**：零改动。

**协议**：零改动。

### 20.2 新增传感器

**设备端**：

1. `sensor_type` 用新值（如 `pressure`）
2. 上报时带 `sensor_type`

**服务器端**：零改动。

### 20.3 新增事件

**设备端**：

1. `event` 用新事件名
2. 上报

**服务器端**：零改动。

### 20.4 新增指令动作

**设备端**：

1. `action` 加新值（如 `calibrate`）
2. 处理逻辑

**服务器端**：零改动。

### 20.5 新增广播功能

**服务器端**：

1. 发 `$broadcast/command`，指定 `target` 和 `action`
2. 设备端处理

**协议**：零改动。
