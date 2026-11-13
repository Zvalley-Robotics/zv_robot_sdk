"""订阅 /robot/battery/battery_info（DDS: rt/robot/battery/battery_info）。"""

import argparse
import json
import pathlib
import signal
import sys
import threading
import time

BATTERY_TOPIC = "rt/robot/battery/battery_info"
BATTERY_FIELDS = (
    "total_volt", "current", "soc", "life", "power", "energy", "mos_temp", "board_temp",
    "heat_temp", "heat_current", "max_volt", "max_volt_no", "min_volt", "min_volt_no", "diff_volt",
    "max_temp", "max_temp_no", "min_temp", "min_temp_no", "diff_temp", "chg_mos", "dis_mos",
    "pre_mos", "heat_mos", "fan_mos", "bat_state", "chg_detect", "load_detect", "do_state", "di_state",
    "cell_num", "ntc_num", "remain_capacity", "cycle_count", "rest_charge_time", "wake_source",
    "limit_state", "limit_current", "soh", "pwm")
DDS_CONFIG_PATH = pathlib.Path(__file__).resolve().parents[1] / "config/dds.json"


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--network_interface", default="")
    parser.add_argument("--use_config", "--use-config", action="store_true")
    parser.add_argument("--json", action="store_true", help="逐行输出全部 40 个原始字段")
    parser.add_argument("--period_ms", "--period-ms", type=int, default=2000,
                        help="最短输出周期，默认 2000ms；0 表示每条消息，最大 60000ms")
    args = parser.parse_args(argv)
    if not 0 <= args.period_ms <= 60000:
        parser.error("period_ms must be between 0 and 60000")
    return args


def battery_dict(message):
    return {name:getattr(message, name+"_") for name in BATTERY_FIELDS}


def main(argv=None):
    args = parse_args(argv)
    # 显式 PYTHONPATH 优先，其次是制品内 SDK；帮助信息不加载 DDS。
    sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "libs/python"))
    import zv_robot_sdk_python as zv

    stopped = threading.Event()
    for sig in (signal.SIGINT,signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())

    output_lock = threading.Lock()
    last_output = None

    def print_message(message):
        raw = battery_dict(message)
        if args.json:
            print(json.dumps(raw, ensure_ascii=False), flush=True)
        else:
            temperature = raw['mos_temp'] if raw['mos_temp']<128 else raw['mos_temp']-256
            print(f"SOC: {raw['soc']/10:g} %, voltage: {raw['total_volt']/10:g} V, "
                  f"current: {raw['current']/10:g} A, power: {raw['power']} W, "
                  f"MOS temperature: {temperature} C, SOH: {raw['soh']/10:g} %, "
                  f"battery state: {raw['bat_state']}", flush=True)

    def handler(message):
        nonlocal last_output
        with output_lock:
            now = time.monotonic()
            if last_output is not None and now-last_output < args.period_ms/1000:
                return
            last_output = now
            print_message(message)

    if args.use_config:
        zv.ChannelFactory.Instance().Init(str(DDS_CONFIG_PATH))
    else:
        zv.ChannelFactory.Instance().Init(0,args.network_interface)
    subscriber = zv.ChannelSubscriber_BatteryInfo_(BATTERY_TOPIC)
    try:
        subscriber.InitChannel(handler,5,False)
        stopped.wait()
    finally:
        subscriber.CloseChannel()


if __name__=="__main__": main()
