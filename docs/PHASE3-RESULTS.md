# Phase 3 Verification Results

Date: 2026-08-27

## Automated Verification

| Check | Result | Evidence |
|---|---|---|
| Phase 1 and Phase 2 host regression | PASS | 15/15 CTest tests |
| Native `START_OTA` parsing | PASS | Valid and malformed semantic-version cases |
| ESP-AT HTTP framing | PASS | Incremental header and binary NUL/CR/LF body cases |
| Cloud stream to Phase 1 update engine | PASS | Header, 256-byte chunking, CRC/vector verification, pending metadata |
| Oversized package before erase | PASS | Host fake Flash erase count remains zero |
| Truncated stream | PASS | Deterministic `CLOUD_OTA_TRUNCATED` result |
| TI ARM Clang integrated build | PASS | Cloud-linked bootloader build with dummy local credentials |
| Vector address | PASS | `.intvecs` at `0x00000000` in `bootloader.map` |
| Bootloader size | PASS | 21,336 bytes of 32,768-byte budget |
| Static SRAM layout | PASS | `.bss` ends at `0x20005B90`; 2 KB stack, top `0x20007FC0` |

The build-only credentials were dummy values in ignored `secrets.h`; that file
was removed after measurement. No live credential is stored in the repository.

## Hardware Acceptance

The following checks remain pending and must not be reported as PASS until their
COM7 and ThingsBoard observations are captured:

- Flash the integrated `bootloader.bin` built with the device's local secrets.
- Observe Wi-Fi/MQTT connection and `MQTT_RPC_READY`.
- Upload a packaged Phase 1 image to ThingsBoard and invoke native `START_OTA`.
- Observe size query, binary download, progress, inactive-slot write, reboot, and
  application boot confirmation.
- Verify the normal success path and one physical interruption/rollback path.

Phase 3 status: **IMPLEMENTED, HARDWARE VERIFICATION PENDING**.

ESP-AT cannot accept MQTT publish commands while `HTTPCGET` owns the UART binary
response. Phase 3 therefore reports cloud progress at the safe command boundaries
(`DOWNLOADING` 0 and `REBOOTING` 100), while exact byte progress remains available
internally for error telemetry after reconnect.
