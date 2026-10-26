#pragma once

#include <cstdint>
#include <string>

namespace zv::robot
{

struct TopicMatchInfo
{
    std::string request_topic;
    std::string response_topic;
    int32_t request_subscriber_count = 0;
    int32_t response_publisher_count = 0;

    bool RequestMatched() const { return request_subscriber_count > 0; }

    bool ResponseMatched() const { return response_publisher_count > 0; }

    bool AllMatched() const { return RequestMatched() && ResponseMatched(); }
};

struct PingResult
{
    int32_t code = 0;
    std::string message;
    bool reachable = false;
    TopicMatchInfo topics;
    std::string client_api_version;
    std::string server_api_version;
    bool api_version_match = false;
    int64_t latency_us = -1;
};

}  // namespace zv::robot
