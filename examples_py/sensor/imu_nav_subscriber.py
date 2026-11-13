# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import signal
import threading

_sdk_sys.path.append(str(_sdk_pathlib.Path(__file__).resolve().parents[1]))
import dds_options

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
    gyro = nav_msg.gyro_
    euler = nav_msg.euler_
    quat = nav_msg.quat_
    pos = nav_msg.pos_
    vel = nav_msg.vel_
    status = nav_msg.status_
    temp = nav_msg.temp_
    pressure = nav_msg.pressure_
    utc = nav_msg.utc_

    print(f"[Subscriber] NavAll received, tid: {tid}")
    print(f"    acc   [x, y, z]: {acc.x_}, {acc.y_}, {acc.z_}")
    print(f"    gyro  [x, y, z]: {gyro.x_}, {gyro.y_}, {gyro.z_}")
    print(f"    euler [pitch, roll, yaw]: {euler.pitch_}, {euler.roll_}, {euler.yaw_}")
    print(f"    quat  [q0, q1, q2, q3]: {quat.q0_}, {quat.q1_}, {quat.q2_}, {quat.q3_}")
    print(f"    gnss  [lon, lat, alt]: {pos.longitude_}, {pos.latitude_}, {pos.altitude_}")
    print(f"    vel   [e, n, u]: {vel.vel_e_}, {vel.vel_n_}, {vel.vel_u_}")
    print(
        f"    utc   [{utc.year_}-{utc.month_}-{utc.day_} "
        f"{utc.hour_}:{utc.min_}:{utc.sec_}.{utc.ms_}]"
    )
    print(f"    status [fusion, gnss]: {status.fusion_status_}, {status.gnss_status_}")
    print(f"    temp: {temp.temp_}")
    print(f"    pressure: {pressure.val_}")


def main():
    args = dds_options.parser(__doc__).parse_args()
    dds_options.init(zv, args)
    subscriber = zv.ChannelSubscriber_NavAll_(NAV_ALL_TOPIC)
    subscriber.InitChannel(Handler, NAV_ALL_QUEUE_LEN, NAV_ALL_RELIABLE)
    stopped = threading.Event()
    for sig in (signal.SIGINT, signal.SIGTERM):
        signal.signal(sig, lambda *_: stopped.set())
    try:
        stopped.wait()
    finally:
        subscriber.CloseChannel()


if __name__ == "__main__":
    main()
