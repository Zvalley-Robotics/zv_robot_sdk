# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import time

import zv_robot_sdk_python as zv

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
