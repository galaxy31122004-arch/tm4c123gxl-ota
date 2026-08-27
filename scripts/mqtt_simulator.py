#!/usr/bin/env python3
"""Emit or publish deterministic ThingsBoard OTA contract scenarios."""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "pc_tool" / "src"))

from tm4c_ota.mqtt_contract import (  # noqa: E402
    ATTRIBUTES_TOPIC,
    DEVICE_NAME,
    ERROR_CODES,
    TELEMETRY_TOPIC,
    build_telemetry,
    validate_firmware_metadata,
    validate_transition,
)


def scenario_events(name: str) -> list[tuple[str, dict]]:
    now = datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")
    attributes = {
        "device_model": "TM4C123GXL",
        "hardware_version": "1.0",
        "bootloader_version": "1.0.0",
        "app_version": "1.0.0",
        "active_slot": "A",
    }
    metadata = {
        "firmware_version": "1.0.1",
        "firmware_size": 376,
        "firmware_checksum": "0x1234abcd",
        "firmware_file": "tm4c123gxl_v1.0.1.bin",
        "release_date": "2026-08-27",
    }
    validate_firmware_metadata(metadata)
    base = dict(app_version="1.0.0", bootloader_version="1.0.0", active_slot="A", device_uptime=123, last_update=now)
    states = {
        "success": [("CHECKING", 0, 0), ("DOWNLOADING", 0, 0), ("DOWNLOADED", 0, 0),
                    ("TRANSFERRING", 60, 0), ("VERIFYING", 100, 0), ("REBOOTING", 100, 0),
                    ("CONFIRMING", 100, 0), ("SUCCESS", 100, 0)],
        "error": [("CHECKING", 0, 0), ("DOWNLOADING", 20, 0), ("ERROR", 20, 2)],
        "rollback": [("CHECKING", 0, 0), ("DOWNLOADING", 100, 0), ("DOWNLOADED", 100, 0),
                      ("TRANSFERRING", 100, 0), ("VERIFYING", 100, 0), ("REBOOTING", 100, 0),
                      ("CONFIRMING", 100, 8), ("ROLLBACK", 0, 9)],
    }
    if name not in states:
        raise ValueError(f"unknown scenario: {name}")
    events = [(ATTRIBUTES_TOPIC, attributes)]
    previous = "IDLE"
    for state, progress, error in states[name]:
        validate_transition(previous, state)
        previous = state
        values = dict(base, ota_state=state, ota_progress=progress, ota_error=error)
        if state == "SUCCESS":
            values.update(app_version="1.0.1", active_slot="B", last_update=now)
        if state == "ROLLBACK":
            values.update(app_version="1.0.0", active_slot="A")
        events.append((TELEMETRY_TOPIC, build_telemetry(**values)))
    return events


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scenario", choices=("success", "error", "rollback"), default="success")
    parser.add_argument("--interval", type=float, default=0.0)
    parser.add_argument("--host", help="Optional MQTT broker host; offline JSON output is the default")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--token", help="ThingsBoard device access token")
    args = parser.parse_args()
    events = scenario_events(args.scenario)
    publisher = None
    if args.host:
        if not args.token:
            parser.error("--token is required with --host")
        try:
            import paho.mqtt.client as mqtt
        except ImportError as exc:
            raise SystemExit("Install paho-mqtt to publish to a broker") from exc
        publisher = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        publisher.username_pw_set(args.token)
        publisher.connect(args.host, args.port)
        publisher.loop_start()
    try:
        for topic, payload in events:
            if publisher:
                publisher.publish(topic, json.dumps(payload), qos=1).wait_for_publish()
            else:
                print(json.dumps({"device": DEVICE_NAME, "topic": topic, "payload": payload}, sort_keys=True))
            if args.interval:
                time.sleep(args.interval)
    finally:
        if publisher:
            publisher.loop_stop()
            publisher.disconnect()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
