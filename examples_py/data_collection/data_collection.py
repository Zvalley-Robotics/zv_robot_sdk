"""数据采集示例，对应 examples/data_collection/data_collection.cpp。"""

# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import json
import pathlib
import sys

import zv_robot_sdk_python as zv


def parse_args():
    parser = argparse.ArgumentParser(
        description="ZV data collection client example", allow_abbrev=False
    )
    parser.add_argument("--network_interface", default="lo", metavar="IFACE")
    commands = parser.add_mutually_exclusive_group(required=True)
    commands.add_argument("--read", metavar="MCAP", help="read a local MCAP file offline")
    parser.add_argument("--read_after_download", action="store_true")
    commands.add_argument("--start", action="store_true", help="start recording")
    commands.add_argument(
        "--stop", nargs="?", const="", metavar="LOCAL_DIR",
        help="stop recording and download the MCAP file (default: current directory)",
    )
    commands.add_argument(
        "--get_topics", action="store_true", help="print configured recording topics"
    )
    commands.add_argument(
        "--set_topics", metavar="T1,T2,...",
        help="modify recording topics; an empty value clears the list in replace mode",
    )
    parser.add_argument(
        "--mode", choices=("replace", "add", "remove"),
        help="how to apply --set_topics (default: replace)",
    )
    parser.add_argument(
        "--persist", action="store_true", help="write the topic list to the config file"
    )
    if len(sys.argv) == 1:
        parser.print_help()
        parser.exit()
    args = parser.parse_args()
    if args.read == "":
        parser.error("--read requires a local MCAP file path")
    if args.read_after_download and args.stop is None:
        parser.error("--read_after_download only applies to --stop")
    if args.set_topics is None and (args.mode is not None or args.persist):
        parser.error("--mode/--persist only apply to --set_topics")
    return args


def print_topic_list(label, topics):
    # 与 C++ 示例保持一致，空列表仅输出标签及冒号。
    print(f"{label}:" + (" " + ",".join(topics) if topics else ""))


def report_error(action, result):
    print(f"Failed to {action} (code={result})", file=sys.stderr)
    return 1


def read_mcap(path):
    counts = {}
    count = 0
    previewed_topics = set()
    try:
        with zv.McapReader(str(path)) as reader:
            print(f"MCAP: {path}\nPreview: first 10 messages and first message per topic, up to 16 payload bytes")
            for message in reader:
                count += 1
                counts[message.topic] = counts.get(message.topic, 0) + 1
                first_for_topic = message.topic not in previewed_topics
                previewed_topics.add(message.topic)
                if count <= 10 or first_for_topic:
                    schema = message.schema_name if message.has_schema else "<none>"
                    schema_encoding = message.schema_encoding if message.has_schema else "<none>"
                    payload = message.data
                    print(
                        f"Message {count}: topic={message.topic} "
                        f"encoding={message.message_encoding} schema={schema} "
                        f"schema_encoding={schema_encoding} sequence={message.sequence} "
                        f"log_time_ns={message.log_time} publish_time_ns={message.publish_time} "
                        f"bytes={len(payload)}"
                    )
                    print(f"  payload_hex: {payload[:16].hex(' ')}" + (" ..." if len(payload) > 16 else ""))
                    print("  business_json: " + json.dumps(message.decoded, ensure_ascii=False))
    except RuntimeError as error:
        print(f"Failed to read MCAP: {error}", file=sys.stderr)
        return 1
    print(f"Messages: {count}")
    for topic, total in sorted(counts.items()):
        print(f"Topic: {topic} messages={total}")
    return 0


def main():
    args = parse_args()
    if args.read is not None:
        return read_mcap(args.read)
    zv.ChannelFactory.Instance().Init(0, args.network_interface)
    client = zv.DataCollectionClient()
    client.Init()
    client.SetTimeout(30.0)

    if args.get_topics:
        result, topics = client.GetRecordingTopics()
        if result != zv.RPC_OK:
            return report_error("get recording topics", result)
        print(f"Recording: {int(topics.recording)}")
        print_topic_list("Topics", topics.topics)
        print_topic_list("Subscribed", topics.subscribed_topics)
        print_topic_list("Skipped", topics.skipped_topics)
        return 0

    if args.set_topics is not None:
        modes = {
            "replace": zv.TopicEditMode.kReplace,
            "add": zv.TopicEditMode.kAdd,
            "remove": zv.TopicEditMode.kRemove,
        }
        topics = [topic for topic in args.set_topics.split(",") if topic]
        result, updated = client.SetRecordingTopics(
            topics, modes[args.mode or "replace"], args.persist
        )
        if result != zv.RPC_OK:
            return report_error("set recording topics", result)
        print_topic_list("Topics", updated.topics)
        print_topic_list("Invalid", updated.invalid_topics)
        return 0

    if args.start:
        result = client.StartRecording()
        if result != zv.RPC_OK:
            return report_error("start recording", result)
        print("Recording started.")
        return 0

    result, recording = client.StopRecording()
    if result != zv.RPC_OK:
        return report_error("stop recording", result)

    local_dir = pathlib.Path(args.stop) if args.stop else pathlib.Path.cwd()
    try:
        local_dir.mkdir(parents=True, exist_ok=True)
    except OSError as error:
        print(f"Cannot create download directory '{local_dir}': {error}", file=sys.stderr)
        return 1
    local_path = local_dir / recording.file.name

    def progress(downloaded, total):
        expected_total = total if total > 0 else recording.file.size
        if expected_total > 0:
            percentage = int(downloaded * 100 / expected_total)
            print(
                f"\rDownloading {recording.file.name}: {percentage}%",
                end="", flush=True,
            )

    result = client.DownloadRecording(recording, str(local_path), progress)
    if result != zv.RPC_OK:
        print()
        return report_error("download recording", result)
    print(
        f"\rDownloaded {recording.file.name} to {local_path} "
        f"({recording.file.size} bytes, {recording.message_count} messages)"
    )
    if args.read_after_download:
        return read_mcap(local_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
