#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <zv/idl/ros2/DiagnosticArray_.hpp>

namespace zv::robot
{

// ROS 2 /diagnostics_agg (diagnostic_msgs/msg/DiagnosticArray).
inline constexpr const char * DIAGNOSTICS_TOPIC = "rt/diagnostics_agg";

// 独立 DDS 订阅；非阻塞读取最新一帧，不在 DDS 线程中执行应用回调。
class DiagnosticsSubscriber
{
public:
    DiagnosticsSubscriber();
    ~DiagnosticsSubscriber();
    DiagnosticsSubscriber(const DiagnosticsSubscriber &) = delete;
    DiagnosticsSubscriber & operator=(const DiagnosticsSubscriber &) = delete;

    // domain_id=0 跟随 ZV_DDS_DOMAIN_ID；空网卡名自动选择。
    void Init(int32_t domain_id = 0, const std::string & network_interface = "");
    void Init(const std::string & config_path);
    void Close();
    // 返回完整聚合快照，包括 OK/WARN/ERROR/STALE；无新消息时重置 output。
    bool Take(diagnostic_msgs::msg::dds_::DiagnosticArray_ & output);
    int32_t GetPublisherCount() const;

private:
    void InitConfigured(int32_t domain_id, const std::string & network_interface, const std::string & config_path);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace zv::robot
