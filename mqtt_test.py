#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ESP32 Smart Home - MQTT 功能测试脚本

覆盖场景：
  1. MOS 单路控制 / 全部控制 / 状态查询
  2. OTA 升级触发
  3. 传感器数据接收监听
  4. 心跳 / 事件 / 遗嘱 监听
  5. 设备注册（provisioning）响应模拟

依赖：
  pip install paho-mqtt

用法：
  python mqtt_test.py                              # 交互菜单
  python mqtt_test.py --mos 1 1                    # 直接执行：通道1开
  python mqtt_test.py --ota 1.0.30 http://192.168.124.6:8000/sample_project.bin
  python mqtt_test.py --listen                     # 监听所有 topic
  python mqtt_test.py --provision 192.168.124.6 1884  # 模拟注册响应
"""

import json
import sys
import time
import argparse
import threading

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("请先安装依赖: pip install paho-mqtt")
    sys.exit(1)

# ==================== 配置 ====================
DEVICE_ID = "B4BFE90CDBA0"
PRODUCT_ID = "SmartHome-v1"

# 正式 broker
BROKER_HOST = "192.168.124.6"
BROKER_PORT = 1883
BROKER_USER = "MQTT1"
BROKER_PASS = "123456"

# 注册 broker
PROVISION_PORT = 1884

# Topic 模板
TPL_CONTROL = "device/{dev}/control"
TPL_STATUS = "device/{dev}/status"
TPL_EVENT = "device/{dev}/event"
TPL_MOS_STATE = "device/{dev}/mos_state"
TPL_HEART = "device/{dev}/heart"
TPL_SENSOR = "device/{dev}/sensor"
TPL_OTA = "device/{dev}/ota"
TPL_WILL = "device/{dev}/will"
TPL_CONFIG = "device/{dev}/config"
TPL_PROVISION_REG = "/provision/device/{dev}/register"
TPL_PROVISION_RESP = "/provision/device/{dev}/config/response"


def topic(template, dev=DEVICE_ID):
    return template.format(dev=dev)


# ==================== MQTT 客户端封装 ====================
class DeviceClient:
    def __init__(self, host, port, username="", password="", client_id="test_script"):
        self.host = host
        self.port = port
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        if username:
            self.client.username_pw_set(username, password)
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.connected = threading.Event()
        self._subscribed = set()

    def _on_connect(self, client, userdata, flags, rc, properties=None):
        if rc == 0:
            print(f"[连接成功] {self.host}:{self.port}")
            self.connected.set()
        else:
            print(f"[连接失败] rc={rc}")

    def _on_message(self, client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
            try:
                parsed = json.loads(payload)
                print(f"[{msg.topic}] {json.dumps(parsed, indent=2, ensure_ascii=False)}")
            except json.JSONDecodeError:
                print(f"[{msg.topic}] {payload}")
        except Exception as e:
            print(f"[消息解析异常] {e}")

    def connect(self, timeout=5):
        self.client.connect(self.host, self.port, keepalive=30)
        self.client.loop_start()
        if not self.connected.wait(timeout):
            raise RuntimeError(f"连不上 {self.host}:{self.port}")
        return self

    def disconnect(self):
        self.client.loop_stop()
        self.client.disconnect()
        self.connected.clear()

    def sub(self, *topics, qos=1):
        for t in topics:
            if t not in self._subscribed:
                self.client.subscribe(t, qos=qos)
                self._subscribed.add(t)
                print(f"[订阅] {t}")

    def pub(self, tpl_or_topic, payload, qos=1, retain=False, use_tpl=True):
        t = topic(tpl_or_topic) if use_tpl else tpl_or_topic
        if isinstance(payload, dict):
            payload = json.dumps(payload)
        elif payload is None:
            payload = ""
        info = self.client.publish(t, payload, qos=qos, retain=retain)
        rc = info.wait_for_publish(timeout=5)
        if rc == mqtt.MQTT_ERR_SUCCESS:
            print(f"[发布] {t} → {payload[:120]}{'...' if len(payload) > 120 else ''}")
        else:
            print(f"[发布失败] {t} rc={rc}")


# ==================== 命令实现 ====================

def cmd_mos_single(client, channel, state):
    """单路 MOS 控制"""
    print(f"\n=== MOS 单路控制: channel={channel} state={state} ===")
    # 先订阅事件和状态，看设备回复
    client.sub(topic(TPL_EVENT), topic(TPL_MOS_STATE))
    client.pub(TPL_CONTROL, {"cmd": "mos", "channel": channel, "state": state})
    time.sleep(1.5)


def cmd_mos_all(client, state):
    """全部 MOS 控制"""
    print(f"\n=== MOS 全部控制: state={state} ===")
    client.sub(topic(TPL_EVENT), topic(TPL_MOS_STATE))
    client.pub(TPL_CONTROL, {"cmd": "mos_all", "state": state})
    time.sleep(1.5)


def cmd_mos_query(client):
    """查询当前 MOS 状态"""
    print("\n=== MOS 状态查询 ===")
    client.sub(topic(TPL_MOS_STATE))
    client.pub(TPL_CONTROL, {"cmd": "mos_query"})
    time.sleep(1.5)


def cmd_ota(client, version, url):
    """触发 OTA 升级"""
    print(f"\n=== OTA 升级: version={version} ===")
    client.sub(topic(TPL_EVENT))
    client.sub(topic(TPL_STATUS))
    client.pub(TPL_OTA, {"url": url, "version": version})
    print("设备开始下载后会自动重启，观察串口日志...")


def cmd_listen(client, duration=15):
    """监听所有 topic"""
    print(f"\n=== 监听模式 ({duration}s) ===")
    client.sub(
        topic(TPL_STATUS),
        topic(TPL_EVENT),
        topic(TPL_MOS_STATE),
        topic(TPL_HEART),
        topic(TPL_SENSOR),
        topic(TPL_WILL),
        topic(TPL_CONFIG),
        topic(TPL_PROVISION_REG),
    )
    print(f"监听中... Ctrl+C 停止（等待 {duration}s）")
    try:
        time.sleep(duration)
    except KeyboardInterrupt:
        pass


def cmd_provision_response(provision_host, provision_port, prod_host, prod_port):
    """
    模拟服务器：等待设备 register，然后回复 config response
    broker:1884 上发 /provision/device/{mac}/register
    我们监听后自动回复 /provision/device/{mac}/config/response
    """
    print(f"\n=== 模拟注册服务器: {provision_host}:{provision_port} ===")
    print("等待设备注册请求...")

    reply_sent = threading.Event()

    def on_register(client, userdata, msg):
        try:
            payload = msg.payload.decode("utf-8", errors="replace")
            print(f"[收到注册请求] {msg.topic}")
            print(f"  payload: {payload}")
            data = json.loads(payload)
            dev_id = data.get("device_id", DEVICE_ID)

            response = {
                "status": "success",
                "config": {
                    "broker_uri": f"mqtt://{prod_host}:{prod_port}",
                    "client_id": dev_id,
                    "username": BROKER_USER,
                    "password": BROKER_PASS,
                    "will_topic": f"device/{dev_id}/will",
                },
            }
            resp_topic = topic(TPL_PROVISION_RESP, dev=dev_id)
            client.publish(resp_topic, json.dumps(response), qos=1)
            print(f"[回复配置] {resp_topic}")
            reply_sent.set()
        except Exception as e:
            print(f"[注册处理异常] {e}")

    c = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="provision_simulator")
    c.on_message = on_register
    c.connect(provision_host, provision_port, keepalive=30)
    c.subscribe(topic(TPL_PROVISION_REG), qos=1)
    c.loop_start()
    print("已订阅 /provision/device/+/register，等待设备上线...")

    try:
        reply_sent.wait(timeout=60)
        print("[完成] 配置已下发，设备应该会重连到正式 broker")
    except KeyboardInterrupt:
        pass
    c.loop_stop()
    c.disconnect()


# ==================== 交互菜单 ====================

def interactive_menu():
    print(f"""
╔══════════════════════════════════════════╗
║  ESP32 Smart Home - MQTT 测试菜单          ║
║  设备: {DEVICE_ID}                          ║
║  Broker: {BROKER_HOST}:{BROKER_PORT}                    ║
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
""")

    try:
        choice = input("选择: ").strip()
    except (EOFError, KeyboardInterrupt):
        return

    if choice == "0":
        return
    elif choice == "1":
        ch = int(input("通道 (0-7): ").strip() or "0")
        st = int(input("状态 (0/1): ").strip() or "1")
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_mos_single(client, ch, st)
        finally:
            client.disconnect()
    elif choice == "2":
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_mos_all(client, 1)
        finally:
            client.disconnect()
    elif choice == "3":
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_mos_all(client, 0)
        finally:
            client.disconnect()
    elif choice == "4":
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_mos_query(client)
        finally:
            client.disconnect()
    elif choice == "5":
        ver = input("版本号 (如 1.0.30): ").strip() or "1.0.30"
        url = input("固件 URL: ").strip() or f"http://{BROKER_HOST}:8000/sample_project.bin"
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_ota(client, ver, url)
        finally:
            client.disconnect()
    elif choice == "6":
        client = DeviceClient(BROKER_HOST, BROKER_PORT, BROKER_USER, BROKER_PASS).connect()
        try:
            cmd_listen(client)
        finally:
            client.disconnect()
    elif choice == "7":
        cmd_provision_response(BROKER_HOST, PROVISION_PORT, BROKER_HOST, BROKER_PORT)


# ==================== 命令行入口 ====================

def main():
    global DEVICE_ID
    parser = argparse.ArgumentParser(description="ESP32 Smart Home MQTT 测试")
    parser.add_argument("--host", default=BROKER_HOST)
    parser.add_argument("--port", type=int, default=BROKER_PORT)
    parser.add_argument("--user", default=BROKER_USER)
    parser.add_argument("--pass", dest="password", default=BROKER_PASS)
    parser.add_argument("--dev", default=DEVICE_ID)

    # 直接执行模式
    parser.add_argument("--mos", nargs=2, metavar=("CHANNEL", "STATE"),
                        help="MOS 单路控制: channel state")
    parser.add_argument("--mos-all", type=int, metavar="STATE",
                        help="MOS 全部控制: 0 或 1")
    parser.add_argument("--mos-query", action="store_true",
                        help="查询 MOS 状态")
    parser.add_argument("--ota", nargs=2, metavar=("VERSION", "URL"),
                        help="OTA 升级: version url")
    parser.add_argument("--listen", type=int, nargs="?", const=15, metavar="SECONDS",
                        help="监听所有 topic（默认15秒）")
    parser.add_argument("--provision", nargs="?", default=None,
                        help="模拟注册响应（连接到 broker 1884）")

    args = parser.parse_args()

    # 切换 DEVICE_ID
    DEVICE_ID = args.dev

    # 命令行直接执行
    if args.provision is not None:
        cmd_provision_response(args.host, PROVISION_PORT, args.host, args.port)
        return

    if any([args.mos, args.mos_all is not None, args.mos_query, args.ota, args.listen]):
        client = DeviceClient(args.host, args.port, args.user, args.password).connect()
        try:
            if args.mos:
                cmd_mos_single(client, int(args.mos[0]), int(args.mos[1]))
            if args.mos_all is not None:
                cmd_mos_all(client, args.mos_all)
            if args.mos_query:
                cmd_mos_query(client)
            if args.ota:
                cmd_ota(client, args.ota[0], args.ota[1])
            if args.listen is not None:
                cmd_listen(client, args.listen)
        finally:
            client.disconnect()
        return

    # 交互菜单
    interactive_menu()


if __name__ == "__main__":
    main()
