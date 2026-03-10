#include <thread>

#include <zv/robot/channel/channel_publisher.hpp>
#include "String_.hpp"

static const std::string HELLO_WORLD_TOPIC = "rt/hello_world";

using namespace zv::robot;
using namespace zv::common;

int main() {
    ChannelFactory::Instance()->Init(0);
    ChannelPublisher<std_msgs::msg::dds_::String_> publisher(HELLO_WORLD_TOPIC);

    publisher.InitChannel();

    int count = 0;
    while (true)
    {
        std_msgs::msg::dds_::String_ msg("HelloWorld. "  + std::to_string(count++));
        publisher.Write(msg);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
