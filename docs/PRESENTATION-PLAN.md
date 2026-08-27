# Ke hoach slide thuyet trinh OTA TM4C123GXL

Cap nhat: 2026-08-28

## 1. Muc tieu bai thuyet trinh

Giai thich ro cach he thong OTA cho TM4C123GXL duoc xay dung tu bootloader A/B
den cloud OTA qua ESP32 ESP-AT va ThingsBoard. Phan trinh bay phai tach bach:

- Phan da test/PASS tren board.
- Phan moi chi co host-test hoac simulator.
- Gioi han application chua tu nhan RPC/reset vao bootloader.
- Huong hoan thien thanh OTA tu xa khong can thao tac vat ly.

Thoi luong de xuat: 12-15 phut trinh bay, 3-5 phut demo, 5 phut hoi dap.

## 2. Doi tuong

- Giang vien/hoi dong co kien thuc embedded co ban.
- Nguoi nghe can hieu ly do chon A/B, vai tro ESP32 va ThingsBoard.
- Khong gia dinh nguoi nghe da biet ESP-AT hoac ThingsBoard RPC.

## 3. Cau truc slide

### Slide 1 - Tieu de

**OTA firmware cho TM4C123GXL qua ESP32 ESP-AT va ThingsBoard**

Noi dung:

- Ten de tai, nguoi thuc hien, board phan cung.
- Anh that TM4C123GXL + ESP32 neu co.

Thong diep: day la he thong OTA tren MCU khong co network stack rieng.

### Slide 2 - Bai toan

- TM4C123GXL khong co Wi-Fi/TCP/IP.
- Firmware can update tu xa nhung mat nguon/rut cap khong duoc brick board.
- RAM/Flash han che, khong the buffer toan bo firmware.

Visual: ba khoi `Cloud -> Network modem -> MCU Flash`.

### Slide 3 - Yeu cau an toan

- Khong ghi de active firmware.
- Header/size/CRC/vector phai hop le.
- Mat nguon va transfer interruption phai rollback.
- Firmware moi chi ACTIVE sau boot confirmation.

Visual: checklist va mot duong rollback mau do.

### Slide 4 - Kien truc tong the

```text
ThingsBoard Cloud
  -> MQTT RPC / telemetry
  -> HTTPS firmware
ESP32 ESP-AT
  -> UART1 115200
TM4C bootloader
  -> Slot A / Slot B / metadata
```

Thong diep: ESP32 la modem; TM4C giu quyen quyet dinh Flash va rollback.

### Slide 5 - Memory map

Hien thi thanh Flash 256 KiB:

- Bootloader: `0x00000000..0x00007FFF`.
- Slot A header/payload: `0x8000` / `0x8400`.
- Slot B header/payload: `0x23C00` / `0x24000`.
- Metadata copies: `0x3F800`, `0x3FC00`.

Visual: mot thanh ngang co mau rieng cho tung vung.

### Slide 6 - Phase 1: A/B bootloader

```text
START_UPDATE -> DATA -> END_UPDATE -> PENDING
             -> reset -> boot -> CONFIRM -> ACTIVE
```

- Packet CRC, sequence, ACK/NACK.
- Flash range guard va read-back.
- Ba lan khong confirm -> FAILED -> rollback.

Bang chung: UART OTA, power loss, cable removal va rollback PASS.

### Slide 7 - Phase 2: ESP-AT va ThingsBoard

- Wi-Fi/MQTT state machine.
- RPC request/response topics.
- Telemetry: state, progress, version, active slot, error.
- Dashboard va `GET_INFO` hai chieu.

Visual: sequence tu `AT` den `MQTT_RPC_READY`.

### Slide 8 - Phase 3: START_OTA

```json
{"method":"START_OTA","params":{"version":"1.0.1"}}
```

- TM4C tao URL co dinh tu config noi bo.
- RPC khong duoc thay token/slot/Flash address.
- ThingsBoard response da test: `{"accepted":true}`.

### Slide 9 - Dinh dang package

```text
0..31       header structure
32..1023    0xFF padding
1024..end   application payload
```

Giai thich tai sao raw CCS `.bin` khong upload truc tiep; application phai link
dung slot va duoc dong goi boi `package_image.py`.

### Slide 10 - Bug full-file download

Mo ta van de:

```text
ESP32 gui lien tuc -> TM4C erase/program Flash -> UART FIFO overflow -> mat byte
```

- UART khong co RTS/CTS.
- Full `HTTPCGET` khong an toan khi Flash operation block CPU.

Visual: timeline cho thay ESP32 van gui trong khoang Flash busy.

### Slide 11 - Thuat toan HTTP Range chunk

Dung vi du package 1400 byte:

```text
0-31, 32-287, 288-543, 544-799,
800-1023, 1024-1279, 1280-1399
```

- Header 32 byte rieng de erase tai cuoi response.
- Padding chunk dung tai boundary 1024.
- Payload chunk toi da 256 byte.
- Cho trailing `OK` roi moi request chunk tiep.
- Clear global Range truoc GETSIZE/retry.

Day la slide ky thuat trung tam cua bai thuyet trinh.

### Slide 12 - Verify, reboot va rollback

```text
END_UPDATE
  -> payload CRC
  -> vector validation
  -> metadata PENDING
  -> reset
  -> application confirmation
  -> ACTIVE hoac ROLLBACK
```

Khong ghi `SUCCESS` truoc confirmation.

### Slide 13 - Ket qua test

| Hang muc | Ket qua |
| --- | --- |
| CTest | 15/15 PASS |
| pytest | 18/18 PASS |
| TI ARM build | PASS |
| ICDI Flash verify | PASS |
| ThingsBoard RPC | PASS |
| Range download | PASS |
| Active Slot B 1.0.1 | PASS |
| Fault rollback Phase 1 | PASS |

Dung log rut gon `OTA_RANGE_*`, `OTA_CHUNK_DONE_*`, `OTA_REBOOTING` lam bang
chung; khong dua token/password vao slide.

### Slide 14 - Demo

Kich ban:

1. Mo dashboard ThingsBoard.
2. Mo COM7 de thay `MQTT_RPC_READY`.
3. Gui `START_OTA` cho package nham inactive slot.
4. Quan sat Range logs va `OTA_REBOOTING`.
5. Sau confirmation, cho telemetry active slot/version.

Chuan bi video backup neu Wi-Fi/cloud khong on dinh trong luc bao cao.

### Slide 15 - Gioi han hien tai

- MQTT controller hien chi song trong bootloader window 60 giay.
- Application dang chay chua nhan duoc dashboard RPC.
- `GET_INFO` can chuyen sang metadata runtime.
- TLS certificate validation chua production-ready.

Thong diep: core OTA PASS, production remote trigger con pending.

### Slide 16 - Roadmap auto-reset

```text
Application MQTT online
 -> START_OTA
 -> retained request mailbox
 -> NVIC reset
 -> bootloader consumes request
 -> Range OTA
 -> confirm / rollback
```

Them test duplicate RPC, mailbox CRC, reset giua handoff va package target
inactive slot.

### Slide 17 - Ket luan

- A/B bootloader va rollback da xac minh.
- ThingsBoard cloud OTA bang Range chunks da PASS tren hardware.
- Thiet ke giu active firmware an toan trong cac failure path.
- Buoc tiep theo la application handoff de bo reset vat ly.

## 4. Tai nguyen can chuan bi

- Anh board va so do day UART1.
- Memory-map diagram.
- Range/Flash timing diagram.
- Screenshot dashboard khong chua credential.
- COM7 log da redact.
- Video demo backup 30-60 giay.

## 5. Nguyen tac noi dung

- Moi nhan dinh PASS phai co test/log/telemetry di kem.
- Khong noi application auto-reset da xong.
- Khong hien Wi-Fi password, ThingsBoard token hoac account credential.
- Phan biet raw application binary va packaged OTA binary.
- Neu demo package moi, target slot phai khac active slot.
