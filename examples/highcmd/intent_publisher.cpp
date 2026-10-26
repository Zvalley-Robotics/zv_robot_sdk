#include <unistd.h>

#include <zv/robot/channel/channel_publisher.hpp>

#include "RobotIntent_.hpp"

static const std::string INTENT_TOPIC = "rt/robot/input/intent";

using namespace zv::robot;
using namespace zv::common;

int main()
{
    // 1. 初始化 Channel 工厂（进程级一次）
    ChannelFactory::Instance()->Init(0);

    // 2. 创建并初始化 RobotIntent 发布者
    ChannelPublisher<robot_interfaces::msg::dds_::RobotIntent_> publisher(INTENT_TOPIC);
    publisher.InitChannel();

    // 3. 切换到 ready 状态：intent_type=status，command=ready
    robot_interfaces::msg::dds_::RobotIntent_ intent;
    intent.source_("sdk");
    intent.priority_(50);
    intent.intent_type_("status");
    intent.command_("ready");
    publisher.Write(intent);
    sleep(2);

    // 4. 切换动作模型：intent_type=model，command=switch_model，action=walk
    intent.intent_type_("model");
    intent.command_("switch_model");
    intent.action_("walk");
    publisher.Write(intent);
    sleep(2);

    // 5. 连续前进 0.3m/s：intent_type=motion，command=walk，约 20Hz 持续下发
    intent.intent_type_("motion");
    intent.command_("walk");
    intent.action_("");
    intent.linear_x_mps_(0.3F);
    intent.linear_y_mps_(0.0F);
    intent.angular_z_radps_(0.0F);
    for (int i = 0; i < 200; ++i) {
        publisher.Write(intent);
        usleep(50000);
    }

    // 6. 停止：下发零速度
    intent.linear_x_mps_(0.0F);
    intent.linear_y_mps_(0.0F);
    intent.angular_z_radps_(0.0F);
    publisher.Write(intent);
    return 0;
}
