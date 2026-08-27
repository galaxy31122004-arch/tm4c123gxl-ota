# Phase 2 - ESP-AT va ThingsBoard

> Trang thai va thu tu hien tai duoc quan ly tai `docs/OTA-PLAN.md`. File nay mo
> ta pham vi va tieu chi nghiem thu cua Phase 2.

Cap nhat: 2026-08-27

## Muc tieu

Xac minh contract cloud va ket noi that giua TM4C123GXL, ESP32 chay ESP-AT
v4.1.1.0 va ThingsBoard Cloud. Phase nay khong tai hoac flash firmware tu cloud.

## Kien truc

```text
TM4C123GXL -- UART1 115200 --> ESP32 ESP-AT -- MQTT 1883 --> ThingsBoard
      |
      +-- UART0 / COM7 diagnostic
```

ESP32 la modem AT. TM4C khoi tao Wi-Fi, MQTT, subscription va xu ly RPC.

## Contract da chot

Contract chi tiet nam tai `docs/PHASE2-CONTRACT.md`.

| Muc dich | MQTT topic |
| --- | --- |
| Attributes | `v1/devices/me/attributes` |
| Telemetry | `v1/devices/me/telemetry` |
| RPC request | `v1/devices/me/rpc/request/{request_id}` |
| RPC response | `v1/devices/me/rpc/response/{request_id}` |

Telemetry toi thieu: `ota_state`, `ota_progress`, `app_version`,
`bootloader_version`, `active_slot`, `device_uptime`, `last_update`, va
`ota_error`.

OTA states:

```text
IDLE CHECKING DOWNLOADING DOWNLOADED TRANSFERRING VERIFYING
REBOOTING CONFIRMING SUCCESS ROLLBACK ERROR
```

RPC duoc dinh nghia: `START_OTA`, `CANCEL_OTA`, `GET_INFO`, `REBOOT`. Firmware
hien tai chi thuc thi `GET_INFO`; cac command OTA con lai thuoc Phase 3.

## Da hoan thanh

- ThingsBoard device va attributes: PASS.
- Dashboard widget doc duoc device data: PASS, user confirmed.
- Telemetry that tren tenant: PASS.
- MQTT simulator: PASS.
- Progress `0..100`, `SUCCESS`, `ERROR`, `ROLLBACK`: PASS.
- MQTT topic/payload contract: PASS.
- ESP-AT Wi-Fi/MQTT startup va subscription: PASS tren hardware.
- Host tests cho parser va controller: PASS.
- TI firmware build va gioi han bootloader 32 KiB: PASS.

## RPC cloud da xac minh

ThingsBoard da gui `GET_INFO` den device that va nhan response dung hai lan:

```json
{"app_version":"1.0.0","bootloader_version":"1.0.0","active_slot":"A"}
```

## Con lai

- COM7 hien `RPC_GET_INFO_RESPONSE_SENT`.

Day la gate bang chung duy nhat de ket thuc Phase 2. Cho den khi co log tren,
Phase 2 van la **IN PROGRESS**.

## Ngoai pham vi

- Firmware download tu cloud.
- UART transfer image vao bootloader Phase 1.
- Flash/boot/confirmation end-to-end tu ThingsBoard.
- TLS va security hardening.

Nhung muc nay thuoc Phase 3.
