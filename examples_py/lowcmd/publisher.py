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
    parser = dds_options.parser("Publish HeadControl or Twist DDS messages")
    parser.add_argument("--topic", choices=("head", "velocity"), default="head")
    parser.add_argument("--yaw", type=float, default=0)
    parser.add_argument("--pitch", type=float, default=0)
    parser.add_argument("--is_relative", type=int, choices=(0, 1), default=0)
    parser.add_argument("--linear_x", type=float, default=0)
    parser.add_argument("--linear_y", type=float, default=0)
    parser.add_argument("--linear_z", type=float, default=0)
    parser.add_argument("--angular_x", type=float, default=0)
    parser.add_argument("--angular_y", type=float, default=0)
    parser.add_argument("--angular_z", type=float, default=0)
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
        if args.topic == "head" and args.is_relative and args.count != 1:
            raise ValueError("relative head control requires count=1")
        if not all(abs(v) <= 3.4028234663852886e38 for v in (args.yaw, args.pitch)):
            raise ValueError("head angles must be finite float32 values")
    except (ValueError, OverflowError) as error:
        parser.error(str(error))

    import zv_robot_sdk_python as zv
    if args.topic == "head":
        topic = "rt/robot/external/head_control"
        message = zv.HeadControl_()
        message.yaw_ = args.yaw
        message.pitch_ = args.pitch
        message.is_relative_ = bool(args.is_relative)
        publisher = zv.ChannelPublisher_HeadControl_(topic)
    else:
        topic = "rt/cmd_vel"
        message = zv.Twist_()
        linear = zv.Vector3_()
        linear.x_ = args.linear_x
        linear.y_ = args.linear_y
        linear.z_ = args.linear_z
        message.linear_ = linear
        angular = zv.Vector3_()
        angular.x_ = args.angular_x
        angular.y_ = args.angular_y
        angular.z_ = args.angular_z
        message.angular_ = angular
        publisher = zv.ChannelPublisher_Twist_(topic)
    dds_options.init(zv, args)
    publisher.InitChannel()
    stopped = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())
    try:
        stopped.wait(0.5)
        sent = 0
        while not stopped.is_set() and (args.count == 0 or sent < args.count):
            publisher.Write(message)
            sent += 1
            print(f"Published {topic} count={sent}", flush=True)
            if args.count == 0 or sent < args.count:
                stopped.wait(args.period_ms / 1000)
        if args.topic == "velocity" and (args.count == 0 or stopped.is_set()):
            publisher.Write(zv.Twist_())
            print("Stopped; sent zero velocity on rt/cmd_vel", flush=True)
        time.sleep(0.1)
    finally:
        publisher.CloseChannel()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
