# Ke hoach OTA TM4C123GXL

Tai lieu nay la roadmap va nguon trang thai chinh cua du an tu Phase 1 den
Phase 3. Cac file `PHASE*-RESULTS.md` luu bang chung chi tiet. Neu co khac
nhau, trang thai trong file nay duoc uu tien.

Cap nhat: 2026-08-28

## 1. Trang thai tong quan

| Phase | Trang thai | Da xac minh | Con lai |
| --- | --- | --- | --- |
| Phase 1 - A/B bootloader | PASS | Build, memory map, UART OTA, CRC, confirmation, rollback, interruption va fault test vat ly | Khong con gate Phase 1 |
| Phase 2 - ESP-AT/ThingsBoard | PASS | Wi-Fi, MQTT, telemetry, dashboard, RPC `GET_INFO`, topic/payload contract | Khong con gate Phase 2 |
| Phase 3 - Cloud firmware OTA | CORE E2E PASS | ThingsBoard `START_OTA`, HTTPS Range chunks, Flash inactive slot, CRC, reboot/confirmation; application handoff build PASS | Hardware acceptance cho app auto-reset, TLS certificate hardening, telemetry chi tiet |

Phase 3 da PASS luong OTA cloud trong bootloader tren board that. Toan bo san
pham chua production-ready vi application hien tai khong duy tri MQTT sau khi
bootloader het cua so dich vu 60 giay.

## 2. Kien truc hien tai

```text
ThingsBoard Cloud
   |  MQTT RPC / telemetry
   |  HTTPS firmware download
   v
ESP32 WROOM-32, ESP-AT v4.1.1.0
   |  UART1, 115200, 8-N-1
   v
TM4C123GXL bootloader
   |-- validate package header
   |-- erase/program inactive slot
   |-- CRC + vector validation
   |-- pending/confirm/rollback metadata
   v
Application Slot A hoac Slot B
```

ESP32 chi la modem ESP-AT. TM4C tao AT command, parse MQTT/HTTP response va
quyet dinh slot/Flash/rollback. ESP32 khong tu chon slot va khong ghi Flash
TM4C.

## 3. Memory map bat bien

| Vung | Dia chi | Muc dich |
| --- | --- | --- |
| Bootloader | `0x00000000..0x00007FFF` | Boot, cloud controller, update engine |
| Slot A header | `0x00008000..0x000083FF` | Header page 1 KiB |
| Slot A payload | `0x00008400..0x00023BFF` | Application A |
| Slot B header | `0x00023C00..0x00023FFF` | Header page 1 KiB |
| Slot B payload | `0x00024000..0x0003F7FF` | Application B |
| Metadata copy 0 | `0x0003F800..0x0003FBFF` | Transactional metadata |
| Metadata copy 1 | `0x0003FC00..0x0003FFFF` | Transactional metadata backup |

Moi application phai link dung payload address cua slot dich. Package upload
ThingsBoard gom header page 1 KiB va payload da link dung dia chi.

## 4. Dinh dang firmware

```text
offset 0:    ota_firmware_header_t (32 bytes)
             padding 0xFF den het header page 1024 bytes
offset 1024: application payload
```

Header chua magic, schema, target slot, version, payload size, payload CRC32 va
header CRC32. Khong upload truc tiep `.out` hoac raw application `.bin` len
ThingsBoard; phai qua `scripts/package_image.py`.

## 5. Phase 1 - Bootloader A/B

Trang thai: **PASS**.

### Protocol

```text
GET_INFO
START_UPDATE(header)
DATA(sequence, payload <= 256 bytes)
END_UPDATE
ABORT
RESET
```

Moi packet co SOF, version, command, sequence, length va CRC32. Bootloader chi
ACK packet dung command/state/sequence/range. NACK co error code xac dinh.

### Thuat toan update

```text
START_UPDATE
  -> validate header/version/target
  -> reject neu target == active slot
  -> invalidate/erase target header page
DATA
  -> validate sequence va length
  -> erase payload page khi can
  -> program Flash
  -> read-back compare
END_UPDATE
  -> read payload va tinh CRC32
  -> validate vector MSP/ResetISR
  -> program header
  -> metadata target = PENDING
  -> reset
```

Application moi ghi boot confirmation vao retained SRAM va reset. Bootloader
consume confirmation, chuyen `PENDING -> ACTIVE`. Sau ba lan boot khong confirm,
slot moi thanh `FAILED` va bootloader quay lai active slot cu.

Bang chung: CTest/pytest/TI build, UART update A -> B, confirmation, rollback,
dong serial, reset giua transfer, rut cap va mat nguon deu PASS. Chi tiet:
[PHASE1-RESULTS.md](PHASE1-RESULTS.md).

## 6. Phase 2 - ESP-AT va ThingsBoard

Trang thai: **PASS**.

| Muc dich | MQTT topic |
| --- | --- |
| Attributes | `v1/devices/me/attributes` |
| Telemetry | `v1/devices/me/telemetry` |
| RPC request | `v1/devices/me/rpc/request/{request_id}` |
| RPC response | `v1/devices/me/rpc/response/{request_id}` |

TM4C khoi tao ESP-AT theo state machine co timeout:

```text
AT -> ATE0 -> SYSLOG -> Wi-Fi -> MQTT config/connect
   -> subscribe RPC -> publish telemetry -> MQTT_RPC_READY
```

Device, attributes, dashboard, telemetry va RPC `GET_INFO` da xac minh tren
ThingsBoard tenant that. Chi tiet: [PHASE2-CONTRACT.md](PHASE2-CONTRACT.md).

## 7. Phase 3 - Cloud firmware OTA

Trang thai: **CORE E2E PASS, APPLICATION HANDOFF BUILD PASS / HARDWARE PENDING**.

### ThingsBoard contract

```json
{"method":"START_OTA","params":{"version":"1.0.1"}}
```

TM4C tao download URL noi bo; RPC khong duoc thay host/token/slot/address:

```text
https://thingsboard.cloud/api/v1/{device-token}/firmware
  ?title=TM4C123GXL&version={version}
```

### Thuat toan HTTP Range chunk

ESP-AT `AT+HTTPCHEAD` luu header global, nen moi OTA bat dau bang:

```text
AT+HTTPCHEAD=0
AT+HTTPGETSIZE=<url>
```

Khong tai toan file trong mot `HTTPCGET`. UART khong co RTS/CTS; neu TM4C dang
erase/program Flash ma ESP32 van gui, byte se bi mat. Luong da xac minh:

```text
1. Range 0-31
   -> nhan rieng 32-byte package header
   -> validate header
   -> START_UPDATE/erase inactive header page

2. Range padding, moi chunk <= 256 bytes
   -> bo qua padding
   -> chunk cuoi dung chinh xac tai offset 1023

3. Range payload tu offset 1024
   -> moi response <= 256 bytes
   -> ghi Flash tai cuoi response
   -> cho trailing OK roi moi request chunk tiep

4. Het file
   -> flush DATA -> END_UPDATE -> CRC/vector/metadata
   -> publish REBOOTING -> reset
```

Vi du package 1400 byte da test:

```text
0..31       32 bytes, header
32..287     256 bytes, padding
288..543    256 bytes, padding
544..799    256 bytes, padding
800..1023   224 bytes, ket thuc header page
1024..1279  256 bytes, payload
1280..1399  120 bytes, payload cuoi
```

Retry toi da ba lan va restart tu byte 0. Truoc retry phai clear global Range.
Partial image khong co valid metadata nen khong duoc boot. Active slot khong bi
erase.

### Bang chung hardware

- `MQTT_RPC_READY`: PASS.
- ThingsBoard `START_OTA`: `{"accepted":true}`.
- Tat ca Range cua package 1400 byte: PASS.
- `OTA_REBOOTING`, CRC, reboot va confirmation: PASS.
- Telemetry: `active_slot=B`, `app_version=1.0.1`, `ota_error=0`.
- CTest 15/15, pytest 18/18, TI build va ICDI verify: PASS.

Chi tiet: [PHASE3-RESULTS.md](PHASE3-RESULTS.md).

## 8. Luong van hanh hien tai

```text
Reset TM4C
  -> bootloader mo cloud service window 60 giay
  -> MQTT_RPC_READY
  -> ThingsBoard START_OTA
  -> Range download + inactive-slot Flash
  -> verify + PENDING + reset
  -> boot new application
  -> application confirm + reset
  -> metadata ACTIVE
```

Cloud OTA bootloader flow da test va PASS. Application-side MQTT/reset handoff
da implement va build PASS, nhung chua duoc chot PASS tren board tu widget.

## 9. Cong viec tiep theo dung thu tu

1. Nap application/bootloader moi va hardware test widget khi app dang chay.
2. Cho phep bootloader poll UART0/COM7 va ESP32 UART1 trong cung cloud window.
3. Test duplicate RPC, mailbox CRC hong va reset giua handoff.
4. Sua `GET_INFO` de doc metadata runtime thay vi response tinh.
5. Publish progress chi tiet tai safe AT command boundaries.
6. Provision CA va bat TLS certificate validation truoc production.

Luong dich:

```text
Application running + MQTT online
  -> START_OTA
  -> save retained OTA request
  -> automatic reset
  -> bootloader consumes request
  -> cloud Range OTA
  -> verify/reboot/confirm
  -> SUCCESS or rollback
```

## 10. Nguyen tac nghiem thu

- Chi ghi PASS khi co bang chung tu dung layer.
- Simulator khong thay hardware E2E.
- Khong commit credential, binary build hoac serial dump co secret.
- Bootloader khong vuot `0x00007FFF`.
- Application phai link dung slot payload address.
- Luon giu mot active slot boot/rollback duoc.
- Khong ghi `SUCCESS` truoc application confirmation.
