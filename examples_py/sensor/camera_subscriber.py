# Resolve this example and its SDK independently of the C++ examples.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs/python"))
_sdk_sys.path.append(str(_sdk_pathlib.Path(__file__).resolve().parents[1]))

import dds_options
import time
import zv_robot_sdk_python as zv


def main():
    args = dds_options.parser(__doc__).parse_args()
    dds_options.validate(args)
    # 1. 创建相机订阅器，并绑定相机名称与完整 DDS 话题
    subscriber = zv.CameraSubscriber()
    topics = {"head": "rt/media/camera/head/color/image_raw/compressed"}
    if args.use_config:
        subscriber.Init(str(_sdk_pathlib.Path(__file__).resolve().parents[1] / "config/dds.json"), topics)
    else:
        subscriber.Init(0, args.network_interface, topics)
    # 2. 轮询最新压缩图像，TakeFrame 返回是否有可用帧及对应数据
    while True:
        available, frame = subscriber.TakeFrame("head")
        if available:
            # 3. 输出图像帧标识、压缩格式和数据长度
            print(frame.frame_id, frame.format, len(frame.data))
        # 4. 每 100ms 轮询一次，避免空轮询占用 CPU
        time.sleep(0.1)


if __name__ == "__main__":
    main()
