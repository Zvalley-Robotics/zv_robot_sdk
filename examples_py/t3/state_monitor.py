"""T3 状态监控；与 C++ t3StateMonitor 使用相同的话题名和输出字段。"""

# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import math
import signal
import sys
import threading


CATALOG = (
    ("left_arm_state", "手臂", "ArmState_"),
    ("left_force", "手臂", "ArmForce_"),
    ("left_gripper", "手臂", "Gripper_"),
    ("left_joint_status", "手臂", "JointStatus_"),
    ("right_arm_state", "手臂", "ArmState_"),
    ("right_force", "手臂", "ArmForce_"),
    ("right_gripper", "手臂", "Gripper_"),
    ("right_joint_status", "手臂", "JointStatus_"),
    ("head_motor_states", "头部", "MotorStates_"),
    ("waist_end", "腰部", "WaistEnd_"),
    ("waist_motor_states", "腰部", "MotorStates_"),
)


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, allow_abbrev=False)
    parser.add_argument("--topics", default=None)
    parser.add_argument("--hz", default="1")
    args = parser.parse_args(argv)
    try:
        args.hz = float(args.hz)
        if not math.isfinite(args.hz) or args.hz <= 0:
            raise ValueError()
    except ValueError:
        print("警告: 非法 --hz 取值，回退为 1Hz", flush=True)
        args.hz = 1.0
    requested = None if args.topics is None else set(filter(None, (v.strip() for v in args.topics.split(","))))
    args.selected = [entry for entry in CATALOG if requested is None or entry[0] in requested]
    for name in sorted((requested or set()) - {entry[0] for entry in CATALOG}):
        print(f"警告: 未知监控话题 '{name}'，已忽略", flush=True)
    if not args.selected:
        parser.error("没有可监控的话题")
    return args


def join(values):
    return " ".join(f"{value:g}" for value in values)


def format_message(name, count, msg):
    if not count:
        return f"[{name}] 等待数据..."
    prefix = f"[{name}] count={count}"
    if name.endswith("arm_state"):
        pos, euler = msg.pose_.position_, msg.euler_
        return (f"{prefix} | joint: {join(msg.joint_[:7])}"
                f" | pos: {pos.x_:g} {pos.y_:g} {pos.z_:g}"
                f" | euler: {euler.x_:g} {euler.y_:g} {euler.z_:g}"
                f" | arm_err={msg.arm_err_} sys_err={msg.sys_err_} dof={msg.dof_}")
    if name.endswith("force"):
        return f"{prefix} | force: {join(msg.force_[:6])}"
    if name.endswith("gripper"):
        text = (f"{prefix} | eetype={msg.eetype_} enable={msg.enable_state_}"
                f" status={msg.status_} error={msg.error_} temp={msg.temperature_}")
        if msg.eetype_ in (0, 2):
            hand = msg.gripper_2finger_
            text += f" | 2finger: mode={hand.mode_} force={hand.current_force_} actpos={hand.actpos_}"
        elif msg.eetype_ == 4:
            text += f" | sucker: voltage={msg.sucker_.voltage_} is_sucking={int(msg.sucker_.is_sucking_)}"
        elif msg.eetype_ in (1, 3):
            text += f" | hand: sys_state={msg.hand_.sys_state_} pos[0-3]: {join(msg.hand_.pos_[:4])}"
        return text
    if name.endswith("joint_status"):
        enabled = "".join(str(int(v)) for v in msg.joint_en_flag_)
        return (f"{prefix} | position: {join(msg.joint_position_)}"
                f" | temp: {join(msg.joint_temperature_)} | en: {enabled}"
                f" | err: {join(msg.joint_err_code_)} | force: {join(msg.force_)}"
                f" | plus_state={msg.plus_state_}")
    if name.endswith("motor_states"):
        lines = [f"{prefix} motor_class={msg.motor_class_} motor_num={len(msg.motor_states_)}"]
        for motor in msg.motor_states_:
            lines.append(f"    motor_id={motor.motor_id_} name={motor.name_}"
                         f" position={motor.position_:g} velocity={motor.velocity_:g}"
                         f" effort={motor.effort_:g} angle={motor.angle_:g}"
                         f" enabled={int(motor.enabled_)} fault={int(motor.fault_)}"
                         f" error_code={motor.error_code_}")
        return "\n".join(lines)
    return f"{prefix} | x={msg.x_:g} z={msg.z_:g} ry={msg.ry_:g} rz={msg.rz_:g}"


def run(args, stopped):
    import zv_robot_sdk_python as zv

    zv.ChannelFactory.Instance().Init(0)
    subscribers = []
    try:
        for name, section, typename in args.selected:
            subscriber = getattr(zv, "LatestSubscriber_" + typename)("rt/" + name)
            subscribers.append((name, section, subscriber))
            subscriber.InitChannel()
        print(f"t3 状态监控已启动，订阅 {len(subscribers)} 个话题: "
              + " ".join(item[0] for item in subscribers)
              + f"，以 {args.hz:g} Hz 打印汇总...", flush=True)
        tick = 0
        while not stopped.wait(1 / args.hz):
            tick += 1
            print(f"\n================ T3 State Monitor tick={tick} ================", flush=True)
            previous = None
            for name, section, subscriber in subscribers:
                if section != previous:
                    print(f"--- {section} ---", flush=True)
                    previous = section
                print(format_message(name, *subscriber.ReadLatest()), flush=True)
    finally:
        for _, _, subscriber in reversed(subscribers):
            subscriber.CloseChannel()
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
