#pragma once

#include <cstdint>
#include <exception>
#include <string>
#include <utility>
#include <vector>
#include <zv/common/auth_client.hpp>
#include <zv/common/file_info.hpp>
#include <zv/robot/client/client.hpp>
#include <zv/robot/data_collection/data_collection_api.hpp>
#include <zv/third_party/nlohmann_json/json.hpp>

namespace zv::robot
{

struct RecordingInfo
{
    FileInfo file;
    int64_t message_count{0};
    std::string message;
};

struct TopicList
{
    bool recording{false};
    std::vector<std::string> topics;
    std::vector<std::string> subscribed_topics;
    std::vector<std::string> skipped_topics;
    std::string message;
};

struct TopicUpdateResult
{
    std::vector<std::string> topics;
    std::vector<std::string> invalid_topics;
    std::string message;
};

// 取值与 capture_msgs/srv/SetTopics.srv 中的常量保持一致
enum class TopicEditMode : uint8_t {
    kReplace = 0,
    kAdd = 1,
    kRemove = 2,
};

namespace detail
{

// 读取字符串数组字段：字段缺失时视为空数组，类型不符时返回 false
inline bool ReadStringArray(const nlohmann::json & json, const char * key, std::vector<std::string> & values)
{
    values.clear();
    if (!json.contains(key)) return true;
    if (!json[key].is_array()) return false;
    for (const auto & item : json[key]) {
        if (!item.is_string()) return false;
        values.push_back(item.get<std::string>());
    }
    return true;
}

inline int32_t ParseTopicListResponse(const std::string & data, TopicList & topics)
{
    try {
        const auto json = nlohmann::json::parse(data);
        if (!json.is_object() || !json.value("success", false)) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        TopicList parsed;
        parsed.recording = json.value("recording", false);
        parsed.message = json.value("message", std::string{});
        if (!ReadStringArray(json, "topics", parsed.topics) ||
            !ReadStringArray(json, "subscribed_topics", parsed.subscribed_topics) ||
            !ReadStringArray(json, "skipped_topics", parsed.skipped_topics)) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        topics = std::move(parsed);
        return 0;
    } catch (const nlohmann::json::exception &) {
        return RPC_ERR_CLIENT_API_DATA;
    }
}

inline int32_t ParseTopicUpdateResponse(const std::string & data, TopicUpdateResult & result)
{
    try {
        const auto json = nlohmann::json::parse(data);
        if (!json.is_object() || !json.value("success", false)) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        TopicUpdateResult parsed;
        parsed.message = json.value("message", std::string{});
        if (!ReadStringArray(json, "topics", parsed.topics) ||
            !ReadStringArray(json, "invalid_topics", parsed.invalid_topics)) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        result = std::move(parsed);
        return 0;
    } catch (const nlohmann::json::exception &) {
        return RPC_ERR_CLIENT_API_DATA;
    }
}

inline int32_t ParseRecordingResponse(const std::string & data, RecordingInfo & recording)
{
    try {
        const auto json = nlohmann::json::parse(data);
        if (!json.is_object() || !json.value("success", false) || !json.contains("file") || !json["file"].is_object()) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        const auto & file = json["file"];
        if (!file.contains("name") || !file["name"].is_string() || !file.contains("path") ||
            !file["path"].is_string() || !file.contains("size") || !file["size"].is_number_integer()) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        RecordingInfo parsed;
        parsed.file.name = file["name"].get<std::string>();
        parsed.file.path = file["path"].get<std::string>();
        parsed.file.size = file["size"].get<int64_t>();
        parsed.message_count = json.value("message_count", int64_t{0});
        parsed.message = json.value("message", std::string{});
        if (parsed.file.name.empty() || parsed.file.path.empty() || parsed.file.size < 0 || parsed.message_count < 0) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        recording = std::move(parsed);
        return 0;
    } catch (const nlohmann::json::exception &) {
        return RPC_ERR_CLIENT_API_DATA;
    }
}

}  // namespace detail

class DataCollectionClient : public Client
{
public:
    DataCollectionClient() : Client(DATA_COLLECTION_SERVICE_NAME) {}
    ~DataCollectionClient() override = default;

    void Init() override
    {
        SetApiVersion(DATA_COLLECTION_API_VERSION);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_DATA_COLLECTION_START);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_DATA_COLLECTION_STOP);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_DATA_COLLECTION_GET_TOPICS);
        ZV_ROBOT_CLIENT_REG_API_NO_PROI(ROBOT_API_ID_DATA_COLLECTION_SET_TOPICS);

        auth_client_.Init();
        auth_client_.SetTimeout(30.0f);
    }

    int32_t StartRecording()
    {
        std::string data;
        return Call(ROBOT_API_ID_DATA_COLLECTION_START, "", data);
    }

    int32_t StopRecording(RecordingInfo & recording)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_DATA_COLLECTION_STOP, "", data);
        if (result != 0) return result;
        return detail::ParseRecordingResponse(data, recording);
    }

    int32_t GetRecordingTopics(TopicList & topics)
    {
        std::string data;
        const int32_t result = Call(ROBOT_API_ID_DATA_COLLECTION_GET_TOPICS, "", data);
        if (result != 0) return result;
        return detail::ParseTopicListResponse(data, topics);
    }

    int32_t SetRecordingTopics(const std::vector<std::string> & topics, TopicEditMode mode, bool persist,
                               TopicUpdateResult & result)
    {
        nlohmann::json parameter = {
            {"topics",  topics                    },
            {"mode",    static_cast<uint8_t>(mode)},
            {"persist", persist                   },
        };
        std::string data;
        const int32_t call_result = Call(ROBOT_API_ID_DATA_COLLECTION_SET_TOPICS, parameter.dump(), data);
        if (call_result != 0) return call_result;
        return detail::ParseTopicUpdateResponse(data, result);
    }

    int32_t DownloadRecording(const RecordingInfo & recording, const std::string & local_path,
                              DownloadProgressFn progress = nullptr)
    {
        if (recording.file.path.empty() || local_path.empty()) {
            return RPC_ERR_CLIENT_API_DATA;
        }

        bool acquired = false;
        if (!auth_client_.HasValidToken()) {
            FileAccessInfo access;
            const int32_t result = auth_client_.RequestFileAccess(access);
            if (result != 0) return result;
            acquired = true;
        }

        int32_t result = -1;
        try {
            result = auth_client_.DownloadFile(recording.file.path, local_path, progress);
        } catch (const std::exception &) {
            result = -1;
        }
        if (acquired) auth_client_.RevokeAccess();
        return result;
    }

    int32_t StopAndDownload(const std::string & local_path, RecordingInfo * recording = nullptr,
                            DownloadProgressFn progress = nullptr)
    {
        RecordingInfo stopped_recording;
        int32_t result = StopRecording(stopped_recording);
        if (result != 0) return result;

        if (recording) *recording = stopped_recording;
        result = DownloadRecording(stopped_recording, local_path, progress);
        return result;
    }

private:
    AuthClient auth_client_;
};

}  // namespace zv::robot
