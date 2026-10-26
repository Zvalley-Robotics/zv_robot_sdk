"""订阅 rt/robot/state；--json 按行输出完整状态，供 Web 工具读取。"""

import argparse
import json
import pathlib
import signal
import sys
import threading


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--domain_id", type=int, default=100)
    parser.add_argument("--network_interface", default="")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)
    if not 0 <= args.domain_id <= 232:
        parser.error("domain_id must be in 0..232")
    return args


def state_dict(state):
    return {name: getattr(state, name + "_") for name in (
        "state", "state_name", "active_mode", "active_control_source",
        "active_rl_model", "fault_active", "fault_reason")}


def main():
    args = parse_args()
    # 显式 PYTHONPATH 的 SDK 优先，其次使用包内 SDK。
    sys.path.append(str(pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))
    import zv_robot_sdk_python as zv

    stopped = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())

    def handler(state):
        data = state_dict(state)
        if args.json:
            print(json.dumps(data, ensure_ascii=False), flush=True)
        else:
            print(f"state: {data['state']}, state_name: {data['state_name']}, "
                  f"active_mode: {data['active_mode']}, active_rl_model: {data['active_rl_model']}",
                  flush=True)

    zv.ChannelFactory.Instance().Init(args.domain_id, args.network_interface)
    subscriber = zv.ChannelSubscriber_RobotState_("rt/robot/state")
    try:
        subscriber.InitChannel(handler, 5, False)
        stopped.wait()
    finally:
        subscriber.CloseChannel()


if __name__ == "__main__":
    main()
