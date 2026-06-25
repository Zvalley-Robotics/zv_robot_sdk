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

NAV_ALL_TOPIC = "rt/nav_all"


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
subscriber.InitChannel(Handler)
while True:
    time.sleep(10)
