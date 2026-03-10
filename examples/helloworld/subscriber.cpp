#include <thread>

#include <zv/robot/channel/channel_subscriber.hpp>
#include "String_.hpp"

static const std::string HELLO_WORLD_TOPIC = "rt/chatter";   // ros2 run demo_nodes_cpp talker 

using namespace zv::robot;
using namespace zv::common;

void Handler(const void* msg)
{
    const std_msgs::msg::dds_::String_* hello_world_msg = (const std_msgs::msg::dds_::String_*)msg;

    std::cout << "message:" << hello_world_msg->data_() << std::endl;
}

int main()
{
    ChannelFactory::Instance()->Init(0);
    ChannelSubscriber<std_msgs::msg::dds_::String_> subscriber(HELLO_WORLD_TOPIC);
    subscriber.InitChannel(Handler);

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}
