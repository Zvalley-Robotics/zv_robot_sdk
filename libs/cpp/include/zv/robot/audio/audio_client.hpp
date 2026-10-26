#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <zv/robot/client/client.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

#include "audio_api.hpp"

namespace zv::robot
{

struct SynthesizeSpeechResult
{
    bool success{false};
    std::string file_path;
    std::string error_msg;
    int32_t sample_rate{0};
    uint32_t num_samples{0};
    float duration_s{0.0F};
    float talker_ms{0.0F};
    float code2wav_ms{0.0F};
    float wav_write_ms{0.0F};
    float backend_total_ms{0.0F};
    float rtf{0.0F};
    int64_t rvq_frames{0};
    int64_t rvq_codes{0};
    std::string backend;
};

struct WavPlaybackResult
{
    bool success{false};
    std::string error_msg;
    uint32_t sample_rate{0};
    uint16_t channels{0};
    uint16_t bits_per_sample{0};
    std::string encoding;
    uint64_t frames_played{0};
    float duration_s{0.0F};
};

class AudioClient : public Client
{
public:
    AudioClient() : Client(AUDIO_SERVICE_NAME)
    {
        // TTS inference and synchronous playback can both exceed the generic
        // one-second timeout.
        SetTimeout(120.0F);
    }
    ~AudioClient() override = default;

    void Init() override
    {
        SetApiVersion(AUDIO_API_VERSION);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_AUDIO_SYNTHESIZE_SPEECH);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_AUDIO_PLAY_WAV_STREAM);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_AUDIO_PLAY_WAV_FILE);
    }

    int32_t SynthesizeSpeech(const std::string & text, SynthesizeSpeechResult & result,
                             const std::string & output_name = "", int32_t speaker_id = -1,
                             const std::string & speaker = "")
    {
        result = SynthesizeSpeechResult{};
        if (text.empty() || speaker_id < -1) {
            result.error_msg =
                "text must not be empty and speaker_id must be "
                "greater than or equal to -1";
            return RPC_ERR_SERVER_API_PARAMETER;
        }

        nlohmann::json parameters;
        parameters["text"] = text;
        parameters["output_name"] = output_name;
        parameters["speaker_id"] = speaker_id;
        parameters["speaker"] = speaker;

        std::string data;
        const int32_t ret = Call(ROBOT_API_ID_AUDIO_SYNTHESIZE_SPEECH, parameters.dump(), data);
        if (!ParseSynthesizeSpeechResult(data, result)) {
            return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
        }
        return ret;
    }

    int32_t SynthesizeSpeech(const std::string & text, const std::string & output_name = "", int32_t speaker_id = -1,
                             const std::string & speaker = "")
    {
        SynthesizeSpeechResult result;
        return SynthesizeSpeech(text, result, output_name, speaker_id, speaker);
    }

    int32_t PlayWavStream(const std::vector<uint8_t> & wav_data, WavPlaybackResult & result)
    {
        result = WavPlaybackResult{};
        if (wav_data.empty()) {
            result.error_msg = "wav_data must not be empty";
            return RPC_ERR_SERVER_API_PARAMETER;
        }

        std::string data;
        const int32_t ret = Call(ROBOT_API_ID_AUDIO_PLAY_WAV_STREAM, "{}", wav_data, data);
        if (!ParseWavPlaybackResult(data, result)) {
            return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
        }
        return ret;
    }

    int32_t PlayWavStream(const std::vector<uint8_t> & wav_data)
    {
        WavPlaybackResult result;
        return PlayWavStream(wav_data, result);
    }

    int32_t PlayWavFile(const std::string & file_path, WavPlaybackResult & result)
    {
        result = WavPlaybackResult{};
        if (file_path.empty()) {
            result.error_msg = "file_path must not be empty";
            return RPC_ERR_SERVER_API_PARAMETER;
        }

        nlohmann::json parameters;
        parameters["file_path"] = file_path;

        std::string data;
        const int32_t ret = Call(ROBOT_API_ID_AUDIO_PLAY_WAV_FILE, parameters.dump(), data);
        if (!ParseWavPlaybackResult(data, result)) {
            return ret == RPC_OK ? RPC_ERR_CLIENT_API_DATA : ret;
        }
        return ret;
    }

    int32_t PlayWavFile(const std::string & file_path)
    {
        WavPlaybackResult result;
        return PlayWavFile(file_path, result);
    }

private:
    static bool ParseSynthesizeSpeechResult(const std::string & data, SynthesizeSpeechResult & result)
    {
        try {
            const auto response = nlohmann::json::parse(data);
            if (!response.is_object() || !response.contains("success") || !response["success"].is_boolean()) {
                return false;
            }

            result.success = response["success"].get<bool>();
            result.file_path = response.value("file_path", std::string{});
            result.error_msg = response.value("error_msg", std::string{});
            result.sample_rate = response.value("sample_rate", int32_t{0});
            result.num_samples = response.value("num_samples", uint32_t{0});
            result.duration_s = response.value("duration_s", 0.0F);
            result.talker_ms = response.value("talker_ms", 0.0F);
            result.code2wav_ms = response.value("code2wav_ms", 0.0F);
            result.wav_write_ms = response.value("wav_write_ms", 0.0F);
            result.backend_total_ms = response.value("backend_total_ms", 0.0F);
            result.rtf = response.value("rtf", 0.0F);
            result.rvq_frames = response.value("rvq_frames", int64_t{0});
            result.rvq_codes = response.value("rvq_codes", int64_t{0});
            result.backend = response.value("backend", std::string{});
            return true;
        } catch (const nlohmann::json::exception &) {
            return false;
        }
    }

    static bool ParseWavPlaybackResult(const std::string & data, WavPlaybackResult & result)
    {
        try {
            const auto response = nlohmann::json::parse(data);
            if (!response.is_object() || !response.contains("success") || !response["success"].is_boolean()) {
                return false;
            }

            result.success = response["success"].get<bool>();
            result.error_msg = response.value("error_msg", std::string{});
            result.sample_rate = response.value("sample_rate", uint32_t{0});
            result.channels = response.value("channels", uint16_t{0});
            result.bits_per_sample = response.value("bits_per_sample", uint16_t{0});
            result.encoding = response.value("encoding", std::string{});
            result.frames_played = response.value("frames_played", uint64_t{0});
            result.duration_s = response.value("duration_s", 0.0F);
            return true;
        } catch (const nlohmann::json::exception &) {
            return false;
        }
    }
};

}  // namespace zv::robot
