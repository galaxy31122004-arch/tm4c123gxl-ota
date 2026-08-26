import argparse
import json

from .client import OtaClient
from .transport import SerialTransport


def main():
    parser = argparse.ArgumentParser(prog="tm4c-ota")
    parser.add_argument("--port", required=True)
    subcommands = parser.add_subparsers(dest="command", required=True)
    subcommands.add_parser("info")
    update = subcommands.add_parser("update")
    update.add_argument("image")
    subcommands.add_parser("reset")
    args = parser.parse_args()
    import serial
    with serial.Serial(args.port, 115200, timeout=0.1) as connection:
        client = OtaClient(SerialTransport(connection))
        if args.command == "info":
            print(json.dumps(client.get_info()))
        elif args.command == "update":
            client.update(open(args.image, "rb").read())
        else:
            client.reset()
