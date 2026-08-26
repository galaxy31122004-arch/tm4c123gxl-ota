# Task 1: Host Build, Configuration, Types, and CRC

## Global Constraints

- MCU is TM4C123GH6PM with 256 KB Flash and 32 KB SRAM.
- Bootloader is 32 KB at `0x00000000`; Slot A and B are 111 KB each; two metadata pages are 1 KB each.
- Slot A payload begins at `0x00008400`; Slot B payload begins at `0x00024000`; payload limit is 110 KB.
- UART0 is debug output; UART1 PB0/PB1 is OTA at 115200 baud, 8-N-1.
- Protocol SOF is `55 AA`, protocol version is 1, payload limit is 256 bytes, and all multibyte fields are little-endian.
- CRC is CRC-32/ISO-HDLC with reflected polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.
- Only the inactive application slot and metadata pages may be erased or programmed.
- Pending firmware receives at most three boot attempts before rollback.
- No ESP32 or ThingsBoard code is part of Phase 1.
- Repository paths and CCS projects must not embed a machine-specific absolute TivaWare path; use `TIVAWARE_ROOT`.

## Files

Create `CMakeLists.txt`, `cmake/tm4c_host.cmake`, `tm4c123gxl/common/inc/ota_config.h`, `tm4c123gxl/common/inc/ota_types.h`, `tm4c123gxl/common/inc/ota_crc32.h`, `tm4c123gxl/common/src/ota_crc32.c`, `tests/c/test_crc32.c`, and `tests/c/test_config.c`.

## Interfaces

Produce `ota_crc32_init(void)`, `ota_crc32_update(uint32_t, const uint8_t *, size_t)`, and `ota_crc32_finish(uint32_t)`. Produce constants for every address, size, protocol limit, timeout, and retry count. Produce packed `ota_version_t`, `ota_firmware_header_t`, `ota_slot_record_t`, and `ota_metadata_record_t` with compile-time size assertions.

## Required TDD Sequence

1. Add failing configuration and CRC tests containing these checks:

```c
CHECK(OTA_BOOTLOADER_START + OTA_BOOTLOADER_SIZE == OTA_SLOT_A_START);
CHECK(OTA_SLOT_A_START + OTA_SLOT_SIZE == OTA_SLOT_B_START);
CHECK(OTA_SLOT_B_START + OTA_SLOT_SIZE == OTA_METADATA_COPY0_START);
CHECK(OTA_METADATA_COPY1_START + OTA_FLASH_PAGE_SIZE == OTA_FLASH_END);
CHECK(ota_crc32((const uint8_t *)"123456789", 9u) == UINT32_C(0xCBF43926));
```

2. Run `cmake -S . -B build/host -G Ninja`, `cmake --build build/host`, and `ctest --test-dir build/host --output-on-failure`; retain the expected RED evidence.
3. Implement constants and fixed layouts with `UINT32_C`, require C11, implement the reflected byte loop using `UINT32_C(0xEDB88320)`, and expose:

```c
static inline uint32_t ota_crc32(const uint8_t *data, size_t length) {
    return ota_crc32_finish(ota_crc32_update(ota_crc32_init(), data, length));
}
```

4. Run the configure/build/CTest sequence again. Both `crc32` and `config` must pass under `-Wall -Wextra -Werror -Wpedantic` with pristine output.
5. Commit only Task 1 files with subject `feat: add ota memory model and crc`.

## Scope

Implement only Task 1. Do not modify `docs/PHASE1-IMPLEMENTATION-PLAN.md` or `.planning/tasks/PHASE1/progress.json`. Do not begin packet parsing or any later task.
