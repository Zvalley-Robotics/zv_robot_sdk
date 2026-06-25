import os
import sys

sys.path.insert(
    0,
    os.path.join(
        os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
        "libs",
        "python",
    ),
)

import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

LOW_STATE_TOPIC = "rt/all_joint_state"


def Handler(joint_state_msg: zv.AllJointState_):
    timestamp = joint_state_msg.timestamp_
    index = joint_state_msg.index_
    used_for_ctrl = joint_state_msg.used_for_ctrl_
    joint_states = joint_state_msg.joint_states_
    print(
        f"[Subscriber] Message received msg: {index}, timestamp: {timestamp}, "
        f"used_for_ctrl: {used_for_ctrl}"
    )
    for i in range(len(joint_states)):
        js = joint_states[i]
        print(
            f"    joint {i} pos: {js.joint_pos_}, model_id: {js.model_id_}, "
            f"motor_id: {js.motor_id_}, joint_error: {js.joint_error_}"
        )


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_AllJointState_(LOW_STATE_TOPIC)
subscriber.InitChannel(Handler)
while True:
    time.sleep(10)
