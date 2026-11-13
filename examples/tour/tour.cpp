#include <filesystem>
#include <zv/robot/channel/channel_subscriber.hpp>
#include "../dds_options.hpp"
#include "tour.hpp"

namespace
{
volatile std::sig_atomic_t running = 1;
void HandleSignal(int) { running = 0; }

std::string DefaultConfigPath()
{
    auto directory = std::filesystem::read_symlink("/proc/self/exe").parent_path();
    while (!directory.empty()) {
        const auto path = directory / "config/tour.json";
        if (std::filesystem::is_regular_file(path)) return path.string();
        const auto parent = directory.parent_path();
        if (parent == directory) break;
        directory = parent;
    }
    throw std::runtime_error("找不到 config/tour.json，请使用 --config=<path> 指定导览配置");
}
}  // namespace

int main(int argc, const char * const argv[])
{
    try {
        // 与其他示例一致：--use_config 和指定网卡二选一，不新增域 ID 参数。
        const auto args = example::ParseDdsArguments(argc, argv, {"config", "check"});
        if (args.help) {
            std::cout << "Usage: tour [--use_config | --network_interface=<iface>]\n"
                         "            [--config=<tour.json>] [--check=true]\n"
                         "--check=true 只检查和显示导览配置，不初始化 DDS、不控制机器人\n";
            return 0;
        }
        if (args.values.count("check") && args.values.at("check") != "true" && args.values.at("check") != "false")
            throw std::runtime_error("--check 仅支持 true 或 false");
        const auto path = args.values.count("config") ? args.values.at("config") : DefaultConfigPath();
        const auto config = tour::LoadConfig(path);
        std::cout << "导览配置：" << path << "\n场景：" << config.scene_id
                  << "，点位数：" << config.waypoints.size() << std::endl;
        for (const auto & point : config.waypoints) {
            std::cout << point.name << "：(" << point.pose.position.x << ", " << point.pose.position.y << ", "
                      << point.pose.euler.y << ")，WAV=" << point.wav_file << "，动作=" << point.action_model
                      << "，编排动作数=" << point.actions.size() << '\n';
            for (size_t i = 0; i < point.actions.size(); ++i) {
                std::cout << "  动作" << i + 1 << "：model=" << point.actions[i].model
                          << "，等待间隔=" << point.actions[i].delay_s << "s\n";
            }
        }
        if (args.values.count("check") && args.values.at("check") == "true") return 0;

        std::cout << "DDS：" << (args.use_config ? "配置文件=" + example::DdsConfigPath() :
            "网卡=" + (args.network_interface.empty() ? std::string("自动选择") : args.network_interface)) << '\n'
                  << "状态查询间隔=" << config.poll_interval_s << "s，导航结果等待超时="
                  << config.navigation_timeout_s << "s" << std::endl;
        std::signal(SIGINT, HandleSignal);
        std::signal(SIGTERM, HandleSignal);
        example::InitDds(args.network_interface, args.use_config);
        tour::NavigationResults results;
        zv::robot::ChannelSubscriber<zv::robot::Response_> result_subscriber("rt/rpc/slam/response");
        result_subscriber.InitChannel([&results](const void * message) {
            results.Push(*static_cast<const zv::robot::Response_ *>(message));
        }, 64);
        zv::robot::SlamClient slam;
        zv::robot::LocoClient loco;
        zv::robot::AudioClient audio;
        slam.Init();
        loco.Init();
        audio.Init();
        slam.SetTimeout(static_cast<float>(config.rpc_timeout_s));
        loco.SetTimeout(static_cast<float>(config.rpc_timeout_s));
        audio.SetTimeout(static_cast<float>(config.audio_timeout_s));
        // 独立接收器必须先匹配服务端，避免只收到接受响应而丢失随后到达的终态。
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::duration<double>(config.rpc_timeout_s);
        while (result_subscriber.GetMatchedPublisherCount() == 0) {
            if (!running) return 130;
            if (std::chrono::steady_clock::now() >= deadline)
                throw std::runtime_error("导航结果响应话题未匹配：rt/rpc/slam/response");
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        tour::Runner runner(config, slam, loco, audio, running, results);
        runner.Run();
        return 0;
    } catch (const std::exception & error) {
        std::cerr << error.what() << std::endl;
        return running ? 1 : 130;
    }
}
