#include <algorithm>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <zv/idl/t4/RobotIntent_.hpp>
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
                                       {"source", "priority", "intent_type", "command", "action", "linear_x_mps",
                                        "linear_y_mps", "angular_z_radps", "count", "period_ms"});
        if (options.help || argc == 1) {
            std::cout << "RobotIntent -> rt/robot/input/intent\n"
                         "Options: [--use_config] [--network_interface=<iface>] \n"
                         "  [--intent_type=status] [--command=ready] [--action=] [--source=sdk] [--priority=50]\n"
                         "  [--linear_x_mps=0] [--linear_y_mps=0] [--angular_z_radps=0]\n"
                         "  [--count=1] [--period_ms=50]\n"
                         "count=0: publish until stopped; motion sends zero velocity on stop.\n";
            return 0;
        }
        const auto value = [&](const std::string & key, const std::string & fallback) {
            const auto found = options.values.find(key);
            return found == options.values.end() ? fallback : found->second;
        };
        const int count = example::ParseInteger(value("count", "1"), 0, 1000000);
        const int period = example::ParseInteger(value("period_ms", "50"), 10, 60000);
        // 构造高层控制意图，直接设置消息字段。
        robot_interfaces::msg::dds_::RobotIntent_ intent;
        intent.source_(value("source", "sdk"));
        intent.priority_(example::ParseInteger(value("priority", "50"), 0, 255));
        intent.intent_type_(value("intent_type", "status"));
        intent.command_(value("command", "ready"));
        intent.action_(value("action", ""));
        intent.linear_x_mps_(std::stof(value("linear_x_mps", "0")));
        intent.linear_y_mps_(std::stof(value("linear_y_mps", "0")));
        intent.angular_z_radps_(std::stof(value("angular_z_radps", "0")));

        example::InitDds(options.network_interface, options.use_config);
        zv::robot::ChannelPublisher<robot_interfaces::msg::dds_::RobotIntent_> publisher("rt/robot/input/intent");
        publisher.InitChannel();
        std::signal(SIGINT, Stop);
        std::signal(SIGTERM, Stop);
        Wait(500);  // 等待 DDS 发现，单次发送同样可以匹配已有订阅者。
        const auto stamp = [&] {
            const auto now = std::chrono::system_clock::now().time_since_epoch();
            const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(now);
            intent.header_().stamp_().sec_(static_cast<int32_t>(seconds.count()));
            intent.header_().stamp_().nanosec_(
                static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - seconds).count()));
        };
        for (int sent = 0; running && (count == 0 || sent < count); ++sent) {
            stamp();
            publisher.Write(intent);
            std::cout << "Published rt/robot/input/intent count=" << sent + 1 << std::endl;
            if (count == 0 || sent + 1 < count) Wait(period);
        }
        if ((count == 0 || !running) && intent.intent_type_() == "motion") {
            intent.linear_x_mps_(0);
            intent.linear_y_mps_(0);
            intent.angular_z_radps_(0);
            stamp();
            publisher.Write(intent);
            std::cout << "Stopped; sent zero velocity on rt/robot/input/intent" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        publisher.CloseChannel();
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
