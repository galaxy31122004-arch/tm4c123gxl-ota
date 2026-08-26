# Phase 1 Basic Verification Results

Date: 2026-08-26

## Automated Evidence

- Host C build: GCC 16.1, C11, strict warnings.
- CTest: 9/9 passed (`crc32`, `config`, `protocol`, `metadata`, `boot`, `image`, `flash_guard`, `update`, `confirmation`).
- Python: 8/8 pytest cases passed with Python 3.10.
- Embedded build: TI Arm Clang 5.1.1.LTS from CCS 21.0 and TivaWare 2.2.0.295.
- Bootloader vector: `0x00000000`; binary size: 6,396 bytes.
- Slot A vector: `0x00008400`.
- Slot B vector: `0x00024000`.
- Factory image: `artifacts/factory_slot_a_v1.0.0.bin`, flashed at `0x00008000`.
- OTA image: `artifacts/ota_slot_b_v1.0.1.bin`, sent through UART1.

The TI linker reports a `wchar_t` ABI warning between legacy prebuilt libraries and the current runtime. This project does not use `wchar_t`; linking and binary generation complete successfully.

## Hardware Status

CCS UniFlash successfully generated `config/tm4c123gxl.ccxml` for the Stellaris ICDI and TM4C123GH6PM. The flash attempt could not connect to the target, and Windows reported no serial COM ports. Therefore no hardware flash or UART OTA transfer is claimed as passed yet.

Run these after the LaunchPad DEBUG USB and external USB-UART are visible:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup_pc_tool.ps1
powershell -ExecutionPolicy Bypass -File scripts/flash_factory.ps1
powershell -ExecutionPolicy Bypass -File scripts/simulate_esp32.ps1 -Port COM5
```

The simulation command sends `GET_INFO`, `START_UPDATE`, sequential `DATA` packets, `END_UPDATE`, and `RESET`, matching the command flow intended for ESP32 Phase 2.
