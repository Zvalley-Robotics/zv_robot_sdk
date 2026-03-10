import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

LOW_STATE_TOPIC = "rt/all_joint_state"


def Handler(joint_state_msg: zv.AllJointState_):
    timestamp = joint_state_msg.timestamp_
    index = joint_state_msg.index_
    joint_states = joint_state_msg.joint_states_
    print(f"[Subscriber] Message received msg: {index}, timestamp: {timestamp}")
    for i in range(len(joint_states)):
        print(f"    joint {i} positions : {joint_states[i].joint_pos_}")


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_AllJointState_(LOW_STATE_TOPIC)
subscriber.InitChannel(Handler)
while True:
    time.sleep(10)
