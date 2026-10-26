import zv_robot_sdk_python as sdk

# 1. 打开本地 MCAP 文件，退出上下文时自动关闭
with sdk.McapReader("capture.mcap") as reader:
    # 2. 逐条读取消息并输出话题、记录时间和原始数据长度
    for message in reader:
        print(message.topic, message.log_time, len(message.data))
