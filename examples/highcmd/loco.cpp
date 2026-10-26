#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <zv/robot/channel/channel_factory.hpp>
#include <zv/robot/loco/loco_client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace
{

constexpr std::chrono::milliseconds MOVE_PUBLISH_PERIOD{50};

const std::set<std::string> SUPPORTED_STATES = {"ready", "walk", "damp", "reinit"};

volatile std::sig_atomic_t keepRunning = 1;

struct Options
{
    std::string networkInterface{"lo"};
    std::string command;
    std::string value;
    bool relative{false};
};

void handleSignal(int) { keepRunning = 0; }

void printHelp()
{
    constexpr const char * HELP_TEXT =
        R"(Usage: loco [--network_interface=<iface>] <command[=args]>

查询命令:
  --robot_state                    查询机器人当前运动状态、模型
  --supported_models               查询 zv_apid 当前支持的动作模型
  --head_pose                      查询头部位置（米）与姿态四元数
  --version                        查询机器人版本号、发布日期

控制命令:
  --state=<state>                  设置运动状态
  --move=<vx vy vyaw>              连续移动 (速度m/s)
  --model=<model>                  切换动作模型（由 zv_apid 校验）
  --reset_fault                    单次提交故障复位意图（实际结果查看机器人状态）
  --config=<command action>        单次切换配置：getup 姿态、nav manual/auto/follow、speed std/high、mode rl/mix/app
  --head=<yaw pitch>               单次下发头部角度（度，-180..180）
  --relative                       与 --head 配合使用，表示相对角度；默认绝对角度

动作命令:
  --stop                          暂停当前动作（模型）
  --resume                        恢复当前动作（模型）
  --finish_loop                    当前循环动作播完后回到行走模型

示例:
  loco --robot_state
  loco --supported_models
  loco --state="ready"
  loco --reset_fault
  loco --config="getup stand"
  loco --config="nav auto"
  loco --config="speed high"
  loco --config="mode mix"
  loco --head="30 15"
  loco --head="10 5" --relative
  loco --head_pose
  loco --version
  loco --move="0.3 0.0 0.0"
  loco --stop
  loco --resume
  loco --model="t4_walk"
)";
    std::cout << HELP_TEXT;
}

std::string stripQuotes(std::string value)
{
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool setCommand(Options & options, const std::string & command, const std::string & value, std::string & error)
{
    if (!options.command.empty()) {
        error = "一次只能执行一个命令";
        return false;
    }
    options.command = command;
    options.value = value;
    return true;
}

bool parseArguments(int argc, const char * const argv[], Options & options, std::string & error)
{
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument.rfind("--", 0) != 0) {
            error = "无效参数: " + argument;
            return false;
        }

        const std::size_t separator = argument.find('=');
        const std::string key = argument.substr(2, separator == std::string::npos ? std::string::npos : separator - 2);
        const std::string value = separator == std::string::npos ? "" : stripQuotes(argument.substr(separator + 1));

        if (key == "help") {
            return setCommand(options, "help", "", error);
        }
        if (key == "network_interface") {
            if (value.empty()) {
                error = "--network_interface 不能为空";
                return false;
            }
            options.networkInterface = value;
            continue;
        }
        if (key == "relative") {
            if (!value.empty()) {
                error = "--relative 不接受参数";
                return false;
            }
            options.relative = true;
            continue;
        }
        if (key == "robot_state" || key == "head_pose" || key == "version") {
            if (!value.empty()) {
                error = "--" + key + " 不接受参数";
                return false;
            }
            if (!setCommand(options, key, "", error)) {
                return false;
            }
            continue;
        }
        if (key == "supported_models") {
            if (!value.empty()) {
                error = "--supported_models 不接受参数";
                return false;
            }
            if (!setCommand(options, "supported_models", "", error)) {
                return false;
            }
            continue;
        }
        if (key == "finish_loop" || key == "stop" || key == "resume" || key == "reset_fault") {
            if (!value.empty()) {
                error = "--" + key + " 不接受参数";
                return false;
            }
            if (!setCommand(options, key, "", error)) {
                return false;
            }
            continue;
        }
        if (key == "state" || key == "move" || key == "model" || key == "config" || key == "head") {
            if (value.empty()) {
                error = "--" + key + " 需要参数";
                return false;
            }
            if (!setCommand(options, key, value, error)) {
                return false;
            }
            continue;
        }
        error = "未知命令: --" + key;
        return false;
    }

    return true;
}

bool parseMove(const std::string & value, float & vx, float & vy, float & vyaw)
{
    std::istringstream stream(value);
    if (!(stream >> vx >> vy >> vyaw)) {
        return false;
    }
    stream >> std::ws;
    return stream.eof() && std::isfinite(vx) && std::isfinite(vy) && std::isfinite(vyaw);
}

// 统一失败输出：服务端返回了错误描述时追加在错误码后，tools 的 web 界面直接展示 stderr。
void printFailure(const char * action, int32_t code, const zv::robot::LocoClient & client)
{
    std::cerr << action << " failed with code: " << code;
    const std::string message = client.GetLastErrorMessage();
    if (!message.empty()) {
        std::cerr << " (" << message << ")";
    }
    std::cerr << std::endl;
}

int runMove(zv::robot::LocoClient & client, float vx, float vy, float vyaw)
{
    const bool stopCommand = vx == 0.0F && vy == 0.0F && vyaw == 0.0F;
    if (!stopCommand) {
        std::cout << "连续移动中，按 Ctrl+C 停止。" << std::endl;
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);
    }

    auto nextPublishTime = std::chrono::steady_clock::now();
    do {
        const int32_t result = client.Move(vx, vy, vyaw);
        if (result != 0) {
            printFailure("Move", result, client);
            return 1;
        }
        if (stopCommand) {
            std::cout << "停止命令已发送。" << std::endl;
            return 0;
        }

        nextPublishTime += MOVE_PUBLISH_PERIOD;
        std::this_thread::sleep_until(nextPublishTime);
    } while (keepRunning != 0);

    const int32_t stopResult = client.Move(0.0F, 0.0F, 0.0F);
    if (stopResult != 0) {
        printFailure("Stop", stopResult, client);
        return 1;
    }
    std::cout << "运动已停止。" << std::endl;
    return 0;
}

}  // namespace

int main(int argc, const char * argv[])
{
    if (argc == 1) {
        printHelp();
        return 0;
    }

    Options options;
    std::string error;
    if (!parseArguments(argc, argv, options, error)) {
        std::cerr << error << std::endl;
        printHelp();
        return 1;
    }
    if (options.relative && options.command != "head") {
        std::cerr << "--relative 仅适用于 --head" << std::endl;
        return 1;
    }
    if (options.command.empty() || options.command == "help") {
        printHelp();
        return 0;
    }

    if (options.command == "state" && SUPPORTED_STATES.count(options.value) == 0) {
        std::cerr << "不支持的运动状态: " << options.value << std::endl;
        return 1;
    }
    float vx = 0.0F;
    float vy = 0.0F;
    float vyaw = 0.0F;
    if (options.command == "move") {
        if (!parseMove(options.value, vx, vy, vyaw)) {
            std::cerr << "--" << options.command << " 需要且仅接受三个有限浮点数: vx vy vyaw" << std::endl;
            return 1;
        }
    }

    std::string configCommand;
    std::string configAction;
    if (options.command == "config") {
        std::istringstream stream(options.value);
        if (!(stream >> configCommand >> configAction) || !(stream >> std::ws).eof()) {
            std::cerr << "--config 需要且仅接受两个参数: command action" << std::endl;
            return 1;
        }
    }

    float yaw = 0.0F;
    float pitch = 0.0F;
    if (options.command == "head") {
        std::istringstream stream(options.value);
        if (!(stream >> yaw >> pitch) || !(stream >> std::ws).eof() || !std::isfinite(yaw) || !std::isfinite(pitch) ||
            std::abs(yaw) > 180.0F || std::abs(pitch) > 180.0F) {
            std::cerr << "--head 需要两个 -180..180 度内的有限角度: yaw pitch" << std::endl;
            return 1;
        }
    }

    zv::robot::ChannelFactory::Instance()->Init(0, options.networkInterface);

    zv::robot::LocoClient client;
    client.Init();
    client.SetTimeout(10.0F);

    if (options.command == "head") {
        const int32_t result = client.HeadControl(yaw, pitch, options.relative);
        if (result != 0) {
            printFailure("HeadControl", result, client);
            return 1;
        }
        std::cout << "head control command accepted" << std::endl;
        return 0;
    }
    if (options.command == "head_pose") {
        zv::robot::LocoHeadPose pose;
        const int32_t result = client.GetHeadPose(pose);
        if (result != 0) {
            printFailure("GetHeadPose", result, client);
            return 1;
        }
        std::cout << nlohmann::json(
                         {
                             {"head_pose",
                              {{"position", pose.position}, {"orientation", pose.orientation}, {"age", pose.age}}}
        })
                         .dump()
                  << std::endl;
        return 0;
    }
    if (options.command == "version") {
        zv::robot::LocoVersion version;
        const int32_t result = client.GetRobotVersion(version);
        if (result != 0) {
            printFailure("GetRobotVersion", result, client);
            return 1;
        }
        std::cout
            << nlohmann::json(
                   {
                       {"robot_version",
                        {{"version", version.version}, {"release_date", version.releaseDate}, {"age", version.age}}}
        })
                   .dump()
            << std::endl;
        return 0;
    }

    if (options.command == "config") {
        const int32_t result = client.SetConfig(configCommand, configAction);
        if (result != 0) {
            printFailure("SetConfig", result, client);
            return 1;
        }
        std::cout << "config command accepted: " << configCommand << " " << configAction << std::endl;
        return 0;
    }

    if (options.command == "reset_fault") {
        const int32_t result = client.ResetFault();
        if (result != 0) {
            printFailure("ResetFault", result, client);
            return 1;
        }
        std::cout << "reset fault command accepted; check robot state for the result" << std::endl;
        return 0;
    }

    if (options.command == "stop" || options.command == "resume") {
        const int32_t result = options.command == "stop" ? client.PauseMotion() : client.ResumeMotion();
        if (result != 0) {
            printFailure("MotionStop", result, client);
            return 1;
        }
        std::cout << "motion " << options.command << " command accepted" << std::endl;
        return 0;
    }

    if (options.command == "robot_state") {
        zv::robot::LocoState state;
        const int32_t result = client.GetRobotState(state);
        if (result != 0) {
            printFailure("GetRobotState", result, client);
            return 1;
        }
        std::cout << "robot state: " << state.stateName << " (" << state.state << ")\n"
                  << "active model: " << state.activeModel << std::endl;
        return 0;
    }

    if (options.command == "supported_models") {
        std::vector<std::string> models;
        const int32_t result = client.GetSupportedModels(models);
        if (result != 0) {
            printFailure("GetSupportedModels", result, client);
            return 1;
        }
        std::cout << nlohmann::json({
                                        {"supported_models", models}
        })
                         .dump()
                  << std::endl;
        return 0;
    }

    if (options.command == "state") {
        const int32_t result = client.SetState(options.value);
        if (result != 0) {
            printFailure("SetState", result, client);
            return 1;
        }
        std::cout << "state command accepted: " << options.value << std::endl;
        return 0;
    }

    if (options.command == "model") {
        const int32_t result = client.SetModel(options.value);
        if (result != 0) {
            printFailure("SetModel", result, client);
            return 1;
        }
        std::cout << "model command accepted: " << options.value << std::endl;
        return 0;
    }

    if (options.command == "finish_loop") {
        const int32_t result = client.FinishLoop();
        if (result != 0) {
            printFailure("FinishLoop", result, client);
            return 1;
        }
        std::cout << "finish loop command accepted" << std::endl;
        return 0;
    }

    return runMove(client, vx, vy, vyaw);
}
