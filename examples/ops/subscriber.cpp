#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <zv/third_party/nlohmann_json/json.hpp>
#include <zv/robot/ops/diagnostics_subscriber.hpp>
#include "../dds_options.hpp"

namespace
{
volatile std::sig_atomic_t stopped = 0;
void Stop(int) { stopped = 1; }
}

int main(int argc, char ** argv)
{
    try {
        const auto options = example::ParseDdsArguments(argc, argv);
        if (options.help) {
            std::cout << "diagnosticsSubscriber [--use_config | --network_interface=...]\n";
            return 0;
        }
        zv::robot::DiagnosticsSubscriber subscriber;
        if (options.use_config) subscriber.Init(example::DdsConfigPath());
        else subscriber.Init(0, options.network_interface);
        std::signal(SIGINT, Stop);
        std::signal(SIGTERM, Stop);
        while (!stopped) {
            diagnostic_msgs::msg::dds_::DiagnosticArray_ message;
            if (subscriber.Take(message)) {
                // 每行输出一帧完整诊断数据，保留正常项与诊断详情。
                auto statuses = nlohmann::json::array();
                for (const auto & status : message.status_()) {
                    auto values = nlohmann::json::array();
                    for (const auto & pair : status.values_())
                        values.push_back({{"key", pair.key_()}, {"value", pair.value_()}});
                    statuses.push_back({{"level", status.level_()}, {"name", status.name_()},
                        {"message", status.message_()}, {"hardware_id", status.hardware_id_()}, {"values", values}});
                }
                const auto & header = message.header_();
                std::cout << nlohmann::json({{"header", {{"stamp", {{"sec", header.stamp_().sec_()},
                    {"nanosec", header.stamp_().nanosec_()}}}, {"frame_id", header.frame_id_()}}},
                    {"status", statuses}}).dump() << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        subscriber.Close();
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
    return 0;
}
