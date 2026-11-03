#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <zv/idl/ros2/Twist_.hpp>
#include <zv/idl/t4/HeadControl_.hpp>
#include <zv/robot/channel/channel_publisher.hpp>

#include "../dds_options.hpp"

namespace
{
volatile std::sig_atomic_t running = 1;
void Stop(int) { running = 0; }

void Wait(int period_ms)
{
    for (int elapsed = 0; running && elapsed < period_ms; elapsed += 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(std::min(10, period_ms - elapsed)));
}
}  // namespace

int main(int argc, const char * const argv[])
{
    try {
        const auto options =
            example::ParseDdsArguments(argc, argv,
                                       {"topic", "yaw", "pitch", "is_relative", "linear_x", "linear_y", "linear_z",
                                        "angular_x", "angular_y", "angular_z", "count", "period_ms"});
        if (options.help || argc == 1) {
            std::cout << "HeadControl -> rt/robot/external/head_control; Twist -> rt/cmd_vel\n"
                         "Options: [--use_config] [--network_interface=<iface>] \n"
                         "  [--topic=head|velocity] [--count=1] [--period_ms=50]\n"
                         "  [--yaw=0] [--pitch=0] [--is_relative=0|1]\n"
                         "  [--linear_x=0] [--linear_y=0] [--linear_z=0]\n"
                         "  [--angular_x=0] [--angular_y=0] [--angular_z=0]\n"
                         "count=0: publish until stopped; Twist sends zero velocity on stop.\n";
            return 0;
        }
        const auto value = [&](const std::string & key, const std::string & fallback) {
            const auto found = options.values.find(key);
            return found == options.values.end() ? fallback : found->second;
        };
        const auto topic = value("topic", "head");
        if (topic != "head" && topic != "velocity") throw std::invalid_argument("Unsupported topic: " + topic);
        const int count = example::ParseInteger(value("count", "1"), 0, 1000000);
        const int period = example::ParseInteger(value("period_ms", "50"), 10, 60000);
        // 构造头部与底盘消息，直接设置各字段。
        robot_interfaces::msg::dds_::HeadControl_ head;
        geometry_msgs::msg::dds_::Twist_ velocity;
        head.yaw_(std::stof(value("yaw", "0")));
        head.pitch_(std::stof(value("pitch", "0")));
        head.is_relative_(example::ParseInteger(value("is_relative", "0"), 0, 1) != 0);
        if (topic == "head" && head.is_relative_() && count != 1)
            throw std::invalid_argument("relative head control requires count=1");
        velocity.linear_().x_(std::stod(value("linear_x", "0")));
        velocity.linear_().y_(std::stod(value("linear_y", "0")));
        velocity.linear_().z_(std::stod(value("linear_z", "0")));
        velocity.angular_().x_(std::stod(value("angular_x", "0")));
        velocity.angular_().y_(std::stod(value("angular_y", "0")));
        velocity.angular_().z_(std::stod(value("angular_z", "0")));
        example::InitDds(options.network_interface, options.use_config);
        std::signal(SIGINT, Stop);
        std::signal(SIGTERM, Stop);
        if (topic == "head") {
            zv::robot::ChannelPublisher<robot_interfaces::msg::dds_::HeadControl_> publisher(
                "rt/robot/external/head_control");
            publisher.InitChannel();
            Wait(500);
            for (int sent = 0; running && (count == 0 || sent < count); ++sent) {
                publisher.Write(head);
                std::cout << "Published rt/robot/external/head_control count=" << sent + 1 << std::endl;
                if (count == 0 || sent + 1 < count) Wait(period);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            publisher.CloseChannel();
        } else {
            zv::robot::ChannelPublisher<geometry_msgs::msg::dds_::Twist_> publisher("rt/cmd_vel");
            publisher.InitChannel();
            Wait(500);
            for (int sent = 0; running && (count == 0 || sent < count); ++sent) {
                publisher.Write(velocity);
                std::cout << "Published rt/cmd_vel count=" << sent + 1 << std::endl;
                if (count == 0 || sent + 1 < count) Wait(period);
            }
            if (count == 0 || !running) {
                publisher.Write(geometry_msgs::msg::dds_::Twist_{});
                std::cout << "Stopped; sent zero velocity on rt/cmd_vel" << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            publisher.CloseChannel();
        }
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
