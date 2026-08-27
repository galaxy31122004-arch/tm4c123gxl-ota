import argparse
import sys
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pc_tool" / "src"))

from tm4c_ota.client import DATA, RESET, START_UPDATE, OtaClient
from tm4c_ota.transport import RetryExhausted, SerialTransport


def main() -> int:
    parser = argparse.ArgumentParser(description="Reset TM4C during a partial OTA transfer.")
    parser.add_argument("--port", required=True)
    parser.add_argument("--image", type=Path, default=ROOT / "artifacts" / "ota_slot_b_v1.0.1.bin")
    args = parser.parse_args()
    image = args.image.read_bytes()

    port = serial.Serial(args.port, 115200, timeout=0.1)
    try:
        client = OtaClient(SerialTransport(port))
        client.get_info()
        client._request(START_UPDATE, image[:32], 90.0)
        print("START_UPDATE acknowledged")
        client._request(DATA, image[1024:1280])
        print("first DATA acknowledged: 256/376")
        try:
            client._request(RESET)
        except RetryExhausted:
            print("RESET disconnected the transport as expected")
    finally:
        port.close()

    time.sleep(0.75)
    port = serial.Serial(args.port, 115200, timeout=0.1)
    try:
        info = OtaClient(SerialTransport(port)).get_info()
    finally:
        port.close()

    if info["active_slot"] != 0 or info["pending_slot"] != 0xFF or info["update_progress"] != 0:
        raise RuntimeError(f"partial update survived reset: {info}")
    print(f"PASS: reset discarded partial update: {info}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
