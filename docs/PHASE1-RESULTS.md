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
- A partial `v1.0.1` update was interrupted after the first 256 of 376 payload
  bytes by closing the COM7 serial transport. After the packet timeout, Slot A
  remained active and no pending image was created. A subsequent reboot
  discarded the partial update.
- A partial update was also interrupted by issuing `RESET` after the first data
  chunk. The TM4C reset immediately, disconnected COM7 as expected, and rebooted
  with Slot A active, no pending image, and zero update progress.

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
A-to-B update with confirmation, unconfirmed-image rollback to A, interruption
of the host serial session, and reset during an OTA transfer. The extended
fault-injection list in `OTA-PLAN.md` has host-test coverage where applicable.
Actual VDD power removal while Flash is writing and physically unplugging the
USB cable mid-transfer have not been run yet.

## Reproduce

Build, flash, and exercise the board with:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/setup_pc_tool.ps1
powershell -ExecutionPolicy Bypass -File scripts/flash_factory.ps1
powershell -ExecutionPolicy Bypass -File scripts/simulate_esp32.ps1 -Port COM7
& .\.venv\Scripts\python.exe scripts/test_interrupted_transfer.py --port COM7
& .\.venv\Scripts\python.exe scripts/test_reset_during_ota.py --port COM7
```

The simulation command sends `GET_INFO`, `START_UPDATE`, sequential `DATA`
packets, `END_UPDATE`, and `RESET`, matching the command flow intended for
ESP32 Phase 2.
