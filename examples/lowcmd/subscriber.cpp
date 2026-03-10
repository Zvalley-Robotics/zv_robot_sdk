#include <chrono>
#include <thread>

#include <zv/robot/channel/channel_subscriber.hpp>
#include "AllJointState_.hpp"

static const std::string LOW_STATE_TOPIC = "rt/all_joint_state";

using namespace zv::robot;
using namespace zv::common;


void Handler(const void* msg)
{
    const robot_msgs::msg::dds_::AllJointState_* joint_state_msg = (const robot_msgs::msg::dds_::AllJointState_*)msg;
    int64_t timestamp = joint_state_msg->timestamp_();
    int64_t index = joint_state_msg->index_();
    std::vector<::robot_msgs::msg::dds_::JointState_> joint_states = joint_state_msg->joint_states_();

    std::cout << "[Subscriber] Message received msg: " << index << ", timestamp: " << timestamp << std::endl;
    for (int i = 0; i < joint_states.size(); i++)
    {
        std::cout << "    joint " << i << " positions : " << joint_states[i].joint_pos_() << std::endl;
    }
}

int main()
{
    ChannelFactory::Instance()->Init(0);
    ChannelSubscriber<robot_msgs::msg::dds_::AllJointState_> subscriber(LOW_STATE_TOPIC);
    subscriber.InitChannel(Handler);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}