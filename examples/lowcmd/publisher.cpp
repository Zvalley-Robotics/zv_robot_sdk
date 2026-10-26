#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <zv/robot/channel/channel_publisher.hpp>

#include "HeadControl_.hpp"
#include "Twist_.hpp"

// DDS 话题名 = rt + ROS2 绝对路径。
static const std::string HEAD_CONTROL_TOPIC = "rt/robot/external/head_control";
static const std::string CMD_VEL_TOPIC = "rt/cmd_vel";
static constexpr auto PUBLISH_PERIOD = std::chrono::seconds(1);

static volatile std::sig_atomic_t keep_running = 1;

static void HandleSignal(int) { keep_running = 0; }

using namespace zv::robot;

int main()
{
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    // 1. 初始化 Channel 工厂（全局通信资源）
    ChannelFactory::Instance()->Init(0);

    // 2. 创建 Publisher，绑定 Topic 和消息类型
    ChannelPublisher<robot_interfaces::msg::dds_::HeadControl_> head_publisher(HEAD_CONTROL_TOPIC);
    ChannelPublisher<geometry_msgs::msg::dds_::Twist_> velocity_publisher(CMD_VEL_TOPIC);

    // 3. 初始化通信通道
    head_publisher.InitChannel();
    velocity_publisher.InitChannel();

    // 4. 构造 HeadControl_ 消息，设定头部控制参数
    // 按需修改目标角度（度）。持续发布使用绝对角度，避免相对角度累加。
    robot_interfaces::msg::dds_::HeadControl_ head;
    head.yaw_(0.0f);
    head.pitch_(0.0f);
    head.is_relative_(false);

    // 5. 构造 Twist_ 消息，设定底盘控制参数
    // 按需修改底盘速度：linear.x/y 为 m/s，angular.z 为 rad/s。
    // 其余分量保持零值。
    geometry_msgs::msg::dds_::Twist_ velocity;
    velocity.linear_().x_(0.0);
    velocity.linear_().y_(0.0);
    velocity.angular_().z_(0.0);

    std::cout << "Publishing " << HEAD_CONTROL_TOPIC << " and " << CMD_VEL_TOPIC << " at 1 Hz; press Ctrl+C to stop."
              << std::endl;

    auto next_publish_time = std::chrono::steady_clock::now();
    while (keep_running != 0) {
        // 6. 发布消息到 Topic
        head_publisher.Write(head);
        velocity_publisher.Write(velocity);
        next_publish_time += PUBLISH_PERIOD;
        std::this_thread::sleep_until(next_publish_time);
    }

    velocity_publisher.Write(geometry_msgs::msg::dds_::Twist_{});
    std::cout << "Stopped; sent zero velocity." << std::endl;
    return 0;
}
