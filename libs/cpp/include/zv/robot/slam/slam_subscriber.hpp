#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <zv/idl/ros2/MapMetaData_.hpp>
#include <zv/idl/ros2/PoseStamped_.hpp>

namespace zv::robot
{

inline constexpr const char * SLAM_MAP_TOPIC = "rt/map";
inline constexpr const char * SLAM_POSE_TOPIC = "rt/guide_tour/global_pose";
inline constexpr const char * SLAM_COLLISION_RISK_TOPIC = "rt/slam/collision_risk";

struct SlamMap
{
    std_msgs::msg::dds_::Header_ header;
    nav_msgs::msg::dds_::MapMetaData_ info;
    // ROS OccupancyGrid values: -1 unknown, 0..100 occupancy probability.
    std::vector<int8_t> data;
};

// Independent DDS participant; pass the same domain/interface as the RPC
// client. Polling only: no application/Python callbacks run on DDS threads.
class SlamSubscriber
{
public:
    SlamSubscriber();
    ~SlamSubscriber();
    SlamSubscriber(const SlamSubscriber &) = delete;
    SlamSubscriber & operator=(const SlamSubscriber &) = delete;

    // Volatile map QoS works with live publishers of either durability. Set
    // transient_map=true to receive retained maps from transient-local
    // publishers. domain_id=0 follows ZV_DDS_DOMAIN_ID, matching
    // ChannelFactory.
    void Init(int32_t domain_id = 0, const std::string & network_interface = "", bool transient_map = false);
    void Init(const std::string & config_path, bool transient_map);
    void Close();
    // Nonblocking, consume the newest sample; false resets output.
    bool TakeMap(SlamMap & output);
    bool TakePose(geometry_msgs::msg::dds_::PoseStamped_ & output);
    // Latest risk in [0,1]; no message means unavailable, never assume zero risk.
    bool TakeCollisionRisk(float & output);
    int32_t GetCollisionRiskPublisherCount() const;
    int32_t GetMapPublisherCount() const;
    int32_t GetPosePublisherCount() const;

private:
    void InitConfigured(int32_t domain_id, const std::string & network_interface, bool transient_map,
                        const std::string & config_path);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace zv::robot
