import argparse
import sys
import time
from pathlib import Path

import serial

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "pc_tool" / "src"))

from tm4c_ota.client import DATA, RESET, START_UPDATE, OtaClient
from tm4c_ota.transport import RetryExhausted, SerialTransport


def get_info(port_name: str):
    port = serial.Serial(port_name, 115200, timeout=0.1)
    try:
        return OtaClient(SerialTransport(port)).get_info()
    finally:
        port.close()


def main() -> int:
    parser = argparse.ArgumentParser(description="Interrupt a partial OTA serial transfer.")
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
    finally:
        port.close()
    print("serial transport closed before final DATA and END_UPDATE")

    time.sleep(1.5)
    after_timeout = get_info(args.port)
    if after_timeout["active_slot"] != 0 or after_timeout["pending_slot"] != 0xFF:
        raise RuntimeError(f"partial update became bootable: {after_timeout}")
    print(f"PASS: timeout kept Slot A active: {after_timeout}")

    port = serial.Serial(args.port, 115200, timeout=0.1)
    try:
        try:
            OtaClient(SerialTransport(port))._request(RESET)
        except RetryExhausted:
            pass
    finally:
        port.close()
    time.sleep(0.75)
    after_reset = get_info(args.port)
    if after_reset["active_slot"] != 0 or after_reset["pending_slot"] != 0xFF or after_reset["update_progress"] != 0:
        raise RuntimeError(f"partial update survived reset: {after_reset}")
    print(f"PASS: reboot discarded partial update: {after_reset}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
