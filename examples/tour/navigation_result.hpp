#pragma once

#include <deque>
#include <mutex>
#include <string>
#include <utility>
#include <zv/robot/internal/internal.hpp>
#include <zv/robot/slam/slam_api.hpp>

namespace tour
{
struct NavigationResult
{
    int32_t code;
    std::string data;
};

// 普通 RPC 在收到“接受”后返回；保留 zv_api 随后发出的同请求终态。
class NavigationResults
{
public:
    void Push(const zv::robot::Response_ & response)
    {
        if (response.header_().identity_().api_id_() != zv::robot::ROBOT_API_ID_SLAM_POSE_NAV ||
            response.header_().status_().code_() == 0) return;
        std::lock_guard<std::mutex> lock(mutex_);
        if (results_.size() == 64) results_.pop_front();
        results_.push_back({std::to_string(response.header_().identity_().id_()),
            {response.header_().status_().code_(), response.data_()}});
    }

    void Reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        results_.clear();
    }

    bool Take(const std::string & request_id, NavigationResult & output)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!results_.empty()) {
            auto result = std::move(results_.front());
            results_.pop_front();
            if (result.first == request_id) {
                output = std::move(result.second);
                return true;
            }
        }
        return false;
    }

private:
    std::mutex mutex_;
    std::deque<std::pair<std::string, NavigationResult>> results_;
};
}  // namespace tour
