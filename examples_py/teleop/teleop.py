# Resolve the packaged SDK when this example is run directly.
import pathlib as _sdk_pathlib
import sys as _sdk_sys
_sdk_sys.path.insert(0, str(_sdk_pathlib.Path(__file__).resolve().parents[2] / "libs" / "python"))

import argparse
import math
import json

import zv_robot_sdk_python as zv


def main():
    parser = argparse.ArgumentParser(description="Teleop services via zv_api DDS-RPC")
    parser.add_argument("command", choices=["ping", "start", "stop", "get-device-list", "switch-devices"])
    parser.add_argument("--domain-id", type=int, default=0)
    parser.add_argument("--network-interface", default="")
    parser.add_argument("--timeout", type=float, default=30.0)
    for name in ("source", "task-id", "device-id", "broker-host", "product-id",
                 "peripheral-type", "arm-mode", "takeover-mode", "reserve"):
        parser.add_argument("--" + name, default="")
    parser.add_argument("--broker-port", type=int, default=0)
    parser.add_argument("--header-sec", type=int, default=0)
    parser.add_argument("--header-nanosec", type=int, default=0)
    parser.add_argument("--devices-info", default="[]", help="JSON array of device_type/device_id objects")
    args = parser.parse_args()
    if args.command in ("start", "stop"):
        for name in ("source", "task_id"):
            if not getattr(args, name).strip():
                parser.error(f"--{name.replace('_', '-')} is required and must not be blank")
    if not 0 <= args.domain_id <= 232 or not math.isfinite(args.timeout) or args.timeout <= 0:
        parser.error("invalid domain ID or timeout")
    if not -(2**31) <= args.header_sec < 2**31 or not 0 <= args.header_nanosec < 10**9:
        parser.error("invalid header time")
    if not -(2**31) <= args.broker_port < 2**31:
        parser.error("broker-port must fit int32")
    request = zv.TeleopRequest()
    header = zv.ZVHeader_()
    stamp = zv.Time_()
    stamp.sec_ = args.header_sec
    stamp.nanosec_ = args.header_nanosec
    header.time_ = stamp
    header.reserve_ = args.reserve
    request.zv_header = header
    for name in ("source", "task_id", "device_id", "broker_host", "broker_port",
                 "product_id", "peripheral_type", "arm_mode", "takeover_mode"):
        setattr(request, name, getattr(args, name))
    switch_request = zv.TeleopSwitchDevicesRequest()
    if args.command == "switch-devices":
        switch_request.zv_header = request.zv_header
        switch_request.source = args.source
        try:
            devices = json.loads(args.devices_info)
            if not isinstance(devices, list):
                raise ValueError("expected an array")
            infos = []
            for device in devices:
                if not isinstance(device, dict) or any(
                    not isinstance(device.get(field), str) for field in ("device_type", "device_id")
                ):
                    raise ValueError("each device needs string device_type and device_id")
                info = zv.TeleopDeviceSwitch()
                info.device_type = device["device_type"]
                info.device_id = device["device_id"]
                infos.append(info)
            switch_request.devices_info = infos
        except ValueError as error:
            parser.error(f"invalid --devices-info: {error}")
    zv.ChannelFactory.Instance().Init(args.domain_id, args.network_interface)
    client = zv.TeleopClient()
    client.Init()
    client.SetTimeout(args.timeout)
    if args.command == "ping":
        code, result = client.Ping()
        print(f"rpc_code={code}, {result.message}")
        return 0 if code == zv.RPC_OK else 1
    if args.command == "get-device-list":
        query = zv.TeleopDeviceListRequest()
        query.zv_header = request.zv_header
        query.source = args.source
        code, result = client.GetDeviceList(query)
        print(f"rpc_code={code}, success={result.success}, message={result.message}, "
              f"source={result.source}, request_trace_id={result.request_trace_id}, "
              f"response_trace_id={result.zv_header.trace_id_}, "
              f"data_source_type={result.data_source_type}, node_name={result.status_list.node_name}")
        for device in result.status_list.status_list:
            print(f"type={device.type}, device_id={device.device_id}, device_no={device.device_no}, "
                  f"is_active={device.is_active}, status_code={device.status_code}, "
                  f"status_message={device.status_message}")
        return 0 if code == zv.RPC_OK and result.success else 1
    if args.command == "switch-devices":
        code, result = client.SwitchDevices(switch_request)
        print(f"rpc_code={code}, success={result.success}, message={result.message}, "
              f"source={result.source}, request_trace_id={result.request_trace_id}, "
              f"response_trace_id={result.zv_header.trace_id_}")
        return 0 if code == zv.RPC_OK and result.success else 1
    code, result = (client.Start if args.command == "start" else client.Stop)(request)
    print(f"rpc_code={code}, success={result.success}, message={result.message}, "
          f"source={result.source}, request_trace_id={result.request_trace_id}, "
          f"response_trace_id={result.trace_id}")
    return 0 if code == zv.RPC_OK and result.success else 1


if __name__ == "__main__":
    raise SystemExit(main())
