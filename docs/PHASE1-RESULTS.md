# Phase 1 Verification Results

Date: 2026-08-27

## Automated Evidence

- Host C build: GCC 16.1, C11, strict warnings.
- CTest: 10/10 passed (`crc32`, `config`, `protocol`, `metadata`, `boot`, `image`, `flash_guard`, `update`, `confirmation`, `jump`).
- Python: 8/8 pytest cases passed with Python 3.10.
- Embedded build: TI Arm Clang 5.1.1.LTS from CCS 21.0 and TivaWare 2.2.0.295.
- Bootloader vector: `0x00000000`.
- Slot A vector: `0x00008400`.
- Slot B vector: `0x00024000`.
- Factory image: `artifacts/factory_slot_a_v1.0.0.bin`, flashed at `0x00008000`.
- OTA image: `artifacts/ota_slot_b_v1.0.1.bin`.

The TI linker reports a `wchar_t` ABI warning between legacy prebuilt libraries and the current runtime. This project does not use `wchar_t`; linking and binary generation complete successfully.

## Hardware Evidence

The following tests were completed on a TM4C123GXL LaunchPad connected through
its ICDI debugger and UART COM7:

- CCS UniFlash mass-erased, programmed, and verified the bootloader at
  `0x00000000` and the factory Slot A image at `0x00008000`.
- The PC OTA client transferred `ota_slot_b_v1.0.1.bin` over COM7. The
  bootloader programmed Slot B, verified the image, marked it pending, and
  booted it after reset.
- Slot B `v1.0.1` issued boot confirmation. Metadata then reported Slot B as
  `ACTIVE` and no pending update.
- A 74-byte, deliberately non-confirming Slot B `v1.0.2` image was transferred
  over COM7. This also verifies padded final flash writes for a non-word-aligned
  payload.
- After three unconfirmed boot attempts, the bootloader marked Slot B as
  `FAILED` and rolled back to the factory Slot A image.

Final board status after the rollback test:

```text
Slot A: v1.0.0, ACTIVE
Slot B: v1.0.2, FAILED
Active slot: A
Pending slot: none
```

## Phase 1 Status

```text
Software verification: PASS
Build verification:    PASS
Memory layout:          PASS
Protocol simulation:    PASS
Hardware flashing:      PASS
UART OTA:               PASS
Phase 1 core flow:      PASS
```

The core Phase 1 acceptance paths are verified on hardware: successful
A-to-B update with confirmation, and unconfirmed-image rollback to A. The
extended fault-injection list in `OTA-PLAN.md` has test coverage in the host
suite where applicable, but every physical fault scenario (for example power
loss during a flash write and cable removal mid-transfer) has not been run.

## Reproduce

Build, flash, and exercise the board with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup_pc_tool.ps1
powershell -ExecutionPolicy Bypass -File scripts/flash_factory.ps1
powershell -ExecutionPolicy Bypass -File scripts/simulate_esp32.ps1 -Port COM7
```

The simulation command sends `GET_INFO`, `START_UPDATE`, sequential `DATA`
packets, `END_UPDATE`, and `RESET`, matching the command flow intended for
ESP32 Phase 2.
