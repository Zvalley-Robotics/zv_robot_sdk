import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

LOW_CMD_TOPIC = "rt/all_joint_cmd"

zv.ChannelFactory.Instance().Init(0)
publisher = zv.ChannelPublisher_AllJointCmd_(LOW_CMD_TOPIC)
publisher.InitChannel()

while True:
    msg = zv.AllJointCmd_()

    joint_pos = [
        -0.07279034703969955,
        -0.016103055328130722,
        -0.03257334977388382,
        0.17935675382614136,
        -0.0654093474149704,
        0.04607636481523514,
        -0.03722424805164337,
        -0.0016681907000020146,
        0.019157331436872482,
        0.16593047976493835,
        -0.04944632947444916,
        -0.0660635381937027,
        0.22476732730865479,
        0.1516973376274109,
        -0.004047076217830181,
        -0.4731024205684662,
        -0.046716321259737015,
        0.21820136904716492,
        -0.14873942732810974,
        0.010256296023726463,
        -0.4458266794681549,
        0.05600139498710632,
        0.022615695372223854,
    ]

    kp = [
        100.0,
        100.0,
        100.0,
        150.0,
        10.0,
        10.0,
        100.0,
        100.0,
        100.0,
        150.0,
        10.0,
        10.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        40.0,
        200.0,
    ]

    kd = [
        2.0,
        2.0,
        2.0,
        4.0,
        0.5,
        0.5,
        2.0,
        2.0,
        2.0,
        4.0,
        0.5,
        0.5,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        1.0,
        5.0,
    ]

    cmds = msg.joint_cmds_
    cmds.clear()
    for i in range(23):
        cmds.append(zv.JointCmd_())
        c = cmds[-1]
        c.joint_pos_ = joint_pos[i]
        c.joint_vel_ = 0
        c.joint_torque_ = 0
        c.kp_ = kp[i]
        c.kd_ = kd[i]

    publisher.Write(msg)

    print("test [tx] ")
    time.sleep(1)
