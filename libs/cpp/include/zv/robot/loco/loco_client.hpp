#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <zv/robot/client/client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "loco_api.hpp"

namespace zv::robot
{

struct LocoState
{
    int32_t state{-1};
    std::string stateName;
    std::string activeMode;
    std::string activeControlSource;
    std::string activeModel;
    bool faultActive{false};
    std::string faultReason;
};

struct LocoHeadPose
{
    std::array<double, 3> position{};                       // x, y, z（米）
    std::array<double, 4> orientation{0.0, 0.0, 0.0, 1.0};  // 四元数 x, y, z, w
    double age{0.0};                                        // zv_api 接收该消息后经过的秒数
};

struct LocoVersion
{
    std::string version;
    std::string releaseDate;
    double age{0.0};
};

class LocoClient : public Client
{
public:
    LocoClient() : Client(LOCO_SERVICE_NAME) {}
    ~LocoClient() override = default;

    // 初始化通信频道
    void Init() override
    {
        SetApiVersion(LOCO_API_VERSION);

        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_ROBOT_STATE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_SUPPORTED_MODELS);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_SET_STATE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_SET_MODEL);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_MOVE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_RESET_FAULT);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_SET_CONFIG);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_HEAD_CONTROL);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_HEAD_POSE);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_LOCO_VERSION);
    }

    int32_t GetRobotState(LocoState & robotState)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_LOCO_ROBOT_STATE, "", data);
        if (result != 0) {
            return result;
        }

        try {
            const auto json = nlohmann::json::parse(data);
            if (!json.is_object() || !json.contains("state") || !json["state"].is_number_integer() ||
                !json.contains("active_rl_model") || !json["active_rl_model"].is_string()) {
                return RPC_ERR_CLIENT_API_DATA;
            }

            robotState.state = json["state"].get<int32_t>();
            robotState.stateName = json.value("state_name", "");
            robotState.activeMode = json.value("active_mode", "");
            robotState.activeControlSource = json.value("active_control_source", "");
            robotState.activeModel = json["active_rl_model"].get<std::string>();
            robotState.faultActive = json.value("fault_active", false);
            robotState.faultReason = json.value("fault_reason", "");
            return 0;
        } catch (const nlohmann::json::exception &) {
            return RPC_ERR_CLIENT_API_DATA;
        }
    }

    int32_t SetState(const std::string & state)
    {
        const nlohmann::json json = {
            {"state", state}
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_SET_STATE, json.dump(), data);
    }

    // 单次提交故障复位意图；返回成功表示 zv_api 已接受，实际结果由 RobotState 确认。
    int32_t ResetFault()
    {
        std::string data;
        return Call(ROBOT_API_ID_LOCO_RESET_FAULT, "{}", data);
    }

    // 单次提交配置切换意图；配置是否生效由机器人端准入与执行结果决定。
    int32_t SetConfig(const std::string & command, const std::string & action)
    {
        const nlohmann::json json = {
            {"command", command},
            {"action",  action }
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_SET_CONFIG, json.dump(), data);
    }

    // 单位为度；false 为绝对角度，true 为相对增量。每次调用只发布一次。
    int32_t HeadControl(float yaw, float pitch, bool is_relative = false)
    {
        if (!std::isfinite(yaw) || !std::isfinite(pitch) || std::abs(yaw) > 180.0F || std::abs(pitch) > 180.0F) {
            return RPC_ERR_SERVER_API_PARAMETER;
        }
        std::string data;
        return Call(ROBOT_API_ID_LOCO_HEAD_CONTROL,
                    nlohmann::json({
                                       {"yaw",         yaw        },
                                       {"pitch",       pitch      },
                                       {"is_relative", is_relative}
        })
                        .dump(),
                    data);
    }

    int32_t GetHeadPose(LocoHeadPose & pose)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_LOCO_HEAD_POSE, "{}", data);
        if (result != 0) return result;
        try {
            const auto json = nlohmann::json::parse(data);
            LocoHeadPose parsed;
            const auto & position = json.at("position");
            const auto & orientation = json.at("orientation");
            if (!position.is_array() || position.size() != 3 || !orientation.is_array() || orientation.size() != 4 ||
                !json.at("age").is_number())
                return RPC_ERR_CLIENT_API_DATA;
            for (size_t i = 0; i < parsed.position.size(); ++i) {
                if (!position[i].is_number()) return RPC_ERR_CLIENT_API_DATA;
                parsed.position[i] = position[i].get<double>();
                if (!std::isfinite(parsed.position[i])) return RPC_ERR_CLIENT_API_DATA;
            }
            for (size_t i = 0; i < parsed.orientation.size(); ++i) {
                if (!orientation[i].is_number()) return RPC_ERR_CLIENT_API_DATA;
                parsed.orientation[i] = orientation[i].get<double>();
                if (!std::isfinite(parsed.orientation[i])) return RPC_ERR_CLIENT_API_DATA;
            }
            parsed.age = json.at("age").get<double>();
            if (!std::isfinite(parsed.age) || parsed.age < 0) return RPC_ERR_CLIENT_API_DATA;
            pose = parsed;
            return 0;
        } catch (const nlohmann::json::exception &) {
            return RPC_ERR_CLIENT_API_DATA;
        }
    }

    int32_t GetRobotVersion(LocoVersion & version)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_LOCO_VERSION, "{}", data);
        if (result != 0) return result;
        try {
            const auto json = nlohmann::json::parse(data);
            if (!json.at("version").is_string() || !json.at("release_date").is_string() || !json.at("age").is_number())
                return RPC_ERR_CLIENT_API_DATA;
            LocoVersion parsed{json.at("version").get<std::string>(), json.at("release_date").get<std::string>(),
                               json.at("age").get<double>()};
            if (parsed.version.empty() || !std::isfinite(parsed.age) || parsed.age < 0) return RPC_ERR_CLIENT_API_DATA;
            version = parsed;
            return 0;
        } catch (const nlohmann::json::exception &) {
            return RPC_ERR_CLIENT_API_DATA;
        }
    }

    int32_t GetSupportedModels(std::vector<std::string> & models)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_LOCO_SUPPORTED_MODELS, "", data);
        if (result != 0) {
            return result;
        }

        try {
            const auto json = nlohmann::json::parse(data);
            if (!json.is_object() || !json.contains("models") || !json["models"].is_array()) {
                return RPC_ERR_CLIENT_API_DATA;
            }
            models = json["models"].get<std::vector<std::string>>();
            return 0;
        } catch (const nlohmann::json::exception &) {
            return RPC_ERR_CLIENT_API_DATA;
        }
    }

    int32_t SetModel(const std::string & model)
    {
        const nlohmann::json json = {
            {"model", model}
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_SET_MODEL, json.dump(), data);
    }

    int32_t FinishLoop()
    {
        const nlohmann::json json = {
            {"command", "finish_loop"}
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_SET_MODEL, json.dump(), data);
    }

    int32_t MotionStop(const std::string & action)
    {
        const nlohmann::json json = {
            {"command", "motion_stop"},
            {"action",  action       }
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_SET_MODEL, json.dump(), data);
    }

    int32_t PauseMotion() { return MotionStop("stop"); }

    int32_t ResumeMotion() { return MotionStop("resume"); }

    int32_t Move(float vx, float vy, float vyaw)
    {
        const nlohmann::json json = {
            {"vx",   vx  },
            {"vy",   vy  },
            {"vyaw", vyaw}
        };
        std::string data;
        return Call(ROBOT_API_ID_LOCO_MOVE, json.dump(), data);
    }
};

}  // namespace zv::robot
