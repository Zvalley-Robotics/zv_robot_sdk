# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import pathlib
import sys
import time

import zv_robot_sdk_python as zv


def add_pose_arguments(parser):
    parser.add_argument("x", type=float)
    parser.add_argument("y", type=float)
    parser.add_argument("z", type=float)
    parser.add_argument("roll", type=float)
    parser.add_argument("pitch", type=float)
    parser.add_argument("yaw", type=float)


def make_pose(args):
    pose = zv.NavPose()
    pose.position.x = args.x
    pose.position.y = args.y
    pose.position.z = args.z
    pose.euler.r = args.roll
    pose.euler.p = args.pitch
    pose.euler.y = args.yaw
    return pose


def parse_args():
    parser = argparse.ArgumentParser(description="ZV SLAM client example")
    parser.add_argument("--domain-id", type=int, default=0)
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--timeout", type=float, default=10.0)
    commands = parser.add_subparsers(dest="command", required=True)

    watch = commands.add_parser("watch", help="subscribe to live map, pose and SLAM status")
    watch.add_argument(
        "--transient-map",
        action="store_true",
        help="receive retained maps from transient-local publishers",
    )
    commands.add_parser("ping")
    start_mapping = commands.add_parser("start-mapping")
    start_mapping.add_argument("scene_id")
    start_mapping.add_argument("slam_type", choices=("indoor", "outdoor"))
    stop_mapping = commands.add_parser("stop-mapping")
    stop_mapping.add_argument("scene_id")
    terminate_mapping = commands.add_parser("terminate-mapping")
    terminate_mapping.add_argument("scene_id")
    restart_odom = commands.add_parser("restart-odom")
    restart_odom.add_argument("--scene-id", default="")
    restart_odom.add_argument("--no-relocalize", action="store_true")
    commands.add_parser("query-map")
    preview_map = commands.add_parser("preview-map")
    preview_map.add_argument("map_id")
    delete_map = commands.add_parser("delete-map")
    delete_map.add_argument("map_id")
    download_maps = commands.add_parser("download-maps")
    download_maps.add_argument("local_dir", type=pathlib.Path)

    init_pose = commands.add_parser("init-pose")
    init_pose.add_argument("scene_id")
    init_pose.add_argument("--auto", action="store_true")
    init_pose.add_argument(
        "--pose",
        nargs=6,
        type=float,
        metavar=("X", "Y", "Z", "ROLL", "PITCH", "YAW"),
    )

    start_nav = commands.add_parser("start-nav")
    start_nav.add_argument("--relative", action="store_true")
    add_pose_arguments(start_nav)

    commands.add_parser("get-state")
    commands.add_parser("get-nav-state")
    commands.add_parser("get-pose")
    commands.add_parser("pause-nav")
    commands.add_parser("resume-nav")
    commands.add_parser("cancel-nav")
    commands.add_parser("shutdown")
    return parser.parse_args()


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
        subscriber.Init(args.domain_id, args.network_interface, args.transient_map)
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
            while True:
                available, status = subscriber.TakeStatus()
                if not available:
                    break
                print(f"status: {status}")
            time.sleep(0.05)
    except KeyboardInterrupt:
        return 0
    finally:
        subscriber.Close()


def main():
    args = parse_args()
    if not 0 <= args.domain_id <= 232 or args.timeout <= 0:
        print("invalid domain ID or timeout", file=sys.stderr)
        return 1

    if args.command == "watch":
        return watch_topics(args)

    zv.ChannelFactory.Instance().Init(args.domain_id, args.network_interface)
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
    elif args.command == "restart-odom":
        ret, data = client.RestartOdom(args.scene_id, not args.no_relocalize)
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
            pose.position.x, pose.position.y, pose.position.z = args.pose[:3]
            pose.euler.r, pose.euler.p, pose.euler.y = args.pose[3:]
        if not args.auto and args.pose is None:
            print("init-pose requires --auto or --pose", file=sys.stderr)
            return 1
        ret, data = client.InitPose(args.scene_id, args.auto, pose)
    elif args.command == "start-nav":
        ret, data = client.PoseNav(args.relative, make_pose(args))
    elif args.command == "get-state":
        ret, data = client.GetState()
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
