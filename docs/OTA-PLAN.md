# Ke hoach OTA TM4C123GXL

Day la roadmap va bang trang thai chinh cua du an. Cac file `PHASE*-RESULTS.md`
chi luu bang chung chi tiet; neu co khac biet, trang thai trong file nay duoc uu
tien.

Cap nhat: 2026-08-27

## Trang thai tong quan

| Phase | Trang thai | Da hoan thanh | Con lai |
| --- | --- | --- | --- |
| Phase 1 - TM4C bootloader | PASS | Build, memory map, protocol, flash that, UART OTA A/B, confirmation, rollback va fault test vat ly | Khong con gate Phase 1 |
| Phase 2 - ESP-AT va ThingsBoard contract | IN PROGRESS | Device, attributes, telemetry, dashboard, simulator, SUCCESS/ERROR/ROLLBACK, ESP-AT MQTT session | RPC `GET_INFO` that tu ThingsBoard xuong TM4C va response cung request ID |
| Phase 3 - Cloud firmware OTA end-to-end | NOT STARTED | Chua co | Tai `.bin` tu cloud, kiem tra image, truyen UART vao bootloader, reboot/confirm/rollback, TLS |

## Kien truc dich

```text
ThingsBoard Cloud
        |
        | MQTT / HTTP
        v
ESP32 ESP-AT
        |
        | UART
        v
TM4C123GH6PM
        |
        +-- Bootloader 0x00000000..0x00007FFF
        +-- Application Slot A
        +-- Application Slot B
        +-- Metadata
```

ESP32 ESP-AT la modem do TM4C dieu khien. Firmware ESP-AT/RPC hien tai cung bi
gioi han trong vung bootloader 32 KiB va khong duoc de len Slot A.

## Phase 1 - TM4C bootloader

Trang thai: **PASS**.

Da xac minh:

- TI ARM Clang/TivaWare build thanh cong, vector table tai `0x00000000`.
- Memory map bao ve bootloader, Slot A, Slot B va metadata.
- UART protocol: `GET_INFO`, `START_UPDATE`, `DATA`, `END_UPDATE`, `ABORT`,
  `ACK`, `NACK`, `RESET`.
- CRC packet, CRC image, sequence, timeout, retry va flash range guard.
- Cap nhat A sang B, boot confirmation va chuyen active slot.
- Image khong confirmation bi danh dau `FAILED` va rollback ve Slot A.
- Dong serial bi ngat, reset giua transfer, rut cap vat ly va mat nguon vat ly:
  board van rollback/boot lai firmware active truoc do.

Bang chung: [PHASE1-RESULTS.md](PHASE1-RESULTS.md).

## Phase 2 - ESP-AT va ThingsBoard

Trang thai: **IN PROGRESS**.

Da xac minh:

- ThingsBoard device `TM4C123GXL-OTA-01` va dashboard da duoc tao.
- Attributes va telemetry hien thi dung tren dashboard.
- MQTT topic/payload da chot trong [PHASE2-CONTRACT.md](PHASE2-CONTRACT.md).
- Simulator da publish cac luong progress `0..100`, `SUCCESS`, `ERROR` va
  `ROLLBACK` len tenant that.
- TM4C dieu khien ESP32 ESP-AT qua UART1, ket noi Wi-Fi/MQTT va vao trang thai
  `MQTT_RPC_READY`; telemetry `IDLE` duoc publish khi session san sang.
- Parser/controller ESP-AT co host tests va firmware nam trong gioi han 32 KiB.

Gate duy nhat con lai:

1. Gui server-side RPC `{"method":"GET_INFO","params":{}}` tu ThingsBoard.
2. TM4C nhan request qua ESP-AT va ghi `RPC_GET_INFO_RESPONSE_SENT` tren COM7.
3. ThingsBoard nhan response tren `v1/devices/me/rpc/response/{request_id}` voi
   cung request ID.
4. Response phai co `app_version`, `bootloader_version`, va `active_slot`.

Khong danh dau Phase 2 PASS truoc khi thay du ca COM7 va response that tren
ThingsBoard. Bang chung: [PHASE2-RESULTS.md](PHASE2-RESULTS.md).

## Phase 3 - Cloud firmware OTA end-to-end

Trang thai: **NOT STARTED**.

Thu tu thuc hien sau khi Phase 2 PASS:

1. Chot firmware metadata/download URL va co che authentication.
2. Tai firmware `.bin` theo tung chunk ma khong vuot RAM/Flash budget.
3. Kiem tra size va checksum truoc khi bat dau update.
4. Truyen image qua UART bang protocol Phase 1, co ACK/NACK/timeout/retry.
5. Theo doi telemetry `DOWNLOADING -> TRANSFERRING -> VERIFYING -> REBOOTING`.
6. Xac minh `SUCCESS`, confirmation va active slot moi.
7. Xac minh `ERROR` va `ROLLBACK` tren board va dashboard.
8. Bo sung TLS va quan ly credential phu hop san pham.

## Nguyen tac nghiem thu

- Chi ghi `PASS` khi co bang chung tu dung lop dang duoc nghiem thu.
- Simulator khong thay the hardware RPC hoac OTA end-to-end.
- Khong commit Wi-Fi password, ThingsBoard token, binary build, NVS backup hay
  serial capture.
- Bootloader va firmware adapter khong duoc vuot qua `0x00007FFF`.
- Luon giu mot application slot co kha nang boot va rollback.
