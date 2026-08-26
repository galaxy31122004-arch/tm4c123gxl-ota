# Kế hoạch OTA Firmware cho TM4C123GXL qua ESP32 ESP-AT và ThingsBoard

## 1. Mục tiêu

Xây dựng hệ thống cập nhật firmware từ xa cho TM4C123GH6PM. ESP32 sử dụng ESP-AT để đảm nhiệm Wi-Fi và MQTT với ThingsBoard, sau đó nhận firmware và truyền firmware qua UART cho bootloader của TM4C.

Trong Phase 1, ESP32 và ThingsBoard chưa được sử dụng để kiểm thử OTA. PC + USB-UART sẽ mô phỏng ESP32.

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
TM4C123GH6PM Bootloader
     |
     +---- Application Slot A
     |
     +---- Application Slot B
```

## 3. Phase 1 - TM4C123GH6PM Bootloader

Mục tiêu của Phase 1 là xây dựng bootloader tự chủ trên TM4C với khả năng nhận, ghi, kiểm tra, boot, xác nhận và rollback firmware.

### 3.1 Hardware configuration

- MCU mục tiêu: TM4C123GH6PM.
- Flash: 256 KB.
- SRAM: 32 KB.
- Cấu hình bộ nhớ được tập trung trong một file configuration.

### 3.2 Memory Map

Flash được chia thành:

```text
0x00000000
+----------------------------+
|        BOOTLOADER          |
+----------------------------+
|      APPLICATION A         |
|      Firmware hiện tại     |
+----------------------------+
|      APPLICATION B         |
|      Firmware OTA/backup   |
+----------------------------+
|         METADATA           |
+----------------------------+
0x0003FFFF
```

Các giá trị sẽ gồm:

```c
BOOTLOADER_START
BOOTLOADER_SIZE
APP_SLOT_A_START
APP_SLOT_A_SIZE
APP_SLOT_B_START
APP_SLOT_B_SIZE
METADATA_START
METADATA_SIZE
```

Địa chỉ và kích thước cụ thể chưa chốt cho đến khi kiểm tra kích thước firmware thực tế, kích thước bootloader và Flash erase geometry.

### 3.3 Configuration tập trung

Tạo `bootloader_config.h` chứa các thông số cố định của TM4C123GH6PM:

```c
#define TM4C_FLASH_START     0x00000000UL
#define TM4C_FLASH_SIZE      (256UL * 1024UL)
#define TM4C_SRAM_START      0x20000000UL
#define TM4C_SRAM_SIZE       (32UL * 1024UL)

#define BOOTLOADER_START     ...
#define BOOTLOADER_SIZE      ...
#define APP_SLOT_A_START     ...
#define APP_SLOT_A_SIZE      ...
#define APP_SLOT_B_START     ...
#define APP_SLOT_B_SIZE      ...
#define METADATA_START       ...
#define METADATA_SIZE        ...
```

Các module khác không hard-code địa chỉ Flash riêng.

### 3.4 Linker Script

Bootloader và Application phải được build thành các image riêng biệt.

- Bootloader bắt đầu tại `BOOTLOADER_START`.
- Application A bắt đầu tại `APP_SLOT_A_START`.
- Application B bắt đầu tại `APP_SLOT_B_START`.

Mục tiêu là application không bao giờ ghi đè bootloader.

### 3.5 UART Driver

UART là kênh giao tiếp giữa TM4C và ESP32.

Trong Phase 1:

```text
PC -> USB-UART -> TM4C Bootloader
```

Các chức năng cơ bản:

```text
UART_Init()
UART_Send()
UART_Receive()
UART_ReadByte()
UART_WriteByte()
UART_Timeout()
```

### 3.6 UART Protocol

Protocol được thiết kế từ Phase 1 để sau này ESP32 sử dụng lại.

```text
+------+-----+-----+--------+---------+-------+
| SOF  | CMD | SEQ | LENGTH | PAYLOAD | CRC   |
+------+-----+-----+--------+---------+-------+
```

### 3.7 Command

```text
0x01 GET_INFO
0x02 START_UPDATE
0x03 DATA
0x04 END_UPDATE
0x05 ABORT
0x06 ACK
0x07 NACK
0x08 RESET
```

`GET_INFO` trả về tối thiểu:

- Bootloader version.
- Application version.
- Active slot.
- Application status.
- Maximum firmware size.

### 3.8 Version Management

Version firmware được quản lý ở phía TM4C bootloader.

ESP32/PC gửi `GET_INFO`, TM4C trả về ví dụ:

```text
BOOTLOADER_VERSION = 1.0.0
APP_VERSION        = 1.2.3
ACTIVE_SLOT        = A
```

Sau này ESP32 gửi thông tin này lên ThingsBoard để cloud biết firmware hiện tại.

### 3.9 START_UPDATE

ESP32/PC gửi:

- Firmware version.
- Firmware size.
- Firmware CRC/hash.
- Target slot.

TM4C kiểm tra kích thước, version và slot. Nếu hợp lệ, TM4C ACK rồi erase target slot.

### 3.10 Flash Driver

Các chức năng:

```text
Flash_Init()
Flash_Erase()
Flash_Write()
Flash_Read()
Flash_Verify()
```

Chỉ application slots và metadata được phép ghi/erase. Bootloader phải được bảo vệ.

### 3.11 Firmware Transfer

Firmware được chia thành các chunk. DATA packet chứa sequence number, length, payload và CRC.

```text
ESP32 -> DATA seq=0 -> TM4C
TM4C  -> verify/write -> ACK seq=0
```

Tiếp tục cho đến chunk cuối.

### 3.12 ACK / NACK / Retry

Mỗi DATA packet phải có ACK.

Nếu CRC hoặc packet không hợp lệ:

```text
DATA seq=10
    |
    v
CRC ERROR
    |
    v
NACK seq=10
    |
    v
resend
```

Có timeout và giới hạn retry, ví dụ `MAX_RETRY = 3`.

### 3.13 CRC và Firmware Verification

Có hai mức kiểm tra:

1. CRC từng packet để phát hiện lỗi truyền.
2. CRC/hash toàn bộ firmware để xác minh image sau khi ghi Flash.

Firmware không vượt qua verification sẽ không được boot.

### 3.14 Firmware Header

Firmware image có metadata:

```text
+--------------------------+
| Magic                    |
+--------------------------+
| Version                  |
+--------------------------+
| Firmware Size            |
+--------------------------+
| Firmware CRC             |
+--------------------------+
| Firmware Data            |
+--------------------------+
```

Ví dụ:

```c
typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t image_size;
    uint32_t image_crc;
} firmware_header_t;
```

### 3.15 Application Validation

Bootloader phải kiểm tra:

```text
Application tồn tại?
        |
Header hợp lệ?
        |
Size hợp lệ?
        |
Vector table hợp lệ?
        |
CRC hợp lệ?
        |
       YES
        |
        v
Boot Application
```

### 3.16 A/B Firmware

Hai slot firmware được sử dụng để không ghi đè firmware đang chạy.

Ví dụ:

```text
A = v1.0.0 ACTIVE
B = EMPTY
```

Khi OTA:

```text
A = v1.0.0 ACTIVE
B = v1.0.1 PENDING
```

Firmware mới được verify trước khi trở thành firmware active.

### 3.17 Firmware Confirmation

CRC đúng chỉ chứng minh image không bị lỗi dữ liệu; không chứng minh application thực sự hoạt động.

Sau khi boot firmware mới:

```text
Bootloader -> Application B
Application B -> BOOT_CONFIRM
Bootloader -> B = ACTIVE
```

Nếu firmware mới không gửi confirmation trong thời gian cho phép, bootloader coi firmware mới là lỗi.

### 3.18 Rollback

Nếu firmware mới không hoạt động:

```text
B = FAILED
A = ACTIVE
```

Bootloader tự động quay về firmware cũ để tránh thiết bị không thể boot.

Mục tiêu: luôn giữ ít nhất một firmware có khả năng boot.

### 3.19 Boot Decision

Sau mỗi reset:

```text
RESET
  |
  v
BOOTLOADER
  |
  v
Check metadata
  |
  v
Check Slot A / Slot B
  |
  v
Determine ACTIVE / PENDING / FAILED
  |
  v
Boot firmware phù hợp
```

Các trạng thái dự kiến:

```text
EMPTY
VALID
ACTIVE
PENDING
CONFIRMED
FAILED
```

### 3.20 Jump Application

Khi application hợp lệ được chọn:

```text
Disable interrupts
      |
Stop/reset peripheral cần thiết
      |
Set VTOR
      |
Load MSP
      |
Jump Reset_Handler
```

Application phải có vector table phù hợp với địa chỉ slot của nó.

### 3.21 PC OTA Tool

PC sẽ mô phỏng ESP32:

```text
PC -> USB-UART -> TM4C Bootloader
```

Các chức năng:

```text
info
update firmware.bin
reset
```

Tool phải hỗ trợ protocol, ACK/NACK, retry và truyền firmware theo chunk.

### 3.22 Fault Testing

Kiểm thử:

- CRC packet lỗi.
- CRC firmware lỗi.
- Packet mất.
- Packet sai sequence.
- Packet duplicate.
- Timeout.
- Retry.
- Ngắt kết nối.
- TM4C reset giữa OTA.
- Mất nguồn giữa OTA.
- Firmware quá lớn.
- Firmware không hợp lệ.
- Firmware mới không confirmation.
- Rollback.

### 3.23 Kiểm tra kích thước firmware

Trước khi chốt Memory Map A/B phải build firmware release và lấy kích thước `.bin` thực tế.

Sau đó tính:

```text
256 KB Flash
    |
    +-- Bootloader
    +-- Slot A
    +-- Slot B
    +-- Metadata
```

Đây là bước bắt buộc trước khi chốt `APP_SLOT_A_SIZE` và `APP_SLOT_B_SIZE`.

### 3.24 Cấu trúc source code

```text
tm4c123gxl-ota/
|
+-- README.md
|
+-- docs/
|   +-- OTA-PLAN.md
|
+-- tm4c123gxl/
    |
    +-- bootloader/
    |   +-- inc/
    |   +-- src/
    |   +-- linker/
    |   +-- README.md
    |
    +-- application/
```

Bootloader modules dự kiến:

```text
bootloader.c
protocol.c
uart.c
flash.c
firmware.c
crc.c
metadata.c
```

## 4. Thứ tự triển khai Phase 1

```text
1. Xác định firmware size
        |
2. Chốt Memory Map
        |
3. Tạo bootloader configuration
        |
4. Tạo linker script
        |
5. UART driver
        |
6. Flash driver
        |
7. Application linker
        |
8. Jump Application
        |
9. Firmware Header
        |
10. CRC
        |
11. UART Protocol
        |
12. GET_INFO
        |
13. START_UPDATE
        |
14. DATA + ACK/NACK
        |
15. END_UPDATE
        |
16. Firmware Validation
        |
17. A/B management
        |
18. Firmware Confirmation
        |
19. Rollback
        |
20. PC OTA Tool
        |
21. Fault Testing
```

## 5. Tiêu chí hoàn thành Phase 1

Phase 1 hoàn thành khi có thể thực hiện:

```text
Firmware v1.0.0
       |
       v
PC gửi v1.0.1
       |
       v
TM4C ghi Slot B
       |
       v
Verify
       |
       v
Boot B
       |
       v
Application Confirmation
       |
       v
B ACTIVE
A BACKUP
```

Nếu firmware mới lỗi:

```text
Firmware v1.0.1
       |
       v
Boot lỗi / không confirmation
       |
       v
Bootloader
       |
       v
Rollback
       |
       v
Firmware v1.0.0
       |
       v
BOOT SUCCESS
```

Phase 1 kết thúc khi TM4C123GH6PM tự chủ hoàn toàn trong việc nhận, ghi, kiểm tra, boot, xác nhận và rollback firmware. ESP32 chỉ được đưa vào ở Phase 2.