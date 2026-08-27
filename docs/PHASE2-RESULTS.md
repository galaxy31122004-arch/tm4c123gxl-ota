# Phase 2 Verification Results

> Current phase ordering and status are maintained in `OTA-PLAN.md`. This file
> records evidence only.

Date: 2026-08-27

## Local Verification

- MQTT attributes, telemetry, RPC topic names and OTA state machine are defined in `PHASE2-CONTRACT.md`.
- Firmware metadata validation enforces version, positive size, checksum/file fields and ISO release date.
- Simulator scenarios `success`, `error`, and `rollback` emit deterministic JSON payloads.
- Dashboard template contains device, firmware, slot, OTA state, progress and error widgets.
- Python tests: 16 passed.
- Host CTest: 11/11 passed.

Run the simulator locally:

```powershell
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario success
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario error
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario rollback
```

## Cloud Verification Status

ThingsBoard device creation and broker publish have now been verified with the device
`TM4C123GXL-OTA-01` using the Phase 2 simulator. The observed latest telemetry was:

```text
active_slot       B
app_version       1.0.1
bootloader_version 1.0.0
ota_progress      100
ota_state         SUCCESS
ota_error         0
```

The `error` scenario was accepted with `ota_state=ERROR`, `ota_error=2`.
The `rollback` scenario was accepted with `ota_state=ROLLBACK`, `ota_error=9`,
`active_slot=A`, and `app_version=1.0.0`.

Dashboard widgets have been created on the tenant and confirmed to read device data.

The TM4C/ESP-AT firmware has also reached `MQTT_RPC_READY` on COM7 after joining
Wi-Fi, connecting MQTT, subscribing to the RPC request topic, and publishing its
initial `IDLE` telemetry. This verifies the device-to-cloud hardware path.

The real cloud-to-device `GET_INFO` round-trip remains pending. Acceptance
requires both `RPC_GET_INFO_RESPONSE_SENT` on COM7 and a ThingsBoard response
with the matching request ID. Simulator-only evidence is not sufficient.

The simulator supports broker publishing with:

```powershell
pip install -e ".\pc_tool[mqtt]"
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --host THINGSBOARD_HOST --token DEVICE_ACCESS_TOKEN
```

TLS is intentionally out of scope for Phase 2 and starts in Phase 3.
