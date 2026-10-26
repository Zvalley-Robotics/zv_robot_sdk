# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import time

import zv_robot_sdk_python as zv

print(f"zv_robot_sdk_python version: {zv.__version__}")

# 与 robot_external_bridge 的 /robot/imu/nav_all 对应（DDS 话题名 = rt + ROS2 路径）。
NAV_ALL_TOPIC = "rt/robot/imu/nav_all"
# 桥接配置：reliability=reliable, history=keep_last, depth=10。
NAV_ALL_QUEUE_LEN = 10
NAV_ALL_RELIABLE = True


def Handler(nav_msg: zv.NavAll_):
    # 嵌套子结构字段访问演示
    tid = nav_msg.tid_.tid_
    acc = nav_msg.acc_
    euler = nav_msg.euler_
    pos = nav_msg.pos_
    utc = nav_msg.utc_

    print(f"[Subscriber] NavAll received, tid: {tid}")
    print(f"    acc   [x, y, z]: {acc.x_}, {acc.y_}, {acc.z_}")
    print(f"    euler [pitch, roll, yaw]: {euler.pitch_}, {euler.roll_}, {euler.yaw_}")
    print(f"    gnss  [lon, lat, alt]: {pos.longitude_}, {pos.latitude_}, {pos.altitude_}")
    print(
        f"    utc   [{utc.year_}-{utc.month_}-{utc.day_} "
        f"{utc.hour_}:{utc.min_}:{utc.sec_}.{utc.ms_}]"
    )


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_NavAll_(NAV_ALL_TOPIC)
subscriber.InitChannel(Handler, NAV_ALL_QUEUE_LEN, NAV_ALL_RELIABLE)
while True:
    time.sleep(10)
