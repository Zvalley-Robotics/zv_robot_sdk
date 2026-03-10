#include <thread>

#include <zv/robot/channel/channel_publisher.hpp>
#include "AllJointCmd_.hpp"

static const std::string LOW_CMD_TOPIC = "rt/all_joint_cmd";

using namespace zv::robot;
using namespace zv::common;

int main()
{
    ChannelFactory::Instance()->Init(0);
    ChannelPublisher<robot_msgs::msg::dds_::AllJointCmd_> publisher(
        LOW_CMD_TOPIC);

    publisher.InitChannel();

    while (true)
    {
        robot_msgs::msg::dds_::AllJointCmd_ msg;
        std::vector<::robot_msgs::msg::dds_::JointCmd_> v_msg_joint;
        for (int i = 0; i < 23; i++)
        {
            robot_msgs::msg::dds_::JointCmd_ msg_joint;
            msg_joint.joint_pos_(0.11111 * i / 2);
            msg_joint.joint_vel_(0.22222 * i / 3);
            msg_joint.joint_torque_(0.33333 * i / 4);
            msg_joint.kp_(0.444 * i / 4);
            msg_joint.kd_(0.55555 * i / 7 * 1.1);
            v_msg_joint.push_back(msg_joint);
        }

        publisher.Write(msg);

        std::cout << "test [tx] " << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
