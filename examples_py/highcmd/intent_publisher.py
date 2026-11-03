# Resolve the packaged SDK and this Python example's connection options.
import pathlib as _sdk_pathlib
import sys
sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs/python"))
sys.path.append(str(_sdk_pathlib.Path(__file__).resolve().parents[1]))

import signal
import threading
import time

import dds_options


def main():
    parser = dds_options.parser("Publish RobotIntent on rt/robot/input/intent")
    parser.add_argument("--source", default="sdk")
    parser.add_argument("--priority", type=int, default=50)
    parser.add_argument("--intent_type", default="status")
    parser.add_argument("--command", default="ready")
    parser.add_argument("--action", default="")
    parser.add_argument("--linear_x_mps", type=float, default=0)
    parser.add_argument("--linear_y_mps", type=float, default=0)
    parser.add_argument("--angular_z_radps", type=float, default=0)
    parser.add_argument("--count", type=int, default=1, help="0 publishes until stopped")
    parser.add_argument("--period_ms", type=int, default=50)
    if len(sys.argv) == 1:
        parser.print_help()
        return 0
    args = parser.parse_args()
    try:
        dds_options.validate(args)
        if not 0 <= args.count <= 1000000 or not 10 <= args.period_ms <= 60000:
            raise ValueError("invalid count or period_ms")
        if not 0 <= args.priority <= 255:
            raise ValueError("priority must be an integer in 0..255")
        if not all(abs(v) <= 3.4028234663852886e38
                   for v in (args.linear_x_mps, args.linear_y_mps, args.angular_z_radps)):
            raise ValueError("velocities must be finite float32 values")
    except (ValueError, OverflowError) as error:
        parser.error(str(error))

    import zv_robot_sdk_python as zv
    intent = zv.RobotIntent_()
    intent.source_ = args.source
    intent.priority_ = args.priority
    intent.intent_type_ = args.intent_type
    intent.command_ = args.command
    intent.action_ = args.action
    intent.linear_x_mps_ = args.linear_x_mps
    intent.linear_y_mps_ = args.linear_y_mps
    intent.angular_z_radps_ = args.angular_z_radps
    dds_options.init(zv, args)
    publisher = zv.ChannelPublisher_RobotIntent_("rt/robot/input/intent")
    publisher.InitChannel()
    stopped = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())
    def stamp():
        now = time.time_ns()
        header = intent.header_
        timestamp = header.stamp_
        timestamp.sec_ = now // 1_000_000_000
        timestamp.nanosec_ = now % 1_000_000_000
        header.stamp_ = timestamp
        intent.header_ = header
    try:
        stopped.wait(0.5)
        sent = 0
        while not stopped.is_set() and (args.count == 0 or sent < args.count):
            stamp()
            publisher.Write(intent)
            sent += 1
            print(f"Published rt/robot/input/intent count={sent}", flush=True)
            if args.count == 0 or sent < args.count:
                stopped.wait(args.period_ms / 1000)
        if (args.count == 0 or stopped.is_set()) and intent.intent_type_ == "motion":
            intent.linear_x_mps_ = intent.linear_y_mps_ = intent.angular_z_radps_ = 0
            stamp()
            publisher.Write(intent)
            print("Stopped; sent zero velocity on rt/robot/input/intent", flush=True)
        time.sleep(0.1)
    finally:
        publisher.CloseChannel()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
