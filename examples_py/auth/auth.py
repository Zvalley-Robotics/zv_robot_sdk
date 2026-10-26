# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import sys

import zv_robot_sdk_python as zv


def parse_args():
    parser = argparse.ArgumentParser(description="ZV auth client example")
    parser.add_argument("--domain-id", type=int, default=0)
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--timeout", type=float, default=10.0)
    commands = parser.add_mutually_exclusive_group(required=True)
    commands.add_argument(
        "--request-access",
        nargs="?",
        const="file",
        metavar="TYPE",
        help="request raw access metadata (default type: file)",
    )
    commands.add_argument(
        "--request-file-access",
        action="store_true",
        help="request parsed file access metadata and revoke it",
    )
    commands.add_argument(
        "--download",
        nargs=2,
        metavar=("REMOTE_PATH", "LOCAL_PATH"),
        help="request file access, download a file, then revoke access",
    )
    return parser.parse_args()


def report_error(operation, ret):
    print(
        f"{operation} failed: code={ret}, message={zv.RpcErrorDescription(ret)}",
        file=sys.stderr,
    )


def main():
    args = parse_args()
    if not 0 <= args.domain_id <= 232:
        print("domain ID must be in [0, 232]", file=sys.stderr)
        return 1
    if args.timeout <= 0:
        print("timeout must be positive", file=sys.stderr)
        return 1

    zv.ChannelFactory.Instance().Init(args.domain_id, args.network_interface)
    client = zv.AuthClient()
    client.Init()
    client.SetTimeout(args.timeout)

    if args.request_access is not None:
        ret, response = client.RequestAccess(args.request_access)
        if ret != zv.RPC_OK:
            report_error("RequestAccess", ret)
            return 1
        print(
            f"RequestAccess succeeded: type={args.request_access}, "
            f"response_bytes={len(response)}"
        )
        if client.HasValidToken():
            revoke_ret = client.RevokeAccess()
            if revoke_ret != zv.RPC_OK:
                report_error("RevokeAccess", revoke_ret)
                return 1
            print("Access revoked.")
        return 0

    ret, access = client.RequestFileAccess()
    if ret != zv.RPC_OK:
        report_error("RequestFileAccess", ret)
        return 1

    print(
        f"Access granted: token_received={bool(access.token)}, "
        f"base_url={client.base_url}, expires_at={access.expires_at}, "
        f"ttl_seconds={access.ttl_seconds}"
    )

    exit_code = 0
    if args.download is not None:
        remote_path, local_path = args.download
        last_percent = -1

        def progress(downloaded, total):
            nonlocal last_percent
            if total <= 0:
                return
            percent = downloaded * 100 // total
            if percent != last_percent:
                print(f"\rDownloading: {percent}%", end="", flush=True)
                last_percent = percent

        ret = client.DownloadFile(remote_path, local_path, progress)
        if last_percent >= 0:
            print()
        if ret != zv.RPC_OK:
            print(f"DownloadFile failed: code={ret}", file=sys.stderr)
            exit_code = 1
        else:
            print(f"Downloaded to {local_path}")

    revoke_ret = client.RevokeAccess()
    if revoke_ret != zv.RPC_OK:
        report_error("RevokeAccess", revoke_ret)
        exit_code = 1
    else:
        print("Access revoked.")
    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
