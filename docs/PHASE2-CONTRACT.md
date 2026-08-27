# Phase 2 Cloud Contract

This contract is the interface between the future ESP32 ESP-AT bridge and ThingsBoard Cloud.
Phase 2 uses plain MQTT for validation; TLS is deferred to Phase 3.

## Device

Device name: `TM4C123GXL-OTA-01`  
Model: `TM4C123GXL`  
Hardware: `1.0`  
Bootloader: `1.0.0`

Attributes are published to `v1/devices/me/attributes`:

```json
{"device_model":"TM4C123GXL","hardware_version":"1.0","bootloader_version":"1.0.0","app_version":"1.0.0","active_slot":"A"}
```

## MQTT Topics

| Purpose | Topic |
| --- | --- |
| Attributes | `v1/devices/me/attributes` |
| Telemetry | `v1/devices/me/telemetry` |
| RPC request | `v1/devices/me/rpc/request/{request_id}` |
| RPC response | `v1/devices/me/rpc/response/{request_id}` |

RPC methods are `START_OTA`, `CANCEL_OTA`, `GET_INFO`, and `REBOOT`.
ThingsBoard server-side RPC uses its native `method`/`params` envelope:

```json
{"method":"GET_INFO","params":{}}
```

The current hardware milestone implements `GET_INFO` only. The parser accepts
the legacy `{"command":"GET_INFO"}` payload for compatibility with early
simulator tests, but new dashboard controls must use the native envelope.

The planned Phase 3 request for starting an update is:

```json
{"method":"START_OTA","params":{"version":"1.0.1"}}
```

## Telemetry

```json
{"ota_state":"IDLE","ota_progress":0,"app_version":"1.0.0","bootloader_version":"1.0.0","active_slot":"A","device_uptime":12345,"last_update":"2026-08-27T00:00:00Z","ota_error":0}
```

States are `IDLE`, `CHECKING`, `DOWNLOADING`, `DOWNLOADED`, `TRANSFERRING`,
`VERIFYING`, `REBOOTING`, `CONFIRMING`, `SUCCESS`, `ROLLBACK`, and `ERROR`.
Progress is an integer from 0 to 100. `active_slot` is `A` or `B`.

Error codes: `0x00 SUCCESS`, `0x01 DOWNLOAD_ERROR`, `0x02 DOWNLOAD_CRC_ERROR`,
`0x03 TM4C_TIMEOUT`, `0x04 TM4C_NACK`, `0x05 FIRMWARE_TOO_LARGE`,
`0x06 FIRMWARE_INVALID`, `0x07 VERIFY_FAILED`, `0x08 CONFIRM_TIMEOUT`, `0x09 ROLLBACK`.

## Firmware Metadata

```json
{"firmware_version":"1.0.1","firmware_size":376,"firmware_checksum":"0x1234abcd","firmware_file":"tm4c123gxl_v1.0.1.bin","release_date":"2026-08-27"}
```

The file remains a `.bin` and keeps the Phase 1 memory-size limit.

## Simulator

Offline deterministic scenarios:

```powershell
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario success
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario error
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario rollback
```

Publishing to a broker requires the optional `paho-mqtt` package and a ThingsBoard device token:

```powershell
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --host mqtt.example.com --token DEVICE_TOKEN
```
