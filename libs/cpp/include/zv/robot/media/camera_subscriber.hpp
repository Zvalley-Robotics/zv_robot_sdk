#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace zv::robot
{
struct CameraFrame
{
    int64_t timestamp_ns{0};
    std::string frame_id;
    std::string format;
    std::vector<uint8_t> data;
};

// Owns an independent DDS participant. Polling only: application/Python
// callbacks never run on DDS threads. Does not control camera publishers.
class CameraSubscriber
{
public:
    CameraSubscriber();
    ~CameraSubscriber();
    CameraSubscriber(const CameraSubscriber &) = delete;
    CameraSubscriber & operator=(const CameraSubscriber &) = delete;

    // Topics are complete DDS names (including rt/ where applicable).
    // domain_id=0 follows ZV_DDS_DOMAIN_ID, like ChannelFactory.
    void Init(int32_t domain_id, const std::string & network_interface,
              const std::map<std::string, std::string> & camera_topics);
    void Close();
    // Nonblocking, consumes the newest sample. False resets output.
    bool TakeFrame(const std::string & camera_name, CameraFrame & output);
    int32_t GetPublisherCount(const std::string & camera_name) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}  // namespace zv::robot
