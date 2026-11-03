"""Connection options shared by the standalone Python topic examples."""

import argparse
from pathlib import Path
import re


def parser(description):
    result = argparse.ArgumentParser(description=description, allow_abbrev=False)
    result.add_argument("--use_config", "--use-config", action="store_true")
    result.add_argument("--network_interface", "--network-interface", default="")
    return result


def validate(args):
    if args.network_interface and not re.fullmatch(r"[A-Za-z0-9_.:-]{1,15}", args.network_interface):
        raise ValueError("invalid network interface")


def init(zv, args):
    validate(args)
    if args.use_config:
        zv.ChannelFactory.Instance().Init(str(Path(__file__).resolve().parent / "config/dds.json"))
    else:
        zv.ChannelFactory.Instance().Init(0, args.network_interface)
