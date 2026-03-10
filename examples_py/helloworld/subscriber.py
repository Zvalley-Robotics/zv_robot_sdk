import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

HELLO_WORLD_TOPIC = "rt/chatter"


def Handler(hello_world_msg: zv.String_):
    print(f"message:{hello_world_msg.data_}")


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_String_(HELLO_WORLD_TOPIC)
subscriber.InitChannel(Handler)
while True:
    time.sleep(10)
