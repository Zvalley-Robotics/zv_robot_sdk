# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
import os as _sdk_os
_sdk_bundled = str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python")
# Respect the explicit library selected by tools/PYTHONPATH before bundled fallback.
if _sdk_os.environ.get("PYTHONPATH"):
    _sdk_sys.path.append(_sdk_bundled)
else:
    _sdk_sys.path.insert(0, _sdk_bundled)

DDS_CONFIG_PATH = _sdk_pathlib.Path(__file__).resolve().parents[1] / "config/dds.json"

import argparse
import json
import pathlib
import sys
import time

import zv_robot_sdk_python as zv


def make_pose(values):
    pose = zv.NavPose()
    pose.position.x, pose.position.y, pose.position.z = values[:3]
    if len(values) == 7:
        # 七个值表示位置和四元数，顺序与 C++ 示例一致。
        pose.orientation.q_x, pose.orientation.q_y, pose.orientation.q_z, pose.orientation.q_w = values[3:]
    else:
        pose.euler.r, pose.euler.p, pose.euler.y = values[3:]
    return pose


def parse_args():
    parser = argparse.ArgumentParser(description="ZV SLAM client example")
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--use-config", "--use_config", action="store_true",
                        help="use examples_py/config/dds.json; uses domain ID and network settings from the file")
    parser.add_argument("--timeout", type=float, default=10.0)
    commands = parser.add_subparsers(dest="command", required=True)

    watch = commands.add_parser("watch", help="subscribe to live map and pose")
    watch.add_argument(
        "--transient-map",
        action="store_true",
        help="receive retained maps from transient-local publishers",
    )
    commands.add_parser("watch-risk", help="stream collision risk as JSON snapshots")
    commands.add_parser("ping")
    start_mapping = commands.add_parser("start-mapping")
    start_mapping.add_argument("scene_id")
    start_mapping.add_argument("slam_type", choices=("indoor", "outdoor"))
    stop_mapping = commands.add_parser("stop-mapping")
    stop_mapping.add_argument("scene_id")
    terminate_mapping = commands.add_parser("terminate-mapping")
    terminate_mapping.add_argument("scene_id")
    commands.add_parser("query-map")
    preview_map = commands.add_parser("preview-map")
    preview_map.add_argument("map_id")
    delete_map = commands.add_parser("delete-map")
    delete_map.add_argument("map_id")
    download_maps = commands.add_parser("download-maps")
    download_maps.add_argument("local_dir", nargs="?", type=pathlib.Path, default=pathlib.Path("."))
    get_map = commands.add_parser("get-map")
    get_map.add_argument("name")
    get_map.add_argument("local_path")

    init_pose = commands.add_parser("init-pose")
    init_pose.add_argument("scene_id")
    init_pose.add_argument("--auto", action="store_true")
    init_pose.add_argument(
        "--pose",
        nargs="+",
        type=float,
        metavar="VALUE",
        help="x y z roll pitch yaw or x y z q_x q_y q_z q_w",
    )

    start_nav = commands.add_parser("start-nav")
    start_nav.add_argument("--relative", action="store_true")
    start_nav.add_argument("pose", nargs="+", type=float, metavar="VALUE",
                           help="x y z roll pitch yaw or x y z q_x q_y q_z q_w")

    commands.add_parser("get-state", help="query state and scene_id from API 1815")
    commands.add_parser("get-nav-state")
    commands.add_parser("get-pose")
    commands.add_parser("pause-nav")
    commands.add_parser("resume-nav")
    commands.add_parser("cancel-nav")
    commands.add_parser("shutdown")
    args = parser.parse_args()
    if args.command in ("init-pose", "start-nav") and args.pose is not None and len(args.pose) not in (6, 7):
        parser.error("pose requires 6 values (Euler angles) or 7 values (quaternion)")
    return args


def report_result(command, ret, value=""):
    if ret not in (zv.RPC_OK, 1):
        message = (
            "application reported failure"
            if ret == 2
            else zv.RpcErrorDescription(ret)
        )
        print(
            f"{command} failed: code={ret}, message={message}",
            file=sys.stderr,
        )
        if value:
            print(f"Response Data: {value}", file=sys.stderr)
        return 1
    print(value if value else f"{command} succeeded")
    return 0


def watch_topics(args):
    subscriber = zv.SlamSubscriber()
    try:
        if args.use_config:
            subscriber.Init(str(DDS_CONFIG_PATH), args.transient_map)
        else:
            subscriber.Init(0, args.network_interface, args.transient_map)
        while True:
            available, grid = subscriber.TakeMap()
            if available:
                print(f"map: {grid.info.width_} x {grid.info.height_}, "
                      f"resolution={grid.info.resolution_}, cells={len(grid.data)}")
            available, pose = subscriber.TakePose()
            if available:
                point = pose.pose_.position_
                print(f"pose: frame={pose.header_.frame_id_}, "
                      f"position=({point.x_}, {point.y_}, {point.z_})")
            time.sleep(0.05)
    except KeyboardInterrupt:
        return 0
    finally:
        subscriber.Close()


def watch_risk(args):
    subscriber = zv.SlamSubscriber()
    risk, received = None, None
    has_risk = hasattr(subscriber, "TakeCollisionRisk")
    try:
        if args.use_config:
            subscriber.Init(str(DDS_CONFIG_PATH), False)
        else:
            subscriber.Init(0, args.network_interface, False)
        next_output = 0
        while True:
            now = time.monotonic()
            if has_risk:
                available, value = subscriber.TakeCollisionRisk()
                if available:
                    risk, received = value, now
            if now >= next_output:
                print(json.dumps({"kind": "slam-risk", "risk": risk,
                    "risk_age": now - received if received is not None else None,
                    "risk_supported": has_risk}), flush=True)
                next_output = now + 0.5
            time.sleep(0.05)
    except KeyboardInterrupt:
        return 0
    finally:
        subscriber.Close()


def main():
    args = parse_args()
    if args.timeout <= 0:
        print("invalid timeout", file=sys.stderr)
        return 1

    if args.command == "watch":
        return watch_topics(args)
    if args.command == "watch-risk":
        return watch_risk(args)

    if args.use_config:
        zv.ChannelFactory.Instance().Init(str(DDS_CONFIG_PATH))
    else:
        zv.ChannelFactory.Instance().Init(0, args.network_interface)
    client = zv.SlamClient()
    client.Init()
    client.SetTimeout(args.timeout)

    if args.command == "ping":
        ret, result = client.Ping()
        print(result.message, file=sys.stdout if ret == zv.RPC_OK else sys.stderr)
        return 0 if ret == zv.RPC_OK else 1
    if args.command == "start-mapping":
        ret, data = client.StartMapping(args.scene_id, args.slam_type)
    elif args.command == "stop-mapping":
        ret, data = client.StopMapping(args.scene_id)
    elif args.command == "terminate-mapping":
        ret, data = client.TerminateMapping(args.scene_id)
    elif args.command == "query-map":
        ret, files = client.QueryMap()
        if ret == zv.RPC_OK:
            for file in files:
                print(f"{file.name}\t{file.size}\t{file.path}")
        return report_result(args.command, ret, "no maps found" if not files else "")
    elif args.command == "preview-map":
        ret, data = client.PreviewMap(args.map_id)
    elif args.command == "delete-map":
        ret, data = client.DeleteMap(args.map_id)
    elif args.command == "get-map":
        file = zv.FileInfo()
        file.name = args.name
        ret = client.GetMap(file, args.local_path)
        if ret != 0:
            print(f"get-map failed: code={ret}", file=sys.stderr)
            return 1
        return 0
    elif args.command == "download-maps":
        args.local_dir.mkdir(parents=True, exist_ok=True)
        ret, ok_count, fail_count = client.DownloadMaps(str(args.local_dir))
        print(f"downloaded={ok_count}, failed={fail_count}")
        if ret < 0:
            print(
                f"download-maps failed: code={ret}, "
                f"message={zv.RpcErrorDescription(ret)}",
                file=sys.stderr,
            )
        return 0 if ret == 0 else 1
    elif args.command == "init-pose":
        pose = zv.NavPose()
        if args.pose is not None:
            pose = make_pose(args.pose)
        if not args.auto and args.pose is None:
            print("init-pose requires --auto or --pose", file=sys.stderr)
            return 1
        ret, data = client.InitPose(args.scene_id, args.auto, pose)
    elif args.command == "start-nav":
        ret, data = client.PoseNav(args.relative, make_pose(args.pose))
    elif args.command == "get-state":
        ret, state, scene_id = client.GetState(include_scene=True)
        data = json.dumps({"state": state, "scene_id": scene_id or None})
    elif args.command == "get-nav-state":
        ret, data = client.GetNavState()
    elif args.command == "get-pose":
        ret, data = client.GetPoseState()
    elif args.command == "pause-nav":
        ret, data = client.PauseNav()
    elif args.command == "resume-nav":
        ret, data = client.ResumeNav()
    elif args.command == "cancel-nav":
        ret, data = client.CancelNav()
    else:
        ret, data = client.Close()
    return report_result(args.command, ret, data)


if __name__ == "__main__":
    raise SystemExit(main())
