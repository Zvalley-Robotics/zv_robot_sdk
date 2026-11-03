# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

DDS_CONFIG_PATH = _sdk_pathlib.Path(__file__).resolve().parents[1] / "config/dds.json"

import argparse
import pathlib
import sys

import zv_robot_sdk_python as zv


def parse_args():
    parser = argparse.ArgumentParser(description="ZV media streamer client example")
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--use-config", "--use_config", action="store_true",
                        help="use examples_py/config/dds.json; uses domain ID and network settings from the file")
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--trro-config-file", type=pathlib.Path)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("ping")

    for command in ("start-stream", "stop-stream"):
        stream = commands.add_parser(command)
        stream.add_argument("mode", choices=("local", "remote", "both"))
        stream.add_argument("cameras", nargs="+")

    commands.add_parser("start-audio")
    commands.add_parser("stop-audio")
    return parser.parse_args()


def main():
    args = parse_args()
    if args.timeout <= 0:
        print("invalid timeout", file=sys.stderr)
        return 1

    trro_config = ""
    if args.trro_config_file is not None:
        try:
            trro_config = args.trro_config_file.read_text(encoding="utf-8")
        except OSError as error:
            print(f"failed to read TRRO config: {error}", file=sys.stderr)
            return 1

    if args.use_config:
        zv.ChannelFactory.Instance().Init(str(DDS_CONFIG_PATH))
    else:
        zv.ChannelFactory.Instance().Init(0, args.network_interface)
    client = zv.MediaClient()
    client.Init()
    client.SetTimeout(args.timeout)

    if args.command == "ping":
        ret, result = client.Ping()
        print(result.message, file=sys.stdout if ret == zv.RPC_OK else sys.stderr)
        return 0 if ret == zv.RPC_OK else 1
    if args.command == "start-stream":
        ret, result = client.StartStream(args.cameras, args.mode, trro_config)
    elif args.command == "stop-stream":
        ret, result = client.StopStream(args.cameras, args.mode)
    elif args.command == "start-audio":
        ret, result = client.StartAudio()
    else:
        ret, result = client.StopAudio()

    output = sys.stdout if ret == zv.RPC_OK and result.success else sys.stderr
    print(
        f"{args.command}: rpc_code={ret}, success={result.success}, "
        f"message={result.message}",
        file=output,
    )
    return 0 if ret == zv.RPC_OK and result.success else 1


if __name__ == "__main__":
    raise SystemExit(main())
