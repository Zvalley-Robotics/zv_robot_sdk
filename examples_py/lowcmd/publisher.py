import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

LOW_CMD_TOPIC = "rt/all_joint_cmd"

zv.ChannelFactory.Instance().Init(0)
publisher = zv.ChannelPublisher_AllJointCmd_(LOW_CMD_TOPIC)
publisher.InitChannel()

while True:
    msg = zv.AllJointCmd_()
    v_msg_joint: list[zv.JointCmd_] = []

    for i in range(23):
        msg_joint = zv.JointCmd_()
        msg_joint.joint_pos_ = 0.11111 * i / 2
        msg_joint.joint_vel_ = 0.22222 * i / 3
        msg_joint.joint_torque_ = 0.33333 * i / 4
        msg_joint.kp_ = 0.444 * i / 4
        msg_joint.kd_ = 0.55555 * i / 7 * 1.1
        v_msg_joint.append(msg_joint)

    publisher.Write(msg)

    print("test [tx] ")
    time.sleep(1)
