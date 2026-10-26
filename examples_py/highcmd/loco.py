# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
# 显式 PYTHONPATH 的 SDK 优先；未配置时仍可使用包内 SDK。
_sdk_sys.path.append(str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import math
import json
import signal
import struct
import sys
import time

import zv_robot_sdk_python as zv

MOVE_PUBLISH_PERIOD = 0.05  # 秒

SUPPORTED_STATES = {"ready", "walk", "damp", "reinit"}

keep_running = True


def handle_signal(signum, frame):
    global keep_running
    keep_running = False


HELP_TEXT = """Usage: loco [--network_interface=<iface>] <command[=args]>

查询命令:
  --robot_state                    查询机器人当前运动状态、模型
  --supported_models               查询 zv_apid 当前支持的动作模型
  --head_pose                      查询头部位置（米）与姿态四元数
  --version                        查询机器人版本号、发布日期

控制命令:
  --state=<state>                  设置运动状态
  --move=<vx vy vyaw>              连续移动 (速度m/s)
  --model=<model>                  切换动作模型（由 zv_apid 校验）
  --reset_fault                    单次提交故障复位意图（实际结果查看机器人状态）
  --config=<command action>        单次切换配置：getup 姿态、nav manual/auto/follow、speed std/high、mode rl/mix/app
  --head=<yaw pitch>               单次下发头部角度（度，-180..180）
  --relative                       与 --head 配合使用，表示相对角度；默认绝对角度

动作命令:
  --stop                          暂停当前动作（模型）
  --resume                        恢复当前动作（模型）
  --finish_loop                    当前循环动作播完后回到行走模型

示例:
  loco --robot_state
  loco --supported_models
  loco --state="ready"
  loco --reset_fault
  loco --config="getup stand"
  loco --config="nav auto"
  loco --config="speed high"
  loco --config="mode mix"
  loco --head="30 15"
  loco --head="10 5" --relative
  loco --head_pose
  loco --version
  loco --move="0.3 0.0 0.0"
  loco --stop
  loco --resume
  loco --model="t4_walk"
"""


def strip_quotes(value):
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return value[1:-1]
    return value


def set_command(options, command, value, error):
    if options["command"]:
        error.append("一次只能执行一个命令")
        return False
    options["command"] = command
    options["value"] = value
    return True


def parse_arguments(argv, options, error):
    for argument in argv[1:]:
        if not argument.startswith("--"):
            error.append(f"无效参数: {argument}")
            return False

        separator = argument.find("=")
        if separator == -1:
            key = argument[2:]
            value = ""
        else:
            key = argument[2:separator]
            value = strip_quotes(argument[separator + 1 :])

        if key == "help":
            return set_command(options, "help", "", error)
        if key == "network_interface":
            if not value:
                error.append("--network_interface 不能为空")
                return False
            options["networkInterface"] = value
            continue
        if key == "relative":
            if value:
                error.append("--relative 不接受参数")
                return False
            options["relative"] = True
            continue
        if key in ("robot_state", "head_pose", "version"):
            if value:
                error.append(f"--{key} 不接受参数")
                return False
            if not set_command(options, key, "", error):
                return False
            continue
        if key == "supported_models":
            if value:
                error.append("--supported_models 不接受参数")
                return False
            if not set_command(options, "supported_models", "", error):
                return False
            continue
        if key in ("finish_loop", "stop", "resume", "reset_fault"):
            if value:
                error.append(f"--{key} 不接受参数")
                return False
            if not set_command(options, key, "", error):
                return False
            continue
        if key in ("state", "move", "model", "config", "head"):
            if not value:
                error.append(f"--{key} 需要参数")
                return False
            if not set_command(options, key, value, error):
                return False
            continue
        error.append(f"未知命令: --{key}")
        return False

    return True


def parse_move(value):
    parts = value.split()
    if len(parts) != 3:
        return None
    try:
        # Move 在协议中使用 float32，与 C++ 示例保持相同的速度精度。
        values = struct.unpack("fff", struct.pack("fff", *(float(p) for p in parts)))
        if not all(math.isfinite(v) for v in values):
            return None
        return values
    except (ValueError, OverflowError, struct.error):
        return None


def run_move(client, vx, vy, vyaw):
    global keep_running
    stop_command = vx == 0.0 and vy == 0.0 and vyaw == 0.0
    if not stop_command:
        print("连续移动中，按 Ctrl+C 停止。")
        signal.signal(signal.SIGINT, handle_signal)
        signal.signal(signal.SIGTERM, handle_signal)

    while True:
        result = client.Move(vx, vy, vyaw)
        if result != 0:
            print(f"Move failed with code: {result}", file=sys.stderr)
            return 1
        if stop_command:
            print("停止命令已发送。")
            return 0
        if not keep_running:
            break
        time.sleep(MOVE_PUBLISH_PERIOD)

    stop_result = client.Move(0.0, 0.0, 0.0)
    if stop_result != 0:
        print(f"Stop failed with code: {stop_result}", file=sys.stderr)
        return 1
    print("运动已停止。")
    return 0


def main(argv):
    if len(argv) == 1:
        print(HELP_TEXT)
        return 0

    options = {"networkInterface": "lo", "command": "", "value": "", "relative": False}
    error = []
    if not parse_arguments(argv, options, error):
        print(error[0], file=sys.stderr)
        print(HELP_TEXT)
        return 1
    if options["relative"] and options["command"] != "head":
        print("--relative 仅适用于 --head", file=sys.stderr)
        return 1
    if not options["command"] or options["command"] == "help":
        print(HELP_TEXT)
        return 0

    if options["command"] == "state" and options["value"] not in SUPPORTED_STATES:
        print(f"不支持的运动状态: {options['value']}", file=sys.stderr)
        return 1

    vx = vy = vyaw = 0.0
    if options["command"] == "move":
        parsed = parse_move(options["value"])
        if parsed is None:
            print(f"--{options['command']} 需要且仅接受三个有限浮点数: vx vy vyaw", file=sys.stderr)
            return 1
        vx, vy, vyaw = parsed

    config_args = options["value"].split() if options["command"] == "config" else []
    if options["command"] == "config" and len(config_args) != 2:
        print("--config 需要且仅接受两个参数: command action", file=sys.stderr)
        return 1

    yaw = pitch = 0.0
    if options["command"] == "head":
        try:
            parts = options["value"].split()
            if len(parts) != 2:
                raise ValueError()
            yaw, pitch = struct.unpack("ff", struct.pack("ff", *(float(v) for v in parts)))
            if not all(math.isfinite(v) and abs(v) <= 180 for v in (yaw, pitch)):
                raise ValueError()
        except (ValueError, OverflowError, struct.error):
            print("--head 需要两个 -180..180 度内的有限角度: yaw pitch", file=sys.stderr)
            return 1

    zv.ChannelFactory.Instance().Init(0, options["networkInterface"])

    client = zv.LocoClient()
    client.Init()
    client.SetTimeout(10.0)

    if options["command"] == "head":
        result = client.HeadControl(yaw, pitch, options["relative"])
        if result != 0:
            print(f"HeadControl failed with code: {result}", file=sys.stderr)
            return 1
        print("head control command accepted")
        return 0
    if options["command"] == "head_pose":
        pose = zv.LocoHeadPose()
        result = client.GetHeadPose(pose)
        if result != 0:
            print(f"GetHeadPose failed with code: {result}", file=sys.stderr)
            return 1
        print(json.dumps({"head_pose": {"position": list(pose.position),
                         "orientation": list(pose.orientation), "age": pose.age}}))
        return 0
    if options["command"] == "version":
        version = zv.LocoVersion()
        result = client.GetRobotVersion(version)
        if result != 0:
            print(f"GetRobotVersion failed with code: {result}", file=sys.stderr)
            return 1
        print(json.dumps({"robot_version": {"version": version.version,
                         "release_date": version.releaseDate, "age": version.age}}))
        return 0

    if options["command"] == "config":
        result = client.SetConfig(*config_args)
        if result != 0:
            print(f"SetConfig failed with code: {result}", file=sys.stderr)
            return 1
        print(f"config command accepted: {' '.join(config_args)}")
        return 0

    if options["command"] == "reset_fault":
        result = client.ResetFault()
        if result != 0:
            print(f"ResetFault failed with code: {result}", file=sys.stderr)
            return 1
        print("reset fault command accepted; check robot state for the result")
        return 0

    if options["command"] in ("stop", "resume"):
        method = client.PauseMotion if options["command"] == "stop" else client.ResumeMotion
        result = method()
        if result != 0:
            print(f"MotionStop failed with code: {result}", file=sys.stderr)
            return 1
        print(f"motion {options['command']} command accepted")
        return 0

    if options["command"] == "robot_state":
        state = zv.LocoState()
        result = client.GetRobotState(state)
        if result != 0:
            print(f"GetRobotState failed with code: {result}", file=sys.stderr)
            return 1
        print(f"robot state: {state.stateName} ({state.state})")
        print(f"active model: {state.activeModel}")
        return 0

    if options["command"] == "supported_models":
        models = []
        result = client.GetSupportedModels(models)
        if result != 0:
            print(f"GetSupportedModels failed with code: {result}", file=sys.stderr)
            return 1
        print({"supported_models": models})
        return 0

    if options["command"] == "state":
        result = client.SetState(options["value"])
        if result != 0:
            print(f"SetState failed with code: {result}", file=sys.stderr)
            return 1
        print(f"state command accepted: {options['value']}")
        return 0

    if options["command"] == "model":
        result = client.SetModel(options["value"])
        if result != 0:
            print(f"SetModel failed with code: {result}", file=sys.stderr)
            return 1
        print(f"model command accepted: {options['value']}")
        return 0

    if options["command"] == "finish_loop":
        result = client.FinishLoop()
        if result != 0:
            print(f"FinishLoop failed with code: {result}", file=sys.stderr)
            return 1
        print("finish loop command accepted")
        return 0

    return run_move(client, vx, vy, vyaw)


if __name__ == "__main__":
    sys.exit(main(sys.argv))
