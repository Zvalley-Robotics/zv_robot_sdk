# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import time

import zv_robot_sdk_python as zv

print(f"zv_robot_sdk_python version: {zv.__version__}")

HELLO_WORLD_TOPIC = "rt/chatter"


def Handler(hello_world_msg: zv.String_):
    print(f"message:{hello_world_msg.data_}")


zv.ChannelFactory.Instance().Init(0)
subscriber = zv.ChannelSubscriber_String_(HELLO_WORLD_TOPIC)
subscriber.InitChannel(Handler)
while True:
    time.sleep(10)
