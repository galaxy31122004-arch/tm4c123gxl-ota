# PHASE 2 — ThingsBoard Cloud & OTA Dashboard

## 1. Mục tiêu

Phase 2 xây dựng và xác nhận phần Cloud trước khi tích hợp ESP32 ESP-AT. Mục tiêu là chốt toàn bộ contract giữa thiết bị và ThingsBoard: device, attributes, telemetry, OTA state, command/RPC và firmware metadata.

Phase 2 chưa triển khai TLS. TLS được đưa sang Phase 3.

## 2. Kiến trúc

```text
TM4C123GXL
     |
     | telemetry / command
     v
ESP32 ESP-AT
     |
     | MQTT
     v
ThingsBoard Cloud
     |
     v
Dashboard
```

Trong Phase 2, ESP32 chưa bắt buộc phải tham gia. Có thể dùng MQTT simulator để kiểm thử Cloud contract.

## 3. Device

Tạo device mẫu:

```text
TM4C123GXL-OTA-01
```

Device cần quản lý tối thiểu:

- Device model
- Hardware version
- Bootloader version
- Application version
- Active slot
- Device connectivity status

## 4. Device Attributes

Các attribute dự kiến:

```text
device_model
aardware_version
bootloader_version
app_version
active_slot
```

Ví dụ:

```text
device_model       = TM4C123GXL
hardware_version   = 1.0
bootloader_version = 1.0.0
app_version        = 1.0.0
active_slot        = A
```

## 5. Telemetry Contract

Các telemetry key cần thống nhất:

```text
ota_state
ota_progress
app_version
bootloader_version
active_slot
device_uptime
last_update
ota_error
```

Ví dụ:

```json
{
  "ota_state": "IDLE",
  "ota_progress": 0,
  "app_version": "1.0.0",
  "bootloader_version": "1.0.0",
  "active_slot": "A",
  "device_uptime": 12345,
  "last_update": "2026-08-27T00:00:00Z",
  "ota_error": 0
}
```

## 6. OTA State Machine

Chuẩn hóa trạng thái OTA dùng chung cho ESP32 và Dashboard:

```text
IDLE
CHECKING
DOWNLOADING
DOWNLOADED
TRANSFERRING
VERIFYING
REBOOTING
CONFIRMING
SUCCESS
ROLLBACK
ERROR
```

Dashboard phải có thể hiển thị trạng thái hiện tại.

## 7. OTA Command / RPC

Các command cần thiết kế:

```text
START_OTA
CANCEL_OTA
GET_INFO
REBOOT
```

Ví dụ:

```json
{
  "command": "START_OTA",
  "version": "1.0.1"
}
```

Phase 2 chỉ xác nhận command contract; việc ESP32 thực thi command nằm ở Phase 3/4.

## 8. Firmware Metadata

Firmware OTA sử dụng `.bin`, giữ nguyên giới hạn kích thước đã xác định ở Phase 1.

Metadata tối thiểu:

```text
firmware_version
firmware_size
firmware_checksum
firmware_file
release_date
```

Ví dụ:

```text
Version : 1.0.1
Size    : <theo giới hạn Phase 1>
Checksum: <CRC/hash theo contract>
File    : tm4c123gxl_v1.0.1.bin
```

Không tạo giới hạn firmware mới ở Phase 2.

## 9. MQTT Contract

ThingsBoard Cloud là cloud platform của hệ thống.

MQTT được sử dụng cho:

- Telemetry
- Device attributes khi phù hợp
- OTA command/RPC
- OTA status

Topic/payload cụ thể phải được chốt trước Phase 3 để ESP32 implement đúng contract.

## 10. Dashboard

Dashboard tối thiểu cần hiển thị:

```text
TM4C123GXL OTA

Device Status       ONLINE
Firmware            v1.0.0
Bootloader          v1.0.0
Active Slot         A

OTA Status          IDLE
OTA Progress        0%

Last Update         ...
OTA Error           NONE
```

Khi update:

```text
OTA Status
TRANSFERRING
████████████░░░░░░░░ 60%
```

Khi thành công:

```text
OTA Status    SUCCESS
Firmware      v1.0.1
Active Slot   B
```

Khi rollback:

```text
OTA Status    ROLLBACK
Firmware      v1.0.0
Active Slot   A
```

## 11. MQTT Simulator

Trước khi có ESP32, dùng MQTT simulator để kiểm tra:

```text
MQTT Simulator
      |
      v
ThingsBoard Cloud
      |
      v
Dashboard
```

Simulator phải thử được:

1. Publish telemetry.
2. Cập nhật firmware/version information.
3. Gửi OTA command.
4. Thay đổi OTA state.
5. Cập nhật OTA progress.
6. Mô phỏng SUCCESS.
7. Mô phỏng ERROR.
8. Mô phỏng ROLLBACK.

## 12. Dashboard OTA Flow

```text
Dashboard
    |
    | START_OTA v1.0.1
    v
ThingsBoard
    |
    v
OTA command
    |
    v
ESP32 (Phase 3)
```

Phase 2 chưa yêu cầu firmware thực sự được nạp xuống TM4C.

## 13. Error Contract

Chuẩn hóa error code để Phase 3 sử dụng:

```text
0x00 SUCCESS
0x01 DOWNLOAD_ERROR
0x02 DOWNLOAD_CRC_ERROR
0x03 TM4C_TIMEOUT
0x04 TM4C_NACK
0x05 FIRMWARE_TOO_LARGE
0x06 FIRMWARE_INVALID
0x07 VERIFY_FAILED
0x08 CONFIRM_TIMEOUT
0x09 ROLLBACK
```

## 14. Những điểm giữ nguyên từ Phase 1

- TM4C bootloader quản lý A/B slot.
- TM4C quản lý firmware validation.
- TM4C quản lý confirmation.
- TM4C quản lý rollback.
- UART OTA protocol của Phase 1 được giữ nguyên.
- Firmware format `.bin` được giữ nguyên.
- Firmware size giới hạn theo Memory Map của Phase 1.

ESP32 không tự quyết định firmware slot nào được boot.

## 15. Security

Phase 2 chưa triển khai TLS.

```text
Phase 2: MQTT/HTTP flow cơ bản để xác nhận architecture
Phase 3: TLS + security hardening
```

## 16. Thứ tự triển khai

```text
1. Tạo ThingsBoard Cloud device
        |
2. Chốt device attributes
        |
3. Chốt telemetry keys
        |
4. Chốt OTA states
        |
5. Chốt OTA command/RPC
        |
6. Chốt firmware metadata
        |
7. Chốt MQTT topic/payload
        |
8. Tạo dashboard
        |
9. Tạo MQTT simulator
        |
10. Test telemetry
        |
11. Test OTA command
        |
12. Test progress/state
        |
13. Test SUCCESS
        |
14. Test ERROR
        |
15. Test ROLLBACK
```

## 17. Thông tin đã xác nhận cho các phase sau

### ESP32

```text
Model      : ESP32 DevKit V1
Firmware   : ESP-AT thuần túy
ESP-AT     : v4.2
UART       : 115200 baud
Logic      : 3.3 V
```

### Cloud

```text
Platform   : ThingsBoard Cloud
```

### Firmware

```text
Format     : .bin
Size limit : theo Phase 1
```

### Security

```text
TLS        : Phase 3
```

## 18. Tiêu chí hoàn thành Phase 2

Phase 2 PASS khi:

- ThingsBoard Cloud có device OTA.
- Attributes đã được định nghĩa.
- Telemetry contract hoạt động.
- OTA states được chuẩn hóa.
- OTA command/RPC hoạt động với simulator.
- Firmware metadata được định nghĩa.
- MQTT topic/payload contract được ghi lại.
- Dashboard hiển thị được device, firmware, slot và OTA state.
- Dashboard mô phỏng được progress 0–100%.
- Có thể mô phỏng SUCCESS.
- Có thể mô phỏng ERROR.
- Có thể mô phỏng ROLLBACK.
- Contract đủ rõ để Phase 3 triển khai ESP32 ESP-AT.

## 19. Kết quả mong đợi

Cuối Phase 2 có một Cloud contract ổn định:

```text
ThingsBoard Cloud
      |
      +-- Device
      +-- Attributes
      +-- Telemetry
      +-- OTA metadata
      +-- OTA command
      +-- OTA state
      +-- Dashboard
      +-- MQTT contract

## Verification status (2026-08-27)

| Item | Status | Evidence |
| --- | --- | --- |
| ThingsBoard device | PASS | `TM4C123GXL-OTA-01` received MQTT |
| Attributes | PASS | device, hardware, bootloader, app, slot |
| Live telemetry | PASS | Cloud Latest telemetry |
| MQTT simulator | PASS | `scripts/mqtt_simulator.py` |
| Progress 0-100% | PASS | success/error/rollback scenarios |
| SUCCESS | PASS | Slot B, firmware 1.0.1 |
| ERROR | PASS | `ota_error=2` |
| ROLLBACK | PASS | Slot A, firmware 1.0.0, `ota_error=9` |
| MQTT topic/payload | PASS | `docs/PHASE2-CONTRACT.md` |
| Cloud dashboard widgets | PASS | User confirmed widgets read device data |
| OTA command/RPC round-trip | PENDING | No ESP32/simulator subscriber yet |

Phase 2 remains incomplete until the OTA command/RPC round-trip is verified on the tenant.
```

Sau đó Phase 3 mới triển khai ESP32 DevKit V1 + ESP-AT v4.2 để kết nối thật vào contract này.
