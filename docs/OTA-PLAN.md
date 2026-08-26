# Kế hoạch OTA Firmware cho TM4C123GXL qua ESP32 ESP-AT và ThingsBoard

## 1. Mục tiêu

Xây dựng hệ thống cập nhật firmware từ xa cho TM4C123GXL. ESP32 sử dụng ESP-AT để đảm nhiệm Wi-Fi và MQTT với ThingsBoard, sau đó nhận firmware và truyền firmware qua UART cho bootloader của TM4C123GXL.

## 2. Kiến trúc tổng thể

```text
ThingsBoard
     |
     | MQTT / HTTP
     v
ESP32 ESP-AT
     |
     | UART firmware chunks
     v
TM4C123GXL Bootloader
     |
     v
TM4C123GXL Application
```

## 3. Phân chia hệ thống

### Phase 1 - TM4C123GXL Bootloader

- Xác định vùng Flash cho bootloader và application.
- Khởi tạo UART.
- GET_VERSION.
- START_UPDATE.
- DATA packet.
- END_UPDATE.
- CRC/checksum.
- Erase Flash.
- Write Flash.
- Verify firmware.
- Jump sang application.
- Xem xét A/B firmware và rollback nếu dung lượng Flash cho phép.

### Phase 2 - ESP32 ESP-AT

- Kết nối UART ESP32 <-> TM4C.
- Kết nối Wi-Fi.
- Kết nối MQTT tới ThingsBoard.
- Subscribe OTA command/metadata.
- Nhận firmware.
- Chia firmware thành chunk.
- Gửi chunk qua UART.
- Xử lý ACK/NACK, retry và timeout.
- Báo trạng thái OTA.

### Phase 3 - ThingsBoard

- Tạo device.
- Cấu hình credentials/access token.
- Upload firmware package.
- Quản lý firmware title/version.
- Quản lý firmware size/checksum.
- Cấu hình OTA.
- Theo dõi trạng thái cập nhật.

Metadata dự kiến:

```json
{
  "fw_title": "TM4C_APP",
  "fw_version": "1.0.2",
  "fw_size": 124532,
  "fw_checksum": "...",
  "fw_checksum_algorithm": "SHA256"
}
```

### Phase 4 - Integration

```text
PC -> UART -> TM4C Bootloader

ESP32 -> UART -> TM4C Bootloader

ThingsBoard -> ESP32 -> UART -> TM4C Bootloader
```

Không nên debug toàn bộ OTA cùng lúc. Hoàn thiện từng tầng trước rồi mới tích hợp.

## 4. Protocol ESP32 <-> TM4C

Packet đề xuất:

```text
+--------+------+--------+---------+-------+
| HEADER | CMD  | LENGTH | PAYLOAD | CRC   |
+--------+------+--------+---------+-------+
```

Command dự kiến:

```text
0x01 = START_UPDATE
0x02 = DATA
0x03 = END_UPDATE
0x04 = ACK
0x05 = NACK
0x06 = ABORT
0x07 = GET_VERSION
```

START_UPDATE chứa version, firmware size và checksum/hash.

DATA chứa sequence number, payload length, firmware data và CRC.

END_UPDATE yêu cầu TM4C xác minh toàn bộ firmware trước khi boot firmware mới.

## 5. Thuật toán truyền firmware

```text
START
  |
  v
ESP32 connect WiFi
  |
  v
Connect ThingsBoard
  |
  v
Get firmware metadata
  |
  v
Compare version
  |
  +--------------------+
  |                    |
NO UPDATE           NEW FW
  |                    |
  |                    v
  |              START_UPDATE
  |                    |
  |                    v
  |              TM4C READY?
  |                    |
  |                    v
  |              Download FW
  |                    |
  |                    v
  |              Split chunks
  |                    |
  |                    v
  |              Send chunk
  |                    |
  |               +----+----+
  |               |         |
  |              ACK       NACK
  |               |         |
  |               |       resend
  |               |
  |               v
  |            next chunk
  |               |
  |               v
  |           END_UPDATE
  |               |
  |               v
  |            CRC/SHA
  |               |
  |          +----+----+
  |          |         |
  |         OK        FAIL
  |          |         |
  |          v         v
  |        reboot    rollback
  |          |
  +----------+
       |
      END
```

## 6. Kiểm thử

- Mất Wi-Fi.
- Mất MQTT.
- ESP32 reset giữa OTA.
- TM4C reset giữa OTA.
- Mất điện giữa OTA.
- Packet CRC lỗi.
- Packet mất hoặc sai thứ tự.
- Firmware sai checksum.
- Firmware sai version.
- Firmware quá lớn.
- Firmware không hợp lệ.
- Rollback sau khi firmware mới không chạy được.

## 7. Thứ tự triển khai

1. Hoàn thiện TM4C bootloader và test bằng PC.
2. Hoàn thiện protocol ESP32 <-> TM4C.
3. Cấu hình ESP32 ESP-AT với Wi-Fi và MQTT.
4. Thiết lập ThingsBoard và firmware package.
5. Tích hợp OTA end-to-end.
6. Bổ sung retry, timeout, checksum và rollback.
