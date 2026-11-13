#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <csignal>
#include <ctime>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <zv/robot/audio/audio_client.hpp>
#include <zv/robot/loco/loco_client.hpp>
#include <zv/robot/slam/slam_client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "navigation_result.hpp"

namespace tour
{
using Json = nlohmann::json;

struct Action
{
    std::string model;
    double delay_s = 0.0;  // 首项相对语音开始，其余项相对上一动作的下发开始时间。
};

struct Waypoint
{
    std::string name;
    zv::robot::NavPose pose;
    std::string wav_file;       // 机器人音频播放器主机上的绝对路径。
    std::vector<Action> actions;  // 语音期间按顺序下发的 t4_zl* 一次性动作。
    std::string action_model;   // 兼容原有单模型配置。
    bool action_once = true;    // 仅用于原有循环动作。
    double wait_after_s = 0.0;
};

struct Config
{
    std::string scene_id;
    std::string navigation_model = "t4_walk";
    bool initialize_localization = true;
    double rpc_timeout_s = 10.0;
    double audio_timeout_s = 120.0;
    double localization_timeout_s = 120.0;
    double navigation_timeout_s = 300.0;
    double action_timeout_s = 60.0;
    double poll_interval_s = 0.5;
    std::vector<Waypoint> waypoints;
};

inline double Number(const Json & value)
{
    const double number = value.get<double>();
    if (!value.is_number() || !std::isfinite(number) || std::abs(number) > std::numeric_limits<float>::max())
        throw std::runtime_error("配置或位姿中有无效数值");
    return number;
}

inline Config LoadConfig(const std::string & path)
{
    std::ifstream input(path);
    if (!input) throw std::runtime_error("无法读取导览配置：" + path);
    const auto json = Json::parse(input);
    Config config;
    config.scene_id = json.at("scene_id").get<std::string>();
    config.navigation_model = json.value("navigation_model", config.navigation_model);
    config.initialize_localization = json.value("initialize_localization", true);
    if (config.scene_id.empty() || config.navigation_model.empty())
        throw std::runtime_error("scene_id 和 navigation_model 不能为空");
    const std::vector<std::pair<std::string, double *>> numbers{
        {"rpc_timeout_s", &config.rpc_timeout_s}, {"audio_timeout_s", &config.audio_timeout_s},
        {"localization_timeout_s", &config.localization_timeout_s},
        {"navigation_timeout_s", &config.navigation_timeout_s}, {"action_timeout_s", &config.action_timeout_s},
        {"poll_interval_s", &config.poll_interval_s}
    };
    for (const auto & [key, value] : numbers) {
        if (json.contains(key)) *value = Number(json.at(key));
        if (*value <= 0) throw std::runtime_error(key + " 必须大于 0");
    }
    const auto & points = json.at("waypoints");
    if (!points.is_array()) throw std::runtime_error("waypoints 必须为数组");
    for (const auto & point : points) {
        if (!point.value("enabled", true)) continue;
        Waypoint waypoint;
        waypoint.name = point.at("name").get<std::string>();
        waypoint.pose.position.x = Number(point.at("coordinateX"));
        waypoint.pose.position.y = Number(point.at("coordinateY"));
        waypoint.pose.euler.y = Number(point.at("coordinateA"));  // 附件中的角度已是弧度，无需转为度。
        waypoint.wav_file = point.at("wav_file").get<std::string>();
        waypoint.action_model = point.value("action_model", std::string{});
        waypoint.action_once = point.value("action_once", true);
        if (point.contains("actions")) {
            if (!point.at("actions").is_array()) throw std::runtime_error("actions 必须为数组");
            if (!waypoint.action_model.empty()) throw std::runtime_error("actions 和 action_model 不能同时设置");
            for (const auto & item : point.at("actions")) {
                Action action;
                action.model = item.at("model").get<std::string>();
                if (item.contains("delay_s")) action.delay_s = Number(item.at("delay_s"));
                if (action.model.rfind("t4_zl", 0) != 0 || action.delay_s < 0)
                    throw std::runtime_error("actions 中的 model 必须为 t4_zl*，delay_s 不能为负");
                waypoint.actions.push_back(action);
            }
        } else if (waypoint.action_model.rfind("t4_zl", 0) == 0) {
            waypoint.actions.push_back({waypoint.action_model, 0.0});
            waypoint.action_model.clear();
        }
        if (point.contains("wait_after_s")) waypoint.wait_after_s = Number(point.at("wait_after_s"));
        if (waypoint.name.empty() || waypoint.wav_file.empty() || waypoint.wav_file.front() != '/' ||
            waypoint.wait_after_s < 0)
            throw std::runtime_error("点位名称不能为空，WAV 必须为播放器主机绝对路径，停留秒数不能为负");
        if (waypoint.action_model == config.navigation_model)
            throw std::runtime_error("讲解动作不能使用导航行走模型");
        config.waypoints.push_back(waypoint);
    }
    if (config.waypoints.empty()) throw std::runtime_error("至少需要一个启用的点位");
    return config;
}

// 客户端作为参数传入，示例流程可在不连接机器人的情况下验证。
template <class Slam, class Loco, class Audio>
class Runner
{
public:
    Runner(const Config & config, Slam & slam, Loco & loco, Audio & audio,
           const volatile std::sig_atomic_t & running, NavigationResults & results)
        : config_(config), slam_(slam), loco_(loco), audio_(audio), running_(running), results_(results) {}

    void Run()
    {
        try {
            CheckRunning();
            WaitModel(config_.navigation_model);  // 不自动起身或切换运控模式。
            CheckModels();
            PrepareLocalization();
            for (size_t i = 0; i < config_.waypoints.size(); ++i) {
                const auto & waypoint = config_.waypoints[i];
                CheckRunning();
                Log("导航：" + waypoint.name + "（" + std::to_string(i + 1) + "/" +
                    std::to_string(config_.waypoints.size()) + "），目标=" + PoseText(waypoint.pose.position.x,
                    waypoint.pose.position.y, waypoint.pose.euler.y));
                Navigate(waypoint);
                Explain(waypoint);
                Sleep(waypoint.wait_after_s);
            }
            Log("导览完成");
        } catch (...) {
            Cleanup();
            throw;
        }
    }

private:
    using Clock = std::chrono::steady_clock;
    const Config & config_;
    Slam & slam_;
    Loco & loco_;
    Audio & audio_;
    const volatile std::sig_atomic_t & running_;
    NavigationResults & results_;
    const Clock::time_point started_ = Clock::now();
    bool navigation_owned_ = false;
    std::string action_owned_;
    std::string last_robot_state_, last_slam_state_;

    static std::string Fixed(double value)
    {
        std::ostringstream output;
        output << std::fixed << std::setprecision(4) << value;
        return output.str();
    }

    static std::string PoseText(double x, double y, double yaw)
    {
        return "(x=" + Fixed(x) + "m, y=" + Fixed(y) + "m, yaw=" + Fixed(yaw) + "rad)";
    }

    void Log(const std::string & message) const
    {
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm local{};
        localtime_r(&now, &local);
        std::ostringstream output;
        output << '[' << std::put_time(&local, "%Y-%m-%dT%H:%M:%S%z") << " +"
               << std::fixed << std::setprecision(3)
               << std::chrono::duration<double>(Clock::now() - started_).count() << "s] " << message;
        std::cout << output.str() << std::endl;
    }

    void CheckRunning() const
    {
        if (!running_) throw std::runtime_error("导览已中断");
    }

    void Sleep(double seconds) const
    {
        const auto deadline = Clock::now() + std::chrono::duration<double>(seconds);
        while (Clock::now() < deadline) {
            CheckRunning();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        CheckRunning();
    }

    static void CheckCode(int32_t code, const std::string & operation, bool slam = false)
    {
        if (code != 0 && !(slam && code == 1))
            throw std::runtime_error(operation + "失败，code=" + std::to_string(code));
    }

    zv::robot::LocoState RobotState()
    {
        CheckRunning();
        zv::robot::LocoState robot;
        const auto code = loco_.GetRobotState(robot);
        if (code != 0) Log("查询运控状态，code=" + std::to_string(code));
        CheckCode(code, "查询运控状态");
        const auto snapshot = "state=" + std::to_string(robot.state) + "(" + robot.stateName + "), mode=" +
            robot.activeMode + ", source=" + robot.activeControlSource + ", model=" + robot.activeModel +
            ", fault=" + (robot.faultActive ? "true" : "false") + ", reason=" + robot.faultReason;
        if (snapshot != last_robot_state_) {
            Log("运控状态：" + snapshot);
            last_robot_state_ = snapshot;
        }
        if (robot.faultActive) throw std::runtime_error("运控故障：" + robot.faultReason);
        return robot;
    }

    void WaitModel(const std::string & model)
    {
        Log("等待模型=" + model + "，超时=" + Fixed(config_.action_timeout_s) + "s");
        const auto start = Clock::now();
        auto last_progress = start;
        bool first = true;
        do {
            if (RobotState().activeModel == model) {
                Log("模型已生效：" + model);
                return;
            }
            const auto now = Clock::now();
            if (first || std::chrono::duration<double>(now - last_progress).count() >= 2.0) {
                Log("模型尚未切换：期望=" + model + "，" + last_robot_state_ + "，已等待=" +
                    Fixed(std::chrono::duration<double>(now - start).count()) + "s/" +
                    Fixed(config_.action_timeout_s) + "s");
                first = false;
                last_progress = now;
            }
            Sleep(config_.poll_interval_s);
        } while (std::chrono::duration<double>(Clock::now() - start).count() < config_.action_timeout_s);
        throw std::runtime_error("等待动作模型超时：期望=" + model + "，最后运控状态：" + last_robot_state_);
    }

    void CheckModels()
    {
        if (std::none_of(config_.waypoints.begin(), config_.waypoints.end(),
                         [](const auto & point) { return !point.action_model.empty() || !point.actions.empty(); })) return;
        std::vector<std::string> models;
        CheckCode(loco_.GetSupportedModels(models), "查询动作模型列表");
        for (const auto & point : config_.waypoints) {
            if (!point.action_model.empty() && std::find(models.begin(), models.end(), point.action_model) == models.end())
                throw std::runtime_error("机器人不支持动作模型：" + point.action_model);
            for (const auto & action : point.actions) {
                if (std::find(models.begin(), models.end(), action.model) == models.end())
                    throw std::runtime_error("机器人不支持动作模型：" + action.model);
            }
        }
    }

    std::string State()
    {
        CheckRunning();
        std::string state, scene;
        const auto code = slam_.GetState(state, scene);
        const auto snapshot = "code=" + std::to_string(code) + ", state=" + state + ", scene_id=" + scene;
        if (snapshot != last_slam_state_) {
            Log("SLAM 状态：" + snapshot + "，期望场景=" + config_.scene_id);
            last_slam_state_ = snapshot;
        }
        CheckCode(code, "查询导航状态", true);
        if (state != "IDLE" && scene != config_.scene_id)
            throw std::runtime_error("当前场景与导览场景不同：" + scene);
        return state;
    }

    void PrepareLocalization()
    {
        const auto state = State();
        if (state == "NAV_READY") {
            Log("复用当前场景定位");
            return;
        }
        if (state == "IDLE" && config_.initialize_localization) {
            std::string data;
            const auto code = slam_.InitPose(config_.scene_id, true, zv::robot::NavPose{}, data);
            Log("InitPose，code=" + std::to_string(code) + "，响应=" + data);
            CheckCode(code, "启动自动定位", true);
        } else if (state != "NAV_STARTING") {
            throw std::runtime_error("请先完成指定场景的定位，当前状态：" + state);
        }
        const auto start = Clock::now();
        do {
            const auto current = State();
            if (current == "NAV_READY") return;
            if (current != "NAV_STARTING" && current != "IDLE")
                throw std::runtime_error("定位状态异常：" + current);
            Sleep(config_.poll_interval_s);
        } while (std::chrono::duration<double>(Clock::now() - start).count() < config_.localization_timeout_s);
        throw std::runtime_error("等待定位超时");
    }

    void Navigate(const Waypoint & waypoint)
    {
        if (State() != "NAV_READY") throw std::runtime_error("导航未就绪");
        if (RobotState().activeModel != config_.navigation_model) throw std::runtime_error("当前未使用导航行走模型");
        results_.Reset();
        navigation_owned_ = true;
        const auto start = Clock::now();
        std::string data;
        auto code = slam_.PoseNav(false, waypoint.pose, data);
        Log("PoseNav：" + waypoint.name + "，relative=false，code=" + std::to_string(code) + "，响应=" + data);
        if (code == 0) {
            const auto request_id = Json::parse(data).at("request_id").get<std::string>();
            if (request_id.empty()) throw std::runtime_error("导航接受响应缺少 request_id");
            Log("导航等待：" + waypoint.name + "，request_id=" + request_id + "，等待最终结果 1/2");
            auto last_progress = Clock::now();
            while (true) {
                CheckRunning();
                NavigationResult result;
                if (results_.Take(request_id, result)) {
                    code = result.code;
                    data = std::move(result.data);
                    Log("导航结果：" + waypoint.name + "，request_id=" + request_id +
                        "，code=" + std::to_string(code) + "，响应=" + data);
                    break;
                }
                const auto now = Clock::now();
                const double elapsed = std::chrono::duration<double>(now - start).count();
                if (elapsed >= config_.navigation_timeout_s)
                    throw std::runtime_error("等待导航结果超时：" + waypoint.name + "，request_id=" + request_id);
                if (std::chrono::duration<double>(now - last_progress).count() >= 2.0) {
                    Log("导航等待：" + waypoint.name + "，request_id=" + request_id +
                        "，已等待=" + Fixed(elapsed) + "s/" + Fixed(config_.navigation_timeout_s) + "s");
                    last_progress = now;
                }
                Sleep(config_.poll_interval_s);
            }
        }
        CheckRunning();
        if (code != 1)
            throw std::runtime_error("导航失败：" + waypoint.name + "，code=" + std::to_string(code) + "，响应=" + data);
        navigation_owned_ = false;
        Log("导航到达：" + waypoint.name + "，耗时=" +
            Fixed(std::chrono::duration<double>(Clock::now() - start).count()) + "s");
    }

    void Explain(const Waypoint & waypoint)
    {
        CheckRunning();
        const bool one_shot = !waypoint.actions.empty();
        if (!waypoint.action_model.empty() && !one_shot) {
            action_owned_ = waypoint.action_model;
            Log("SetModel 下发：" + waypoint.name + "，model=" + action_owned_ + "，action_once=" +
                (waypoint.action_once ? "true" : "false"));
            const auto start = Clock::now();
            const auto code = loco_.SetModel(action_owned_);
            Log("SetModel 返回：model=" + action_owned_ + "，code=" + std::to_string(code) + "，调用耗时=" +
                Fixed(std::chrono::duration<double>(Clock::now() - start).count()) + "s" +
                (code == 0 ? "（仅表示请求已接受）" : "，error=" + loco_.GetLastErrorMessage()));
            CheckCode(code, "播放讲解动作");
            WaitModel(action_owned_);  // SetModel 成功只代表接收，须确认模型已切换。
            if (waypoint.action_once) {
                Log("FinishLoop 下发：model=" + action_owned_ + "，设置动作播放一轮");
                const auto code = loco_.FinishLoop();
                Log("FinishLoop 返回：code=" + std::to_string(code));
                CheckCode(code, "设置动作播放一轮");
            }
        }
        CheckRunning();
        Log("讲解：" + waypoint.name + "，WAV=" + waypoint.wav_file);
        zv::robot::WavPlaybackResult result;
        const auto start = Clock::now();
        int32_t code;
        if (one_shot) {
            // WAV RPC 同步等待播放结束，放到独立线程；运控客户端只由主线程调用。
            auto playback = std::async(std::launch::async, [&] {
                return audio_.PlayWavFile(waypoint.wav_file, result);
            });
            auto next_action = start + std::chrono::duration<double>(waypoint.actions.front().delay_s);
            size_t executed = 0;
            while (playback.wait_for(std::chrono::milliseconds(10)) != std::future_status::ready) {
                CheckRunning();
                if (executed >= waypoint.actions.size() || Clock::now() < next_action) continue;
                // 下发前再检查播放是否结束；已结束就不下发后续编排动作。
                if (playback.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) break;
                const auto & action = waypoint.actions[executed];
                Log("SetModel 下发：" + waypoint.name + "，model=" + action.model + "，序号=" +
                    std::to_string(executed + 1) + "/" + std::to_string(waypoint.actions.size()) +
                    "，等待间隔=" + Fixed(action.delay_s) + "s");
                const auto action_start = Clock::now();
                const auto action_code = loco_.SetModel(action.model);
                Log("SetModel 返回：model=" + action.model + "，code=" + std::to_string(action_code) +
                    "，调用耗时=" + Fixed(std::chrono::duration<double>(Clock::now() - action_start).count()) + "s" +
                    (action_code == 0 ? "（仅表示请求已接受）" : "，error=" + loco_.GetLastErrorMessage()));
                CheckCode(action_code, "播放讲解动作");
                ++executed;
                if (executed < waypoint.actions.size()) {
                    next_action = action_start + std::chrono::duration<double>(waypoint.actions[executed].delay_s);
                }
            }
            code = playback.get();
            Log("动作编排下发结束：" + waypoint.name + "，已下发=" + std::to_string(executed) + "/" +
                std::to_string(waypoint.actions.size()));
        } else {
            code = audio_.PlayWavFile(waypoint.wav_file, result);
        }
        Log("WAV 返回：" + waypoint.name + "，code=" + std::to_string(code) + "，success=" +
            (result.success ? "true" : "false") + "，duration_s=" + Fixed(result.duration_s) +
            "，调用耗时=" + Fixed(std::chrono::duration<double>(Clock::now() - start).count()) +
            "s，error=" + result.error_msg);
        CheckCode(code, "播放 WAV");
        if (!result.success) throw std::runtime_error("WAV 播放失败：" + result.error_msg);
        CheckRunning();  // 同步 WAV 返回时播放已结束；超时不自动重试。
        if (!action_owned_.empty()) {
            if (!waypoint.action_once && RobotState().activeModel == action_owned_) {
                Log("FinishLoop 下发：model=" + action_owned_ + "，音频结束后退出动作循环");
                const auto code = loco_.FinishLoop();
                Log("FinishLoop 返回：code=" + std::to_string(code));
                CheckCode(code, "结束讲解动作循环");
            }
            WaitModel(config_.navigation_model);
            action_owned_.clear();
        }
    }

    void Cleanup() noexcept
    {
        // 只处理本示例发起的导航/动作，不关闭 SLAM，不自动恢复运动。
        try {
            if (navigation_owned_) {
                std::string data;
                const auto code = slam_.CancelNav(data);
                Log("已尝试取消导览导航，code=" + std::to_string(code) + "，响应=" + data);
            }
        } catch (const std::exception & error) { std::cerr << error.what() << std::endl; }
        try {
            if (!action_owned_.empty()) {
                zv::robot::LocoState state;
                if (loco_.GetRobotState(state) == 0 && state.activeModel == action_owned_)
                    std::cerr << "已尝试暂停讲解动作，code=" << loco_.PauseMotion() << std::endl;
            }
        } catch (const std::exception & error) { std::cerr << error.what() << std::endl; }
    }
};
}  // namespace tour
