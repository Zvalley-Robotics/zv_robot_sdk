#pragma once

#include <zv/robot/client/client_stub.hpp>

namespace zv::robot
{

const int64_t ROBOT_CLIENT_TIMEOUT = 1000000;  // 1s

class ClientBase
{
public:
    explicit ClientBase(const std::string & name);
    virtual ~ClientBase();

    virtual void Init() = 0;

    void SetTimeout(int64_t timeout);
    void SetTimeout(float timeout);

    /**
     * @brief 最近一次收到服务端响应时携带的错误描述。
     *
     * 从响应 data JSON 中读取 error（运动/数采/鉴权）或 message
     * （SLAM/媒体/语音/遥操作）字段；未收到响应或解析失败时返回空串。
     * 供 CLI/工具在错误码后附带服务端原因，不改变调用返回值。
     */
    std::string GetLastErrorMessage() const;

protected:
    int64_t GetTimeout() const;
    TopicMatchInfo GetTopicMatchInfo() const;
    bool WaitForTopicMatched(int64_t timeout) const;

    int32_t Call(int32_t apiId, const std::string & parameter, std::string & data, int32_t priority);
    int32_t Call(int32_t apiId, const std::string & parameter, int32_t priority);

    int32_t Call(int32_t apiId, const std::vector<uint8_t> & parameter, std::vector<uint8_t> & bin_data,
                 int32_t priority);
    int32_t Call(int32_t apiId, const std::vector<uint8_t> & parameter, int32_t priority);

    int32_t Call(int32_t apiId, const std::string & parameter, const std::vector<uint8_t> & binary, int32_t priority);
    int32_t Call(int32_t apiId, const std::string & parameter, const std::vector<uint8_t> & binary, std::string & data,
                 int32_t priority);

    int32_t Call(int32_t apiId, const std::string & parameter, std::string & data, int32_t priority, int64_t timeout);

    void SetHeader(RequestHeader_ & header, int32_t apiId, int32_t priority, bool noReply);

private:
    int64_t mTimeout;
    ClientStubPtr mClientStubPtr;
    std::string mLastResponseData;  ///< 最近一次响应的 data 原文，用于提取错误描述
};

using ClientBasePtr = std::shared_ptr<ClientBase>;

}  // namespace zv::robot
