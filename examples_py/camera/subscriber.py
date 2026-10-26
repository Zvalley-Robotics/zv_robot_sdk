import time
import zv_robot_sdk_python as zv

# 1. 创建相机订阅器，并绑定相机名称与完整 DDS 话题
subscriber = zv.CameraSubscriber()
subscriber.Init(0, "", {"head": "rt/media/camera/head/color/image_raw/compressed"})
# 2. 轮询最新压缩图像，TakeFrame 返回是否有可用帧及对应数据
while True:
    available, frame = subscriber.TakeFrame("head")
    if available:
        # 3. 输出图像帧标识、压缩格式和数据长度
        print(frame.frame_id, frame.format, len(frame.data))
    # 4. 每 100ms 轮询一次，避免空轮询占用 CPU
    time.sleep(0.1)
