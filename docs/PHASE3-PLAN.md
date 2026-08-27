# Phase 3 - Cloud Firmware OTA End-to-End

Cap nhat: 2026-08-28

## Muc tieu va trang thai

Tai package firmware tu ThingsBoard qua ESP32 ESP-AT, ghi vao inactive slot cua
TM4C, verify, reboot, confirm va rollback ma khong thay memory map/protocol
Phase 1.

Trang thai: **core E2E PASS tren hardware; application-side remote trigger va
auto-reset da build PASS, hardware acceptance con pending**.

## Contract

```json
{"method":"START_OTA","params":{"version":"X.Y.Z"}}
```

- Package title: `TM4C123GXL`.
- Host/token lay tu ignored `secrets.h`, khong lay tu RPC.
- Header target slot phai khac active slot.
- Package = header page 1024 byte + application payload.

## State machine

```text
SYNC -> ECHO_OFF -> SYSLOG -> WIFI -> MQTT
     -> SUBSCRIBE -> ANNOUNCE -> ONLINE

ONLINE + START_OTA
     -> CLEAR_HTTP_HEADER
     -> HTTP_GET_SIZE
     -> RANGE_HEADER
     -> HTTP_GET_CHUNK
     -> RANGE_HEADER ...
     -> VERIFY
     -> REBOOTING
```

Moi state co timeout. Error retry toi da ba lan va restart tu offset 0. Active
slot van bootable trong moi failure path.

## Range algorithm

1. `AT+HTTPCHEAD=0` de xoa Range global cu.
2. `AT+HTTPGETSIZE=<url>` va validate total size.
3. Request `Range: bytes=0-31` de nhan header structure rieng.
4. Validate magic/schema/version/target/header CRC, roi moi START_UPDATE/erase.
5. Request padding theo chunk toi da 256 byte; khong chunk nao vuot offset 1024.
6. Request payload tu offset 1024 theo chunk toi da 256 byte.
7. Parse `+HTTPCGET:<size>,` vao exact-length binary mode; CR/LF/NUL trong body
   khong duoc parse nhu AT line.
8. Ghi/read-back chunk. Cho trailing `OK` cua response xong moi request Range
   tiep theo.
9. Het file: flush final DATA, END_UPDATE, CRC/vector, PENDING, REBOOTING/reset.

Moi Range ket thuc tai diem co the erase/program. Vi vay ESP32 khong con body
byte dang gui trong luc TM4C block de thao tac Flash.

## Boot va rollback

```text
valid image -> PENDING -> reset -> boot candidate
candidate confirms -> ACTIVE
candidate fails 3 boots -> FAILED -> previous ACTIVE slot
```

## Da hoan thanh

- ThingsBoard package/RPC/download URL.
- ESP-AT Wi-Fi, MQTT, HTTPS size/download.
- Exact-length HTTP binary parser.
- Range chunking 32/256 byte va boundary offset 1024.
- Adapter cloud stream -> Phase 1 update engine.
- Retry from zero va clear stale Range.
- Hardware Flash inactive slot, CRC, reboot, confirmation.
- ThingsBoard telemetry Slot B/version 1.0.1.
- Host tests, TI build, ICDI flash/verify.

## Application handoff da trien khai

Application duy tri MQTT tren UART1. Khi nhan `START_OTA`, application luu
`{magic, schema, version, crc}` vao retained mailbox tai `0x20007FA0`, gui RPC
response, cho UART drain 500 ms va tu NVIC reset. Bootloader consume mailbox
mot lan, queue dung version va bat dau OTA sau khi network ready.

Mailbox confirmation 64 byte van nam rieng tai `0x20007FC0`, nen handoff khong
ghi de co che confirmation/rollback. Host tests va TI CCS build da PASS; hardware
test tu widget khi application dang chay van PENDING.

### P1 - Runtime truth

- `GET_INFO` doc version/slot tu metadata runtime.
- Progress telemetry tai safe AT command boundaries.
- Dashboard hien RPC response/error ro rang.

### P2 - Production security

- CA provisioning va HTTPS certificate validation.
- MQTT TLS neu ESP-AT image/budget cho phep.
- Rotate credentials da tung duoc chia se thu cong.

## Acceptance tiep theo

1. Nap artifact moi va de board chay application qua bootloader window.
2. Nhan widget OTA ma khong cham board.
3. Xac minh UART co `RPC_START_OTA_ACCEPTED`, reset va `BL_READY`.
4. Bootloader consume dung version va download inactive-slot package.
5. New application confirm; ThingsBoard bao dung active slot/version.
6. Duplicate RPC, reset giua handoff va mailbox CRC hong khong erase active
   slot.
