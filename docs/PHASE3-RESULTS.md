# Phase 3 Verification Results

Date: 2026-08-28

## Automated verification

| Check | Result | Evidence |
| --- | --- | --- |
| Full host C regression | PASS | 15/15 CTest |
| Python regression | PASS | 18/18 pytest |
| `START_OTA` parsing | PASS | Valid/malformed semantic version cases |
| ESP-AT HTTP framing | PASS | Incremental prefix and binary body tests |
| Package header page | PASS | 1 KiB format and padding skip |
| HTTP Range sequencing | PASS | 32-byte header, <=256-byte chunks, trailing OK gate |
| Payload boundary | PASS | Padding stops exactly at offset 1024 |
| Cloud update adapter | PASS | START/DATA/END, CRC/vector, pending metadata |
| Oversized/truncated package | PASS | Reject before erase / deterministic error |
| TI ARM Clang build | PASS | Integrated cloud bootloader build |
| ICDI program/verify | PASS | Bootloader at 0, factory Slot A at 0x8000 |

## Hardware end-to-end evidence

Board: TM4C123GXL qua ICDI/COM7, ESP32 WROOM-32 ESP-AT qua UART1.

1. Board log `BL_READY`, `MQTT_RPC_READY`.
2. ThingsBoard two-way `START_OTA 1.0.1` tra:

   ```json
   {"accepted":true}
   ```

3. Package 1400 byte duoc tai:

   ```text
   OTA_RANGE_0_32       / OTA_CHUNK_DONE_0
   OTA_RANGE_32_256     / OTA_CHUNK_DONE_32
   OTA_RANGE_288_256    / OTA_CHUNK_DONE_288
   OTA_RANGE_544_256    / OTA_CHUNK_DONE_544
   OTA_RANGE_800_224    / OTA_CHUNK_DONE_800
   OTA_RANGE_1024_256   / OTA_CHUNK_DONE_1024
   OTA_RANGE_1280_120   / OTA_CHUNK_DONE_1280
   OTA_REBOOTING
   ```

4. Bootloader verify, mark pending va reset.
5. Candidate application boot/confirm.
6. ThingsBoard telemetry:

   ```text
   active_slot = B
   app_version = 1.0.1
   ota_error = 0
   ```

## Bugs found and fixed

1. Package da bi hieu la `32 + payload`; format that la `1024 + payload`.
2. Full-file `HTTPCGET` lam mat UART byte khi Flash erase/program.
3. First 1024-byte Range van erase sau byte 32 trong khi body con dang gui.
4. `RANGE_HEADER` state cho phep update engine timeout giua cac chunk.
5. Stale global Range lam sai ket qua `HTTPGETSIZE` lan sau.
6. Padding chunk 256 byte vuot offset 1024 va kich Flash write giua response.

Cac fix tuong ung: header-page validation, 32-byte header Range, <=256-byte
Range, all-cloud-state timeout guard, clear header truoc size/retry, va chunk
alignment dung payload boundary.

## Application reset handoff

Application-side MQTT, retained OTA request mailbox va NVIC reset da duoc
trien khai. Mailbox request 32 byte va mailbox confirmation 64 byte nam o hai
vung SRAM `NOINIT` rieng biet. Bootloader consume request mot lan va queue URL
firmware truoc khi MQTT vao ONLINE.

- Cloud OTA core path trong bootloader: **PASS**.
- Remote trigger khi bootloader online: **PASS**.
- Remote trigger khi application dang chay: **BUILD VERIFIED; HARDWARE PENDING**.
- Automatic application-to-bootloader reset handoff: **BUILD VERIFIED; HARDWARE PENDING**.

Host suite PASS 16/16 va TI CCS build sinh app/bootloader BIN hop le. Can nap
artifact moi len board va bam widget de chot hardware acceptance.

## Status

```text
Cloud RPC acceptance:              PASS
HTTPS firmware download:           PASS
Flash-safe HTTP Range chunking:     PASS
Inactive-slot write/verify:         PASS
Reboot/confirmation:                PASS
Hardware active-slot telemetry:     PASS
Application-side remote handoff:    BUILD PASS / HARDWARE PENDING
TLS certificate hardening:          PENDING
Phase 3 core E2E:                   PASS
Phase 3 production readiness:       INCOMPLETE
```
