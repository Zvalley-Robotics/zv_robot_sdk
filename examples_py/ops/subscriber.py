#!/usr/bin/env python3
"""订阅 ROS 2 聚合诊断，每行输出一帧完整 JSON 快照。"""

import json
from pathlib import Path
import signal
import sys
import time

# 显式 PYTHONPATH 的 SDK 优先；未配置时回退到包内 SDK。
sys.path.append(str(Path(__file__).resolve().parents[2] / "libs" / "python"))
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from dds_options import parser, validate
import zv_robot_sdk_python as zv


def main():
    args = parser("Subscribe /diagnostics_agg").parse_args()
    validate(args)
    subscriber = zv.DiagnosticsSubscriber()
    stopped = False

    def stop(_signum, _frame):
        nonlocal stopped
        stopped = True

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        if args.use_config:
            subscriber.Init(str(Path(__file__).resolve().parents[1] / "config/dds.json"))
        else:
            subscriber.Init(0, args.network_interface)
        while not stopped:
            available, message = subscriber.Take()
            if available:
                header = message.header_
                print(json.dumps({"header": {"stamp": {"sec": header.stamp_.sec_,
                    "nanosec": header.stamp_.nanosec_}, "frame_id": header.frame_id_},
                    "status": [{"level": status.level_, "name": status.name_,
                        "message": status.message_, "hardware_id": status.hardware_id_,
                        "values": [{"key": pair.key_, "value": pair.value_} for pair in status.values_]}
                        for status in message.status_]}, ensure_ascii=False), flush=True)
            time.sleep(0.1)
    finally:
        subscriber.Close()


if __name__ == "__main__":
    main()
