# Phase 2 Verification Results

Date: 2026-08-27

## Local Verification

- MQTT attributes, telemetry, RPC topic names and OTA state machine are defined in `PHASE2-CONTRACT.md`.
- Firmware metadata validation enforces version, positive size, checksum/file fields and ISO release date.
- Simulator scenarios `success`, `error`, and `rollback` emit deterministic JSON payloads.
- Dashboard template contains device, firmware, slot, OTA state, progress and error widgets.
- Python tests: 15 passed.
- Host CTest: 11/11 passed.

Run the simulator locally:

```powershell
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario success
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario error
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario rollback
```

## Cloud Verification Status

ThingsBoard device creation, dashboard import, broker publish and RPC round-trip remain
pending until a ThingsBoard host and device access token are supplied. The simulator
supports those checks with:

```powershell
pip install -e ".\pc_tool[mqtt]"
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --host THINGSBOARD_HOST --token DEVICE_ACCESS_TOKEN
```

TLS is intentionally out of scope for Phase 2 and starts in Phase 3.
