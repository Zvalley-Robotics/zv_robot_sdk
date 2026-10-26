"""Subscribe to compressed camera topics; no GUI/ROS/OpenCV dependency."""

# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import math
from pathlib import Path
import re
import signal
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--domain-id", "--domain_id", type=int, default=0)
    parser.add_argument("--network-interface", "--network_interface", default="")
    parser.add_argument("--camera", action="append", default=[], metavar="NAME=DDS_TOPIC")
    parser.add_argument("--output-dir", type=Path, help="save the latest <camera>.jpg")
    parser.add_argument("--duration", type=float, default=0, help="seconds; 0 runs until Ctrl-C")
    args = parser.parse_args()
    if not math.isfinite(args.duration) or args.duration < 0:
        parser.error("duration must be a finite nonnegative number")
    topics = {}
    for spec in args.camera:
        name, sep, topic = spec.partition("=")
        if not sep or not re.fullmatch(r"[A-Za-z0-9_-]+", name) or not topic or name in topics:
            parser.error("expected unique --camera NAME=DDS_TOPIC")
        topics[name] = topic
    if not topics:
        topics = {name: f"rt/media/camera/{name}/color/image_raw/compressed"
                  for name in ("head", "left", "right", "chest")}
    import zv_robot_sdk_python as zv
    subscriber = zv.CameraSubscriber()
    running = True

    def stop(*_):
        nonlocal running
        running = False

    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    try:
        subscriber.Init(args.domain_id, args.network_interface, topics)
        if args.output_dir:
            args.output_dir.mkdir(parents=True, exist_ok=True)
        counts = dict.fromkeys(topics, 0)
        started = time.monotonic()
        logged = started - 1
        while running and (not args.duration or time.monotonic() - started < args.duration):
            now = time.monotonic()
            for name, topic in topics.items():
                available, frame = subscriber.TakeFrame(name)
                if available:
                    counts[name] += 1
                    data = frame.data
                    if args.output_dir and data.startswith(b"\xff\xd8") and data.endswith(b"\xff\xd9"):
                        path = args.output_dir / f"{name}.jpg"
                        temporary = path.with_suffix(".jpg.tmp")
                        temporary.write_bytes(data)
                        temporary.replace(path)
                if now - logged >= 1:
                    print(f"{name} publishers={subscriber.GetPublisherCount(name)} "
                          f"frames={counts[name]} topic={topic}", flush=True)
            if now - logged >= 1:
                logged = now
            time.sleep(0.1)
    finally:
        subscriber.Close()


if __name__ == "__main__":
    main()
