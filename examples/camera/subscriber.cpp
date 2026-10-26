#include <unistd.h>

#include <iostream>
#include <map>
#include <zv/robot/media/camera_subscriber.hpp>

int main()
{
    // 1. 配置相机名称与完整 DDS 话题的映射
    const std::map<std::string, std::string> topics{
        {"head",  "rt/media/camera/head/color/image_raw/compressed" }
    };

    // 2. 创建并初始化 CameraSubscriber，绑定相机名称与话题
    zv::robot::CameraSubscriber subscriber;
    subscriber.Init(0, "", topics);

    // 3. 持续轮询各相机的最新压缩图像
    zv::robot::CameraFrame frame;
    while (true) {
        for (const auto & [name, topic] : topics) {
            // TakeFrame 非阻塞返回该相机是否有新的可用帧。
            if (subscriber.TakeFrame(name, frame)) {
                // 4. 输出相机名称、图像帧标识、压缩格式和数据长度
                std::cout << name << ": " << frame.frame_id << ", " << frame.format << ", " << frame.data.size()
                          << " bytes" << std::endl;
            }
        }
        // 5. 每 100ms 轮询一次，避免空轮询占用 CPU
        usleep(100000);
    }

    return 0;
}
