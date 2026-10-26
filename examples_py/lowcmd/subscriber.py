# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import time

import zv_robot_sdk_python as zv

print(f"zv_robot_sdk_python version: {zv.__version__}")

# 与 robot_external_bridge 的 /robot/driver/joint_states 对应（DDS 话题名 = rt + ROS2 路径）。
LOW_STATE_TOPIC = "rt/robot/driver/joint_states"
# 桥接配置：reliability=best_effort, history=keep_last, depth=5。
LOW_STATE_QUEUE_LEN = 5
LOW_STATE_RELIABLE = False


def Handler(joint_state_msg: zv.JointStates_):
    sequence = joint_state_msg.sequence_
    last_command_trace_id = joint_state_msg.last_command_trace_id_
    last_command_sequence = joint_state_msg.last_command_sequence_
    joint_states = joint_state_msg.states_
    print(
        f"[Subscriber] Message received sequence: {sequence}, "
        f"last command trace id: {last_command_trace_id}, "
        f"last command sequence: {last_command_sequence}"
    )
    for i in range(len(joint_states)):
        js = joint_states[i]
        print(
            f"    joint {i} name: {js.joint_name_}, position: {js.position_}, "
            f"velocity: {js.velocity_}, effort: {js.effort_}, "
            f"motor_error: {js.motor_error_}, joint_error: {js.joint_error_}, "
            f"error_message: {js.error_message_}"
        )


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_JointStates_(LOW_STATE_TOPIC)
subscriber.InitChannel(Handler, LOW_STATE_QUEUE_LEN, LOW_STATE_RELIABLE)
while True:
    time.sleep(10)
