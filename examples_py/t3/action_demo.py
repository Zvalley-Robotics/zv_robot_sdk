"""T3 动作 CLI；参数与 C++ t3ActionDemo 一致，默认仅演练。"""

# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import math
import signal
import sys
import threading
import time


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--actuate", action="store_true")
    commands = parser.add_mutually_exclusive_group(required=True)
    for name in ("cmd_vel", "head", "waist", "arm", "release"):
        commands.add_argument("--" + name)
    commands.add_argument(
        "--arm_pose", metavar="SIDE x y z rx ry rz",
        help="手臂目标位姿，各臂当前工作坐标系；xyz 米，rx/ry/rz 弧度；"
             "left/right 后跟 6 值，both 后跟左臂 6 值再右臂 6 值")
    args = parser.parse_args(argv)
    args.command = next(name for name in ("cmd_vel", "head", "waist", "arm", "arm_pose", "release")
                        if getattr(args, name) is not None)
    value = getattr(args, args.command).strip().strip("\"'")
    if args.command == "release":
        if value not in ("all", "head", "waist", "arm", "cmd_vel"):
            parser.error("--release 需为 all/head/waist/arm/cmd_vel")
        args.release = value
        return args
    parts = value.split()
    if args.command in ("arm", "arm_pose"):
        if not parts or parts[0] not in ("left", "right", "both"):
            parser.error(f"--{args.command} 侧别需为 left/right/both")
        args.side, parts = parts[0], parts[1:]
    expected = ({"cmd_vel": 3, "head": 2, "waist": 4, "arm": 7}[args.command]
                if args.command != "arm_pose" else (12 if args.side == "both" else 6))
    try:
        args.values = [float(part) for part in parts]
    except ValueError:
        parser.error("参数必须为数值")
    if len(args.values) != expected or not all(math.isfinite(v) for v in args.values):
        parser.error(f"需要 {expected} 个有限数值")
    if args.command == "arm_pose" and any(abs(v) > 3.4028234663852886e38 for v in args.values):
        parser.error("位姿必须为有限 float32 数值")
    return args


def twist(zv, values):
    msg = zv.Twist_()
    linear, angular = zv.Vector3_(), zv.Vector3_()
    linear.x_, linear.y_ = values[:2]
    angular.z_ = values[2]
    msg.linear_, msg.angular_ = linear, angular
    return msg


def arm_header(zv, msg):
    now = time.time_ns()
    stamp = zv.Time_()
    stamp.sec_, stamp.nanosec_ = divmod(now, 1_000_000_000)
    header = zv.ZVHeader_()
    header.time_, header.trace_id_ = stamp, "t3_action_demo"
    msg.zv_header_ = header


def make_commands(zv, args):
    """返回 (消息类型名, topic, 消息, 突发帧数)。不创建 DDS 实体。"""
    command = args.command
    if command == "arm_pose":
        msg = zv.ArmAutoMsg_()
        arm_header(zv, msg)
        msg.is_enable_ = 1
        msg.arm_index_ = {"left": 1, "right": 2, "both": 3}[args.side]
        arm_count = 2 if args.side == "both" else 1
        msg.eetype_, msg.grip_status_ = [0] * arm_count, [255] * arm_count
        # 仅设置目标位姿；关节数组为空，方向增量字段为零。
        if args.side in ("left", "both"):
            msg.left_pose_ = args.values[:6]
        if args.side in ("right", "both"):
            msg.right_pose_ = args.values[6:] if args.side == "both" else args.values
        return [("ArmAutoMsg_", "rt/remote_control/arm_auto_msg", msg, 20)]
    if command == "cmd_vel":
        return [("Twist_", "rt/cmd_vel", twist(zv, args.values), 1)]
    names = ("cmd_vel", "head", "waist", "arm") if command == "release" and args.release == "all" else (
        args.release if command == "release" else command,)
    commands = []
    for name in names:
        if name == "cmd_vel":
            commands.append(("Twist_", "rt/cmd_vel", twist(zv, (0, 0, 0)), 1))
            continue
        typename = {"head": "HeadAutoMsg_", "waist": "WaistAutoMsg_", "arm": "ArmAutoMsg_"}[name]
        msg = getattr(zv, typename)()
        msg.is_enable_ = int(command != "release")
        if name == "arm":
            arm_header(zv, msg)
        if command != "release":
            if name == "head":
                msg.is_absolute_, msg.do_ready_ = 1, 0
                msg.pitch_, msg.yaw_ = args.values
            elif name == "waist":
                msg.is_absolute_, msg.do_ready_, msg.is_joint_ = 0, 0, 0
                msg.x_, msg.z_, msg.ry_, msg.rz_ = args.values
            else:
                msg.arm_index_ = {"left": 1, "right": 2, "both": 3}[args.side]
                msg.eetype_, msg.grip_status_ = [0], [255]
                if args.side in ("left", "both"):
                    msg.left_joints_ = args.values
                if args.side in ("right", "both"):
                    msg.right_joints_ = args.values
        commands.append((typename, f"rt/remote_control/{name}_auto_msg", msg, 20))
    return commands


def run(args, stopped):
    continuous = args.command == "cmd_vel" and any(args.values)
    label = "实际下发" if args.actuate else "演练，不下发"
    print(f"[{args.command}] {getattr(args, args.command)} ({label})", flush=True)
    if not args.actuate:
        print("演练模式：仅打印指令，不实际下发。", flush=True)
        while continuous and not stopped.wait(1):
            print("[cmd_vel] 运行中（演练）", flush=True)
        return 0

    import zv_robot_sdk_python as zv

    zv.ChannelFactory.Instance().Init(0)
    commands = make_commands(zv, args)
    for typename, topic, message, frames in commands:
        if stopped.is_set():
            break
        publisher = getattr(zv, "ChannelPublisher_" + typename)(topic)
        publisher.InitChannel()
        try:
            if continuous:
                tick = 0
                try:
                    while not stopped.is_set():
                        publisher.Write(message)
                        tick += 1
                        if tick % 20 == 0:
                            print(f"[cmd_vel] 运行中 {tick // 20}s", flush=True)
                        stopped.wait(0.05)
                finally:
                    publisher.Write(twist(zv, (0, 0, 0)))
                    print("[cmd_vel] 停止，已下发零速", flush=True)
            else:
                for _ in range(frames):
                    publisher.Write(message)
                    if stopped.wait(0.05):
                        break
        finally:
            publisher.CloseChannel()
    print(f"[{args.command}] 完成", flush=True)
    return 0


def main():
    args = parse_args()
    stopped = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())
    try:
        return run(args, stopped)
    except (ImportError, AttributeError, RuntimeError) as exc:
        print(f"T3 Python SDK 执行失败（需包含 T3 绑定的 SDK）: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
