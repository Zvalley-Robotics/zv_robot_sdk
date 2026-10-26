#pragma once

#include <zv/robot/client/client_base.hpp>

#define ZV_ROBOT_CLIENT_REG_API_NO_PROI(apiId) ZV_ROBOT_CLIENT_REG_API(apiId, 0)

#define ZV_ROBOT_CLIENT_REG_API(apiId, priority) RegistApi(apiId, priority)

namespace zv::robot
{

class Client : public ClientBase
{
public:
    explicit Client(const std::string & name);
    virtual ~Client();

    const std::string & GetApiVersion() const;
    int32_t GetServerApiVersion(std::string & version);
    std::string GetServerApiVersion();
    int32_t Ping();
    int32_t Ping(PingResult & result);
    TopicMatchInfo GetTopicMatchInfo() const;

protected:
    void SetApiVersion(const std::string & apiVersion);

    int32_t Call(int32_t apiId, const std::string & parameter, std::string & data);
    int32_t Call(int32_t apiId, const std::string & parameter);

    int32_t Call(int32_t apiId, const std::vector<uint8_t> & parameter, std::vector<uint8_t> & data);
    int32_t Call(int32_t apiId, const std::vector<uint8_t> & parameter);

    int32_t Call(int32_t apiId, const std::string & parameter, const std::vector<uint8_t> & binary);
    int32_t Call(int32_t apiId, const std::string & parameter, const std::vector<uint8_t> & binary, std::string & data);

    void RegistApi(int32_t apiId, int32_t priority = 0);
    int32_t CheckApi(int32_t apiId, int32_t & priority);

private:
    std::string mApiVersion;
    std::unordered_map<int32_t, int32_t> mApiMap;
};

using ClientPtr = std::shared_ptr<Client>;

}  // namespace zv::robot
