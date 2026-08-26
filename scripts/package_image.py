#!/usr/bin/env python3
import argparse
import binascii
import struct
from pathlib import Path

HEADER = struct.Struct("<IHBBHHHIII6s")
MAGIC = 0x3141544F
SCHEMA_VERSION = 1
HEADER_PAGE_SIZE = 1024
SLOTS = {"A": (0, 0x00008400), "B": (1, 0x00024000)}
MAX_PAYLOAD_SIZE = 110 * 1024


def parse_version(value):
    try:
        parts = tuple(int(part) for part in value.split("."))
    except ValueError as error:
        raise argparse.ArgumentTypeError("version must be X.Y.Z") from error
    if len(parts) != 3 or any(part < 0 or part > 0xFFFF for part in parts):
        raise argparse.ArgumentTypeError("version components must be unsigned 16-bit values")
    return parts


def validate_vector(payload, payload_start):
    if len(payload) < 8:
        raise ValueError("application payload must contain a vector table")
    msp, reset = struct.unpack_from("<II", payload)
    if msp & 3 or not 0x20000000 <= msp <= 0x20008000:
        raise ValueError("invalid initial MSP")
    if not reset & 1 or not payload_start <= (reset & ~1) < payload_start + MAX_PAYLOAD_SIZE:
        raise ValueError("invalid reset vector for target slot")


def package(slot, version, payload):
    target, payload_start = SLOTS[slot]
    if not payload or len(payload) > MAX_PAYLOAD_SIZE:
        raise ValueError("payload size must be between 1 and 112640 bytes")
    validate_vector(payload, payload_start)
    header = bytearray(HEADER.pack(MAGIC, SCHEMA_VERSION, target, 0, *version, len(payload), binascii.crc32(payload), 0, b"\0" * 6))
    struct.pack_into("<I", header, 22, binascii.crc32(header))
    return bytes(header) + b"\xff" * (HEADER_PAGE_SIZE - len(header)) + payload


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--slot", choices=SLOTS)
    parser.add_argument("--version", type=parse_version, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.write_bytes(package(args.slot, args.version, args.input.read_bytes()))


if __name__ == "__main__":
    main()
