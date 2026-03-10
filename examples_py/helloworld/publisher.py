import zv_robot_sdk_python as zv
import time

print(f"zv_robot_sdk_python version: {zv.__version__}")

HELLO_WORLD_TOPIC = "rt/hello_world"

zv.ChannelFactory.Instance().Init(0)
publisher = zv.ChannelPublisher_String_(HELLO_WORLD_TOPIC)
publisher.InitChannel()

count = 0
while True:
    msg = zv.String_(f"HelloWorld. {count}")
    count += 1
    publisher.Write(msg)
    time.sleep(1)
