# PHASE 3 — Cloud Firmware OTA End-to-End

## 1. Mục tiêu

Phase 3 triển khai đường OTA thực tế từ ThingsBoard Cloud xuống firmware TM4C123GXL thông qua ESP32 DevKit V1 chạy ESP-AT thuần túy.

```text
ThingsBoard Cloud
       |
       | MQTT / firmware metadata
       v
     TM4C123GXL
       |
       | UART1 115200
       v
 ESP32 DevKit V1
   ESP-AT v4.2
       |
       | Wi-Fi / HTTP
       v
ThingsBoard Cloud
       |
       | firmware .bin
       v
ESP32 -> UART -> TM4C Bootloader -> inactive slot
```

Phase 1 đã chịu trách nhiệm A/B slot, flash, CRC, confirmation và rollback. Phase 3 không thay đổi Memory Map hoặc bootloader protocol đã PASS.

TLS/security hardening được thực hiện ở phase sau; Phase 3 ban đầu dùng flow không-TLS để xác nhận end-to-end.

## 2. Vai trò các thành phần

### ThingsBoard Cloud

- Lưu firmware metadata/file.
- Gửi OTA command/RPC.
- Nhận telemetry và OTA state.
- Hiển thị dashboard.

### TM4C123GXL

- Host điều khiển ESP-AT.
- Quản lý OTA state machine.
- Kiểm tra firmware metadata.
- Điều phối transfer.
- Giao tiếp với bootloader.
- Báo progress/result lên ThingsBoard.

### ESP32 DevKit V1

- Chạy ESP-AT thuần túy.
- Kết nối Wi-Fi.
- Thực hiện network/MQTT/HTTP theo AT command do TM4C gửi.
- Chuyển firmware data về TM4C theo chunk.

ESP32 không quyết định active slot hoặc rollback. Các quyết định boot/flash thuộc TM4C bootloader.

## 3. Cấu hình đã xác nhận

```text
ESP32              : ESP32 DevKit V1
ESP32 firmware     : ESP-AT thuần túy
ESP-AT version     : v4.2
UART TM4C <-> ESP32: 115200 baud
Logic level        : 3.3 V
Cloud              : ThingsBoard Cloud
Firmware format    : .bin
Firmware limit     : theo Memory Map Phase 1
TLS                : phase sau
```

## 4. Phase 3.1 — Firmware metadata

Giữ nguyên `.bin` và giới hạn firmware của Phase 1.

Metadata tối thiểu:

```text
firmware_version
firmware_size
firmware_checksum
firmware_file
release_date
```

Trước khi flash phải kiểm tra:

```text
firmware_size <= APP_MAX_SIZE
version hợp lệ
checksum/CRC hợp lệ theo contract
```

Không tạo Memory Map hoặc APP_MAX_SIZE mới ở Phase 3.

## 5. Phase 3.2 — ThingsBoard firmware

Tạo/upload một firmware test, ví dụ:

```text
TM4C123GXL
version: 1.0.1
file: tm4c123gxl_v1.0.1.bin
```

Xác minh firmware metadata khớp file thực tế.

## 6. Phase 3.3 — OTA request

Giữ native ThingsBoard RPC envelope đã chốt ở Phase 2:

```json
{"method":"START_OTA","params":{"version":"1.0.1"}}
```

Flow:

```text
ThingsBoard
    |
    | START_OTA
    v
TM4C
    |
    +-- GET_INFO
    |
    +-- compare current/target version
    v
CHECKING
```

Không bắt đầu flash nếu target không hợp lệ.

## 7. Phase 3.4 — Firmware metadata/download information

TM4C phải xác định được:

```text
version
size
checksum
firmware download location
```

Contract cần thống nhất rõ trước implementation download thật.

## 8. Phase 3.5 — ESP32 Wi-Fi

TM4C điều khiển ESP-AT:

```text
TM4C
 |
 +-- AT+CWMODE
 +-- AT+CWJAP
 |
 +-- Wi-Fi connected
```

State tối thiểu:

```text
WIFI_CONNECTING
WIFI_CONNECTED
WIFI_ERROR
```

Phải có timeout và retry; không chờ vô hạn.

## 9. Phase 3.6 — ESP32 HTTP firmware download

Mục tiêu:

```text
ThingsBoard firmware
        |
        | HTTP
        v
ESP32 ESP-AT
        |
        | chunk
        v
TM4C
```

TM4C không tự triển khai TCP/IP. ESP32 ESP-AT đảm nhiệm network transport.

Cần kiểm tra đúng AT command/API của ESP-AT v4.2 trước khi code.

## 10. Phase 3.7 — Firmware buffering/chunking

Không yêu cầu lưu toàn bộ firmware trong RAM TM4C.

Flow ưu tiên:

```text
Cloud -> ESP32 -> chunk -> TM4C -> Bootloader
```

Kích thước chunk ban đầu đề xuất:

```text
256 bytes
```

Sau đó benchmark với:

```text
128 / 256 / 512 / 1024 bytes
```

và chọn kích thước ổn định nhất dựa trên hardware test.

## 11. Phase 3.8 — TM4C ↔ ESP32 transfer protocol

Tái sử dụng ACK/NACK và sequencing đã được xác nhận ở Phase 1.

Flow đề xuất:

```text
TM4C -> ESP32 : REQUEST_CHUNK(offset, length)
ESP32 -> TM4C : DATA(offset, length, payload)
TM4C -> ESP32 : ACK(offset)
```

Nếu lỗi:

```text
DATA
 |
 NACK
 |
 retry
```

Retry ban đầu: tối đa 3 lần cho một chunk. Giá trị có thể điều chỉnh mediante test.

## 12. Phase 3.9 — Bootloader update

TM4C yêu cầu bootloader bắt đầu update bằng protocol Phase 1:

```text
START_UPDATE
    |
    v
Erase inactive slot
    |
    v
DATA + ACK/NACK
    |
    v
END_UPDATE
```

Ví dụ hiện tại:

```text
Slot A = ACTIVE
Slot B = target
```

Không erase active slot.

## 13. Phase 3.10 — Progress telemetry

Progress:

```text
progress = written_bytes / firmware_size * 100
```

Publish ThingsBoard:

```json
{"ota_state":"TRANSFERRING","ota_progress":60}
```

State progression dự kiến:

```text
CHECKING
  -> DOWNLOADING
  -> DOWNLOADED
  -> TRANSFERRING
  -> VERIFYING
  -> REBOOTING
  -> CONFIRMING
  -> SUCCESS
```

## 14. Phase 3.11 — Image verification

Sau `END_UPDATE`, bootloader thực hiện image validation theo cơ chế Phase 1.

```text
Received image
      |
      v
size/range check
      |
      v
CRC/image validation
      |
   +--+--+
   |     |
 PASS   FAIL
   |     |
   v     v
next   ERROR
step
```

Nếu verification fail, firmware mới không được activate.

## 15. Phase 3.12 — Reboot

Khi image hợp lệ:

```text
Slot B = PENDING
       |
       v
TM4C RESET
       |
       v
Bootloader
       |
       v
Boot Slot B
```

Telemetry:

```text
REBOOTING
```

## 16. Phase 3.13 — Boot confirmation

Application mới phải confirm boot theo cơ chế Phase 1.

```text
Slot B
Firmware 1.0.1
       |
       v
BOOT_CONFIRM
       |
       v
CONFIRMED / ACTIVE
```

ThingsBoard:

```json
{"ota_state":"SUCCESS","app_version":"1.0.1","active_slot":"B"}
```

## 17. Phase 3.14 — Rollback

Nếu firmware mới không confirm:

```text
B = PENDING/FAILED
       |
       v
Rollback
       |
       v
A = ACTIVE
```

ThingsBoard:

```json
{"ota_state":"ROLLBACK","app_version":"1.0.0","active_slot":"A"}
```

Cơ chế rollback phải tái sử dụng cơ chế đã PASS ở Phase 1.

## 18. Phase 3.15 — State synchronization

OTA state phải nhất quán giữa:

```text
TM4C <-> ESP32 <-> ThingsBoard
```

Không được báo `SUCCESS` trước khi TM4C xác nhận firmware mới đã boot/confirm.

## 19. Phase 3.16 — Error handling

Các lỗi tối thiểu:

```text
DOWNLOAD_ERROR
DOWNLOAD_CRC_ERROR
TM4C_TIMEOUT
TM4C_NACK
FIRMWARE_TOO_LARGE
FIRMWARE_INVALID
VERIFY_FAILED
CONFIRM_TIMEOUT
ROLLBACK
```

Sử dụng error code đã chốt trong `PHASE2-CONTRACT.md`.

## 20. Phase 3.17 — Timeout/retry/watchdog

Phải có timeout cho:

```text
Wi-Fi connection
HTTP connection/download
UART response
Chunk ACK
Verification
Boot confirmation
```

Không cho phép bất kỳ state nào chờ vô hạn.

## 21. Phase 3.18 — Restart behavior

Phase 3 phiên bản đầu không bắt buộc resume OTA.

Nếu ESP32 reset hoặc OTA bị gián đoạn:

```text
OTA interrupted
      |
      v
old active firmware remains bootable
      |
      v
START_OTA again
```

Resume theo chunk có thể đưa vào phase nâng cấp sau.

## 22. Phase 3.19 — Logging

COM7 cần có log đủ để trace:

```text
OTA_REQUEST_RECEIVED
OTA_VERSION_CHECK
WIFI_CONNECTING
WIFI_CONNECTED
FIRMWARE_DOWNLOAD_START
FIRMWARE_DOWNLOAD_COMPLETE
TM4C_UPDATE_START
FLASH_ERASE
TRANSFER_START
TRANSFER_PROGRESS
VERIFY_START
VERIFY_OK
REBOOTING
WAIT_CONFIRM
BOOT_CONFIRM
OTA_SUCCESS
OTA_ERROR
```

Lỗi phải có `ERROR_CODE`.

## 23. Phase 3.20 — End-to-end test matrix

### Normal OTA

```text
v1.0.0 / A
   -> OTA v1.0.1
   -> B
   -> VERIFY PASS
   -> CONFIRM
   -> SUCCESS
```

### Invalid CRC

```text
v1.0.1
   -> CRC FAIL
   -> VERIFY_FAILED
   -> A remains ACTIVE
```

### Firmware too large

```text
firmware > APP_MAX_SIZE
   -> REJECT
```

### UART interruption

```text
transfer
   -> UART interruption
   -> timeout/retry/error
   -> old active firmware remains bootable
```

### No confirmation

```text
A = v1.0.0
B = v1.0.1
   -> no confirmation
   -> rollback
   -> A = ACTIVE
```

### ESP32 reset

```text
ESP32 reset during OTA
   -> OTA interrupted
   -> old active firmware remains bootable
```

## 24. Phase 3.21 — Performance measurement

Record:

```text
firmware size
firmware download time
TM4C transfer time
total OTA time
average throughput
retry count
```

Không đặt performance target cứng trước khi đo hardware thật.

## 25. Phase 3.22 — Security boundary

Phase 3 initial implementation:

```text
MQTT/HTTP basic flow
```

TLS không nằm trong initial gate.

TLS, certificate validation, credential protection và security hardening được thực hiện ở phase sau.

## 26. Tiêu chí PASS

Phase 3 chỉ PASS khi board thật chạy được tối thiểu:

```text
ThingsBoard
   |
   | START_OTA
   v
TM4C
   |
   | ESP-AT
   v
ESP32
   |
   | HTTP
   v
Firmware .bin
   |
   v
ESP32 -> UART chunks -> TM4C Bootloader
   |
   v
Inactive Slot
   |
   v
CRC/verify PASS
   |
   v
Reboot
   |
   v
Boot confirmation
   |
   v
ThingsBoard SUCCESS
```

Và phải chứng minh:

- OTA bình thường.
- Firmware size validation.
- Firmware verification.
- ACK/NACK.
- Timeout/retry.
- Progress telemetry.
- Error reporting.
- Confirmation.
- Rollback.
- Old firmware vẫn boot được khi OTA thất bại.

## 27. Các điểm phải chốt trước implementation

1. Firmware download URL/API cụ thể của ThingsBoard Cloud.
2. Cơ chế authentication cho firmware download trong Phase 3 non-TLS.
3. AT command HTTP cụ thể của ESP-AT v4.2.
4. Cách TM4C nhận response/data từ ESP-AT và phân tách AT response với firmware payload.
5. Chunk size thực tế sau benchmark.
6. Metadata/checksum mapping giữa ThingsBoard firmware metadata và CRC mà bootloader Phase 1 sử dụng.

## 28. Thứ tự triển khai

```text
3.1  Firmware metadata
3.2  ThingsBoard firmware upload
3.3  START_OTA RPC
3.4  ESP32 Wi-Fi
3.5  HTTP download thử nghiệm
3.6  Firmware chunk transfer
3.7  Bootloader Flash
3.8  CRC/verification
3.9  Reboot
3.10 Boot confirmation
3.11 ThingsBoard SUCCESS
3.12 Rollback
3.13 Error handling
3.14 Fault testing
3.15 Performance measurement
3.16 PASS
```

## 29. Không thay đổi từ Phase 1/2

- Không thay đổi Memory Map.
- Không thay đổi `APP_MAX_SIZE`.
- Không thay đổi bootloader A/B logic nếu không có bug được chứng minh.
- Không thay đổi ACK/NACK protocol Phase 1 nếu không cần thiết.
- Không commit Wi-Fi password, ThingsBoard token hoặc credential.
- Không commit firmware binary vào repo nếu không có chủ đích riêng.
- TLS/security hardening để phase sau.
