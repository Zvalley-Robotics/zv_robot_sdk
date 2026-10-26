#pragma once

#include <string>
#include <vector>
#include <zv/robot/client/client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "media_api.hpp"

namespace zv::robot
{

struct StreamControlResult
{
    bool success{false};
    std::string message;
};

struct AudioControlResult
{
    bool success{false};
    std::string message;
};

class MediaClient : public Client
{
public:
    MediaClient() : Client(MEDIA_SERVICE_NAME)
    {
        // TRRO startup and audio shutdown can exceed the generic SDK's one-second default.
        SetTimeout(20.0F);
    }
    ~MediaClient() override = default;

    void Init()
    {
        SetApiVersion(MEDIA_API_VERSION);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_MEDIA_STREAM_CONTROL);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_MEDIA_START_AUDIO);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_MEDIA_STOP_AUDIO);
    }

    int32_t ControlStream(const std::vector<std::string> & camera_codes, bool enable, const std::string & stream_mode,
                          StreamControlResult & result, const std::string & trro_config = "")
    {
        result = StreamControlResult{};
        if (camera_codes.empty() || (stream_mode != "local" && stream_mode != "remote" && stream_mode != "both")) {
            result.message = "camera_codes must not be empty and stream_mode must be local, remote or both";
            return RPC_ERR_SERVER_API_PARAMETER;
        }

        nlohmann::json parameters;
        parameters["camera_codes"] = camera_codes;
        parameters["enable"] = enable;
        parameters["stream_mode"] = stream_mode;
        if (!trro_config.empty()) {
            parameters["trro_config"] = trro_config;
        }

        std::string data;
        const int32_t ret = Call(ROBOT_API_ID_MEDIA_STREAM_CONTROL, parameters.dump(), data);

        try {
            const auto response = nlohmann::json::parse(data);
            if (!response.is_object() || !response.contains("success") || !response["success"].is_boolean()) {
                return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
            }
            result.success = response["success"].get<bool>();
            result.message = response.value("message", std::string{});
        } catch (const nlohmann::json::exception &) {
            return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
        }

        return ret;
    }

    int32_t ControlStream(const std::vector<std::string> & camera_codes, bool enable, const std::string & stream_mode,
                          const std::string & trro_config = "")
    {
        StreamControlResult result;
        return ControlStream(camera_codes, enable, stream_mode, result, trro_config);
    }

    int32_t StartStream(const std::vector<std::string> & camera_codes, const std::string & stream_mode,
                        StreamControlResult & result, const std::string & trro_config = "")
    {
        return ControlStream(camera_codes, true, stream_mode, result, trro_config);
    }

    int32_t StartStream(const std::vector<std::string> & camera_codes, const std::string & stream_mode,
                        const std::string & trro_config = "")
    {
        return ControlStream(camera_codes, true, stream_mode, trro_config);
    }

    int32_t StopStream(const std::vector<std::string> & camera_codes, const std::string & stream_mode,
                       StreamControlResult & result)
    {
        return ControlStream(camera_codes, false, stream_mode, result);
    }

    int32_t StopStream(const std::vector<std::string> & camera_codes, const std::string & stream_mode)
    {
        return ControlStream(camera_codes, false, stream_mode);
    }

    int32_t StartAudio(AudioControlResult & result) { return ControlAudio(ROBOT_API_ID_MEDIA_START_AUDIO, result); }

    int32_t StartAudio()
    {
        AudioControlResult result;
        return StartAudio(result);
    }

    int32_t StopAudio(AudioControlResult & result) { return ControlAudio(ROBOT_API_ID_MEDIA_STOP_AUDIO, result); }

    int32_t StopAudio()
    {
        AudioControlResult result;
        return StopAudio(result);
    }

private:
    int32_t ControlAudio(int32_t api_id, AudioControlResult & result)
    {
        result = AudioControlResult{};

        std::string data;
        const int32_t ret = Call(api_id, "{}", data);

        try {
            const auto response = nlohmann::json::parse(data);
            if (!response.is_object() || !response.contains("success") || !response["success"].is_boolean()) {
                return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
            }
            result.success = response["success"].get<bool>();
            result.message = response.value("message", std::string{});
        } catch (const nlohmann::json::exception &) {
            return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
        }

        return ret;
    }
};

}  // namespace zv::robot
