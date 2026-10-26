#include <unistd.h>

#include <charconv>
#include <csignal>
#include <iostream>
#include <string_view>
#include <zv/robot/channel/channel_subscriber.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "RobotState_.hpp"

static const std::string STATE_TOPIC = "rt/robot/state";
// 桥接配置：reliability=best_effort, history=keep_last, depth=5。
static constexpr int64_t STATE_QUEUE_LEN = 5;
static constexpr bool STATE_RELIABLE = false;

using namespace zv::robot;
using namespace zv::common;

static volatile std::sig_atomic_t STOPPED = 0;

void stopSubscriber(int) { STOPPED = 1; }

// 1. 消息回调函数，当有 RobotState_ 消息到达指定话题时，SDK 自动调用该函数
void Handler(const void * msg, bool json_output)
{
    const auto * state = static_cast<const robot_interfaces::msg::dds_::RobotState_ *>(msg);

    if (json_output) {
        std::cout << nlohmann::json({
                                        {"state",                 state->state_()                },
                                        {"state_name",            state->state_name_()           },
                                        {"active_mode",           state->active_mode_()          },
                                        {"active_control_source", state->active_control_source_()},
                                        {"active_rl_model",       state->active_rl_model_()      },
                                        {"fault_active",          state->fault_active_()         },
                                        {"fault_reason",          state->fault_reason_()         }
        })
                         .dump()
                  << std::endl;
        return;
    }

    // 2. 读取状态字段并打印
    std::cout << "state: " << static_cast<int>(state->state_()) << ", state_name: " << state->state_name_()
              << ", active_mode: " << state->active_mode_() << ", active_rl_model: " << state->active_rl_model_()
              << std::endl;
}

int main(int argc, char ** argv)
{
    int32_t domain_id = 100;  // 保留此示例现有的默认域；Web 工具显式传入配置值。
    std::string network_interface;
    bool json_output = false;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--help") {
            std::cout << "Usage: robotStateSubscriber [--domain_id=0..232] [--network_interface=eth0] [--json]\n";
            return 0;
        }
        if (arg == "--json") {
            json_output = true;
        } else if (arg.substr(0, 12) == "--domain_id=") {
            const auto value = arg.substr(12);
            const auto result = std::from_chars(value.data(), value.data() + value.size(), domain_id);
            if (result.ec != std::errc() || result.ptr != value.data() + value.size() || domain_id < 0 ||
                domain_id > 232) {
                std::cerr << "Invalid domain_id: " << value << std::endl;
                return 1;
            }
        } else if (arg.substr(0, 20) == "--network_interface=") {
            network_interface = arg.substr(20);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            return 1;
        }
    }
    std::signal(SIGINT, stopSubscriber);
    std::signal(SIGTERM, stopSubscriber);

    try {
        // 3. 初始化 Channel 工厂
        ChannelFactory::Instance()->Init(domain_id, network_interface);

        // 4. 创建 Subscriber 对象，绑定话题和消息类型
        ChannelSubscriber<robot_interfaces::msg::dds_::RobotState_> subscriber(STATE_TOPIC);

        // 5. 初始化通信通道并注册回调函数（QoS 与 robot_external_bridge 保持一致）
        subscriber.InitChannel([json_output](const void * msg) { Handler(msg, json_output); }, STATE_QUEUE_LEN,
                               STATE_RELIABLE);

        // 6. 保持进程运行，Subscriber 在内部线程接收消息并调用 Handler
        while (!STOPPED) {
            sleep(1);
        }
        subscriber.CloseChannel();
    } catch (const std::exception & error) {
        std::cerr << "State subscriber failed: " << error.what() << std::endl;
        return 1;
    }
    return 0;
}
