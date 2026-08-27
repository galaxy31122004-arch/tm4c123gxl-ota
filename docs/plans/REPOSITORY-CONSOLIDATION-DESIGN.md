# Repository Consolidation Design

## Goal

Make `docs/OTA-PLAN.md` the single canonical roadmap and integrate the tested
TM4C-to-ESP-AT-to-ThingsBoard firmware into `main` without committing local
credentials, build output, or hardware backup files.

## Canonical Documentation

- `docs/OTA-PLAN.md` owns phase scope, ordering, gates, and current status.
- `docs/PHASE1-RESULTS.md` and `docs/PHASE2-RESULTS.md` contain verification
  evidence only.
- `docs/PHASE2-CONTRACT.md` remains the MQTT topic and payload contract.
- Historical implementation plans remain reference documents and must point
  readers to `docs/OTA-PLAN.md` for current status.

## Phase Status

- Phase 1 is PASS, including physical power interruption and physical UART/USB
  disconnection with successful rollback to the prior active image.
- Phase 2 telemetry is PASS on TM4C + ESP32 ESP-AT + ThingsBoard.
- Phase 2 remains open only for a real ThingsBoard `GET_INFO` RPC request and
  response round trip.
- Phase 3 starts after that gate and implements cloud firmware acquisition,
  UART transfer to the Phase 1 bootloader, progress reporting, and complete
  success/error/rollback flows.

## Repository Integration

Fast-forward `task/esp-at-rpc` into `main`, retain the tested 32 KiB bootloader
link limit, and extend `.gitignore` for local probes, build trees, generated
package metadata, credentials, and ESP32 backup artifacts. Backup files remain
on disk; they are not deleted.

## Verification

Before pushing, run all C host tests, Python tests, the TI ARM Clang firmware
build, linker memory-budget checks, and `git diff --check`. The hardware result
must retain the observed `MQTT_RPC_READY` evidence. The RPC gate is marked PASS
only after ThingsBoard receives the matching response for a real request ID.
