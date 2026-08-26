# TM4C123GXL OTA Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use axon:subagent-driven-development (recommended) or axon:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify a CCS/TivaWare A/B bootloader, two demonstration applications, and a Python UART updater for the TM4C123GXL.

**Architecture:** Hardware-independent C modules implement CRC, image validation, packet parsing, metadata selection, and boot decisions behind small interfaces that compile on both GCC and TI Arm Clang. TivaWare adapters provide UART, Flash, reset, watchdog, and jump operations on the board. A Python package implements the same wire format and drives updates over USB-UART.

**Tech Stack:** C11, TI Arm Clang in CCS 21.0, TivaWare C Series 2.2.0.295, CMake 4.x, Ninja/GCC, Python 3.14, pytest, pyserial.

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

---

## File Map

```text
CMakeLists.txt                         Host C build and CTest entry point
cmake/tm4c_host.cmake                 Strict host compiler settings
tm4c123gxl/common/inc/ota_config.h     Memory map and protocol constants
tm4c123gxl/common/inc/ota_types.h      Fixed wire/image/state types
tm4c123gxl/common/inc/ota_crc32.h      CRC API
tm4c123gxl/common/src/ota_crc32.c      CRC implementation
tm4c123gxl/common/inc/ota_protocol.h   Packet codec/parser API
tm4c123gxl/common/src/ota_protocol.c   Bounded streaming parser
tm4c123gxl/common/inc/ota_image.h      Image/header validation API
tm4c123gxl/common/src/ota_image.c      Header/vector validation
tm4c123gxl/common/inc/ota_metadata.h   Dual-copy metadata API
tm4c123gxl/common/src/ota_metadata.c   Record validation/selection/commit
tm4c123gxl/common/inc/ota_boot.h       Boot-decision state machine API
tm4c123gxl/common/src/ota_boot.c       Confirmation/attempt/rollback logic
tm4c123gxl/bootloader/inc/bl_hal.h     Hardware boundary
tm4c123gxl/bootloader/inc/bl_update.h  Update-session API
tm4c123gxl/bootloader/src/bl_hal_tm4c.c TivaWare UART/Flash/watchdog/reset
tm4c123gxl/bootloader/src/bl_update.c  Command and update state machine
tm4c123gxl/bootloader/src/bl_main.c    Startup decision and command loop
tm4c123gxl/bootloader/src/bl_jump.c    VTOR/MSP/application jump
tm4c123gxl/bootloader/startup/startup_ccs.c Vector table and reset handler
tm4c123gxl/bootloader/linker/bootloader.cmd Bootloader placement
tm4c123gxl/bootloader/.project         CCS project metadata
tm4c123gxl/bootloader/.cproject        CCS compiler/linker configuration
tm4c123gxl/application/inc/boot_confirm.h Confirmation mailbox API
tm4c123gxl/application/src/boot_confirm.c Mailbox write and system reset
tm4c123gxl/application/src/app_main.c  LED/self-test demonstration app
tm4c123gxl/application/startup/startup_ccs.c Application vectors
tm4c123gxl/application/linker/app_slot_a.cmd Slot A placement
tm4c123gxl/application/linker/app_slot_b.cmd Slot B placement
tm4c123gxl/application/.project        CCS project metadata
tm4c123gxl/application/.cproject       Slot A/B build configurations
pc_tool/pyproject.toml                 Python package and test dependencies
pc_tool/src/tm4c_ota/protocol.py       Packet codec and serial transport
pc_tool/src/tm4c_ota/image.py          Image parsing and validation
pc_tool/src/tm4c_ota/client.py         GET_INFO/update/reset client
pc_tool/src/tm4c_ota/cli.py            Console command interface
pc_tool/tests/                         Python protocol/client tests
tests/c/                               Host C unit and integration tests
scripts/build_ccs.ps1                  Reproducible CCS headless build
scripts/package_image.py               Header-page image packager
scripts/hardware_acceptance.ps1        Board acceptance sequence
docs/PROTOCOL.md                       Byte-accurate protocol reference
docs/HARDWARE-TEST.md                  Wiring and acceptance checklist
```

### Task 1: Host Build, Configuration, Types, and CRC

**Files:** Create `CMakeLists.txt`, `cmake/tm4c_host.cmake`, `tm4c123gxl/common/inc/ota_config.h`, `tm4c123gxl/common/inc/ota_types.h`, `tm4c123gxl/common/inc/ota_crc32.h`, `tm4c123gxl/common/src/ota_crc32.c`, `tests/c/test_crc32.c`, and `tests/c/test_config.c`.

**Interfaces:** Produces `ota_crc32_init(void)`, `ota_crc32_update(uint32_t, const uint8_t *, size_t)`, and `ota_crc32_finish(uint32_t)`. Produces constants for every address, size, protocol limit, timeout, and retry count. Produces packed `ota_version_t`, `ota_firmware_header_t`, `ota_slot_record_t`, and `ota_metadata_record_t` with compile-time size assertions.

- [ ] **Step 1: Add failing configuration and CRC tests**

```c
CHECK(OTA_BOOTLOADER_START + OTA_BOOTLOADER_SIZE == OTA_SLOT_A_START);
CHECK(OTA_SLOT_A_START + OTA_SLOT_SIZE == OTA_SLOT_B_START);
CHECK(OTA_SLOT_B_START + OTA_SLOT_SIZE == OTA_METADATA_COPY0_START);
CHECK(OTA_METADATA_COPY1_START + OTA_FLASH_PAGE_SIZE == OTA_FLASH_END);
CHECK(ota_crc32((const uint8_t *)"123456789", 9u) == UINT32_C(0xCBF43926));
```

- [ ] **Step 2: Configure and run the tests to prove the APIs are absent**

Run: `cmake -S . -B build/host -G Ninja && cmake --build build/host && ctest --test-dir build/host --output-on-failure`

Expected: compilation fails because `ota_config.h` and `ota_crc32.h` do not exist.

- [ ] **Step 3: Implement constants, fixed layouts, and incremental CRC**

Use `UINT32_C` for address arithmetic, reject non-C11 compilers, implement the reflected byte loop with polynomial `UINT32_C(0xEDB88320)`, and expose this convenience wrapper:

```c
static inline uint32_t ota_crc32(const uint8_t *data, size_t length) {
    return ota_crc32_finish(ota_crc32_update(ota_crc32_init(), data, length));
}
```

- [ ] **Step 4: Run strict host tests**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure`

Expected: `crc32` and `config` pass with `-Wall -Wextra -Werror -Wpedantic`.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt cmake tm4c123gxl/common tests/c && git commit -m "feat: add ota memory model and crc"`

### Task 2: Packet Codec and Streaming Parser

**Files:** Create `tm4c123gxl/common/inc/ota_protocol.h`, `tm4c123gxl/common/src/ota_protocol.c`, and `tests/c/test_protocol.c`; modify `CMakeLists.txt`.

**Interfaces:** Produces `ota_packet_encode(const ota_packet_t *, uint8_t *, size_t, size_t *)`, `ota_parser_init(ota_parser_t *)`, and `ota_parser_consume(ota_parser_t *, uint8_t, uint32_t, ota_packet_t *)`. Parser results are `OTA_PARSE_MORE`, `OTA_PARSE_PACKET`, `OTA_PARSE_BAD_CRC`, `OTA_PARSE_BAD_LENGTH`, and `OTA_PARSE_TIMEOUT`.

- [ ] **Step 1: Add failing tests for a golden packet and parser recovery**

```c
const uint8_t golden[] = {0x55,0xAA,0x01,0x01,0x34,0x12,0x00,0x00,
                          0x05,0x66,0x5B,0x4F};
CHECK(encode_get_info(UINT16_C(0x1234), actual, sizeof actual) == sizeof golden);
CHECK(memcmp(actual, golden, sizeof golden) == 0);
CHECK(feed_bytes("noise + bad CRC + golden", &packet) == OTA_PARSE_PACKET);
CHECK(packet.command == OTA_CMD_GET_INFO && packet.sequence == UINT16_C(0x1234));
```

- [ ] **Step 2: Run the focused test and observe failure**

Run: `cmake --build build/host && build\host\tests\test_protocol.exe`

Expected: compile failure for the missing protocol API.

- [ ] **Step 3: Implement bounded codec/parser**

Use an explicit state enum for both SOF bytes, fixed header, payload, and CRC. Reject `length > 256` before indexing payload; reset after timeout using wrap-safe `uint32_t` subtraction; on a mismatched second SOF byte, treat `0x55` as a possible new first SOF.

- [ ] **Step 4: Test zero/max payload, every truncation, CRC corruption, timeout, and resynchronization**

Run: `ctest --test-dir build/host -R protocol --output-on-failure`

Expected: all protocol cases pass under strict warnings.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tm4c123gxl/common tests/c/test_protocol.c && git commit -m "feat: add ota packet protocol"`

### Task 3: Firmware Header, Packaging, and Validation

**Files:** Create `tm4c123gxl/common/inc/ota_image.h`, `tm4c123gxl/common/src/ota_image.c`, `tests/c/test_image.c`, `scripts/package_image.py`, and `pc_tool/tests/test_packager.py`; modify `CMakeLists.txt`.

**Interfaces:** Produces `ota_header_validate(const ota_firmware_header_t *, ota_slot_t)`, `ota_vector_validate(uint32_t initial_msp, uint32_t reset_vector, ota_slot_t)`, and `ota_image_validate(ota_reader_t, ota_slot_t, ota_image_info_t *)`. Packager arguments are `--slot A|B --version X.Y.Z --input app.bin --output firmware.bin`.

- [ ] **Step 1: Add failing boundary and golden-image tests**

```c
CHECK(ota_vector_validate(UINT32_C(0x20008000), UINT32_C(0x00008401), OTA_SLOT_A) == OTA_IMAGE_OK);
CHECK(ota_vector_validate(UINT32_C(0x1FFFFFFC), UINT32_C(0x00008401), OTA_SLOT_A) == OTA_IMAGE_BAD_MSP);
CHECK(ota_vector_validate(UINT32_C(0x20008000), UINT32_C(0x00024001), OTA_SLOT_A) == OTA_IMAGE_BAD_RESET);
```

Python asserts that the output is exactly `1024 + len(payload)`, bytes 32..1023 are `0xFF`, header CRC validates with its CRC field zeroed, and payload CRC equals `binascii.crc32(payload)`.

- [ ] **Step 2: Run focused tests and confirm failure**

Run: `cmake --build build/host && ctest --test-dir build/host -R image --output-on-failure; py -m pytest pc_tool/tests/test_packager.py -q`

Expected: missing image API and packaging script failures.

- [ ] **Step 3: Implement the 32-byte header and packager**

Define explicit serialized offsets instead of relying on compiler padding. Validate magic, schema, reserved-zero fields, slot, version component range, size `1..112640`, header CRC, MSP alignment/range, Thumb bit, and reset address payload range. Package the linked application `.bin` after confirming its first eight bytes form a valid vector table for the requested slot.

- [ ] **Step 4: Run C and Python image tests**

Run: `ctest --test-dir build/host -R image --output-on-failure; py -m pytest pc_tool/tests/test_packager.py -q`

Expected: all image and packager tests pass.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tm4c123gxl/common tests/c/test_image.c scripts pc_tool && git commit -m "feat: define and package ota images"`

### Task 4: Transactional Metadata and Boot Decisions

**Files:** Create `tm4c123gxl/common/inc/ota_metadata.h`, `tm4c123gxl/common/src/ota_metadata.c`, `tm4c123gxl/common/inc/ota_boot.h`, `tm4c123gxl/common/src/ota_boot.c`, `tests/c/test_metadata.c`, and `tests/c/test_boot.c`; modify `CMakeLists.txt`.

**Interfaces:** `ota_metadata_load(const ota_metadata_io_t *, ota_metadata_record_t *, unsigned *)` selects a valid copy. `ota_metadata_commit(const ota_metadata_io_t *, const ota_metadata_record_t *, unsigned)` writes and verifies the older page. `ota_boot_decide(ota_metadata_record_t *, const ota_confirmation_t *, ota_image_probe_fn, void *)` returns `OTA_BOOT_STAY`, `OTA_BOOT_SLOT_A`, or `OTA_BOOT_SLOT_B` plus whether metadata must be committed.

- [ ] **Step 1: Add failing table-driven tests**

```c
CASE("newest valid copy", valid_gen_4, valid_gen_5, SELECT_COPY_1);
CASE("torn newer copy", valid_gen_4, partial_gen_5, SELECT_COPY_0);
CASE("generation wrap", valid_gen_fffffffe, valid_gen_1, SELECT_COPY_1);
CASE("pending attempt 3", active_a_pending_b_attempt_2, no_confirm, BOOT_B_ATTEMPT_3);
CASE("pending exhausted", active_a_pending_b_attempt_3, no_confirm, FAIL_B_BOOT_A);
CASE("valid confirmation", active_a_pending_b, confirm_b, ACTIVATE_B);
```

- [ ] **Step 2: Run tests to confirm missing metadata/boot APIs**

Run: `cmake --build build/host && ctest --test-dir build/host -R "metadata|boot" --output-on-failure`

Expected: compilation fails for missing headers.

- [ ] **Step 3: Implement record validation, copy selection, commit, and pure boot transitions**

CRC each fixed-size record with its CRC field zeroed. Compare generations with `(int32_t)(left - right) > 0`. Commit by erase destination, program record, read back, byte-compare, and revalidate CRC. Do not erase the previous valid copy in the same transaction.

- [ ] **Step 4: Inject erase/program/read failures and verify invariants**

Run: `ctest --test-dir build/host -R "metadata|boot" --output-on-failure`

Expected: every injected interruption leaves at least one selectable record; no boot decision selects an invalid image.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tm4c123gxl/common tests/c && git commit -m "feat: add transactional ota state management"`

### Task 5: CCS Projects, Startup, and Linker Placement

**Files:** Create both CCS project metadata sets, both startup files, all three linker command files, `scripts/build_ccs.ps1`, and `.gitignore`; create `tm4c123gxl/bootloader/src/bl_main.c` and `tm4c123gxl/application/src/app_main.c` as minimal buildable entry points.

**Interfaces:** `scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295` imports and builds `bootloader`, `application/SlotA`, and `application/SlotB`, then copies `.out`, `.bin`, and `.map` files to `artifacts/`.

- [ ] **Step 1: Add a static layout validator before CCS files exist**

The script must fail unless map files contain `.intvecs` at `0x00000000`, `0x00008400`, and `0x00024000` respectively, and unless image end symbols remain below each configured limit.

- [ ] **Step 2: Run the build script and observe the missing-project failure**

Run: `powershell -ExecutionPolicy Bypass -File scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295`

Expected: nonzero exit with a precise missing CCS project message.

- [ ] **Step 3: Create TI Arm Clang projects and placement files**

Set device `TM4C123GH6PM`, include `${TIVAWARE_ROOT}`, link `driverlib.lib`, emit TI-TXT/map/bin outputs, and define `OTA_APP_SLOT=0` or `1` per application configuration. Reserve the confirmation mailbox at `0x20007FC0..0x20007FFF` as `NOINIT`; application RAM ends at `0x20007FBF`. Add linker assertions for every Flash and SRAM boundary.

- [ ] **Step 4: Build all configurations and inspect artifacts**

Run: `powershell -ExecutionPolicy Bypass -File scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295`

Expected: three successful builds and six primary artifacts (`.out` and `.bin`) plus map files; validator reports all vector addresses and limits correct.

- [ ] **Step 5: Commit**

Run: `git add .gitignore scripts tm4c123gxl && git commit -m "build: add ccs bootloader and slot projects"`

### Task 6: TM4C Hardware Abstraction and Flash Protection

**Files:** Create `tm4c123gxl/bootloader/inc/bl_hal.h`, `tm4c123gxl/bootloader/src/bl_hal_tm4c.c`, and `tests/c/test_flash_guard.c`; modify CCS and host build files.

**Interfaces:** Produces UART0 log, UART1 byte read/write with millisecond timeout, monotonic milliseconds, watchdog service, software reset, Flash read, guarded erase/program, and metadata raw I/O. Guard API is host-testable as `bl_flash_range_allowed(uint32_t address, size_t length, ota_slot_t active, bl_flash_operation_t operation)`.

- [ ] **Step 1: Add exhaustive range-guard tests**

Test first/last byte, zero length, `address + length` overflow, crossing boundaries, bootloader overlap, active-slot overlap, inactive payload/header access, and each metadata page. Assert erase addresses and lengths are page-aligned.

- [ ] **Step 2: Run the focused guard test and observe failure**

Run: `cmake --build build/host && ctest --test-dir build/host -R flash_guard --output-on-failure`

Expected: missing guard API compilation failure.

- [ ] **Step 3: Implement guard first, then TivaWare adapters**

Configure clocks from the 16 MHz crystal to 80 MHz, enable GPIOA/B and UART0/1, use ROM driverlib calls where available, poll UART1 without unbounded waits, and read back every Flash program. Convert each driverlib failure into a stable `ota_error_t` without retrying destructive operations internally.

- [ ] **Step 4: Run host tests and CCS builds**

Run: `ctest --test-dir build/host -R flash_guard --output-on-failure; powershell -ExecutionPolicy Bypass -File scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295`

Expected: range tests pass and bootloader remains under 32 KB.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tests/c tm4c123gxl scripts && git commit -m "feat: add protected tm4c flash and uart hal"`

### Task 7: Bootloader Update Engine

**Files:** Create `tm4c123gxl/bootloader/inc/bl_update.h`, `tm4c123gxl/bootloader/src/bl_update.c`, and `tests/c/test_update.c`; modify `bl_main.c` and build files.

**Interfaces:** `bl_update_init(bl_update_t *, const bl_services_t *)`, `bl_update_handle(bl_update_t *, const ota_packet_t *, ota_packet_t *)`, and `bl_update_poll(bl_update_t *, uint32_t)` implement `IDLE`, `RECEIVING`, `VERIFYING`, and `READY_TO_BOOT`. Services inject Flash, metadata, image-read, reset, and info operations.

- [ ] **Step 1: Add a failing successful-transfer integration test**

```c
send_start(valid_header_b); expect_ack(OTA_CMD_START_UPDATE, 0);
send_data(0, payload, 256); expect_ack(OTA_CMD_DATA, 0);
send_duplicate_data(0, payload, 256); expect_ack_without_flash_write(0);
send_remaining_data(); send_end(); expect_ack(OTA_CMD_END_UPDATE, last_seq);
CHECK(metadata.slot_b.state == OTA_SLOT_PENDING);
CHECK(header_write_happened_after_payload_verify);
```

- [ ] **Step 2: Run the update test and observe missing engine failure**

Run: `cmake --build build/host && ctest --test-dir build/host -R update --output-on-failure`

Expected: missing `bl_update` API compilation failure.

- [ ] **Step 3: Implement all commands and stable NACK codes**

Implement `GET_INFO`, `START_UPDATE`, `DATA`, `END_UPDATE`, `ABORT`, and `RESET`. Reject response commands from the host. Write payload at `slot_payload_start + received_bytes`; ACK only after readback; treat only the immediately previous identical sequence/length/CRC as a duplicate; program the header page after full CRC/vector validation; commit pending metadata last.

- [ ] **Step 4: Add and run failure matrix**

Cover bad state, active target, wrong linked slot, zero/oversize payload, bad header, bad CRC, wrong sequence, changed duplicate, Flash failure, timeout, abort, early end, late data, reset injection after every write, and error reporting through `GET_INFO`.

Run: `ctest --test-dir build/host -R update --output-on-failure`

Expected: all cases pass and active-slot bytes never change.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tests/c tm4c123gxl/bootloader && git commit -m "feat: implement uart ota update engine"`

### Task 8: Boot, Jump, Confirmation, and Demo Applications

**Files:** Create `tm4c123gxl/bootloader/src/bl_jump.c`, `tm4c123gxl/application/inc/boot_confirm.h`, and `tm4c123gxl/application/src/boot_confirm.c`; complete both `bl_main.c` and `app_main.c`; create `tests/c/test_confirmation.c`.

**Interfaces:** Application calls `boot_confirm(ota_slot_t, ota_version_t)`; it writes a 64-byte mailbox containing magic, schema, slot, version, and CRC, issues DSB/ISB, then writes SYSRESETREQ. Bootloader calls `bl_confirmation_read_and_clear()` before normal initialization and passes a validated value into `ota_boot_decide`.

- [ ] **Step 1: Add failing mailbox and startup-order tests**

Test correct CRC, single-bit corruption, wrong version/slot, stale mailbox, and clear-on-consume. A link/map test asserts the mailbox is exactly at `0x20007FC0` and is not in either startup zero table.

- [ ] **Step 2: Run host and map tests to observe failure**

Run: `cmake --build build/host && ctest --test-dir build/host -R confirmation --output-on-failure`

Expected: missing confirmation API failure.

- [ ] **Step 3: Implement early confirmation, boot loop, and jump**

On boot: capture/clear mailbox, initialize clock/UART/watchdog, load or reconstruct metadata, validate candidate images, commit boot-attempt/state changes, wait a configurable two-second UART update window, then jump. Before branching: disable global interrupts and NVIC sources, disable SysTick, disable UART interrupts, set VTOR, load MSP, and call the Thumb Reset Handler through a `noreturn` function.

- [ ] **Step 4: Implement demo app variants and rebuild**

Both slot builds log slot/version on UART0 and blink LED green during the three-second self-test. Normal builds confirm and reset; `NO_CONFIRM=1` builds blink red and intentionally reset without writing a mailbox. Run host tests and all CCS builds; inspect vector/MSP values in both packaged binaries.

- [ ] **Step 5: Commit**

Run: `git add tests/c tm4c123gxl scripts && git commit -m "feat: add confirmed boot and rollback applications"`

### Task 9: Python Protocol Client and CLI

**Files:** Create `pc_tool/pyproject.toml`, all modules under `pc_tool/src/tm4c_ota/`, and tests under `pc_tool/tests/`.

**Interfaces:** `Packet.encode/decode`, `SerialTransport.request`, `OtaClient.get_info`, `OtaClient.update`, and `OtaClient.reset`; console entry point is `tm4c-ota` with `info`, `update`, and `reset` subcommands.

- [ ] **Step 1: Add failing codec and fake-serial client tests**

Use the same golden GET_INFO bytes as C. Fake serial must simulate fragmented reads, noise, CRC error then valid response, timeout, NACK, lost ACK causing duplicate DATA, and retry exhaustion. Assert default timeout `1.0` seconds and retries `3`.

- [ ] **Step 2: Create a virtual environment and prove tests fail**

Run: `py -m venv .venv; .\.venv\Scripts\python.exe -m pip install -e ".\pc_tool[test]"; .\.venv\Scripts\python.exe -m pytest pc_tool/tests -q`

Expected: import or missing API failures.

- [ ] **Step 3: Implement strict protocol, client, and CLI**

Decode with `struct.Struct("<2sBBHH")`; use `binascii.crc32`; discard noise with a bounded receive buffer; match ACK/NACK command and sequence; retry timeout and packet-CRC failures only; stop immediately on semantic NACKs. `update` validates the 1 KB header page, compares target with `GET_INFO`, sends header structure in START, sends payload in 256-byte chunks, prints progress to stderr, sends END, and verifies pending state with GET_INFO.

- [ ] **Step 4: Run Python tests and CLI help smoke test**

Run: `.\.venv\Scripts\python.exe -m pytest pc_tool/tests -q; .\.venv\Scripts\tm4c-ota.exe --help`

Expected: tests pass and help lists `info`, `update`, and `reset`.

- [ ] **Step 5: Commit**

Run: `git add pc_tool && git commit -m "feat: add pc uart ota client"`

### Task 10: Cross-Language and Fault-Injection Verification

**Files:** Create `tests/vectors/protocol.json`, `tests/vectors/images.json`, `tests/c/test_vectors.c`, `pc_tool/tests/test_vectors.py`, and `tests/c/test_fault_matrix.c`; modify CMake files.

**Interfaces:** JSON vectors are canonical byte strings consumed independently by C and Python. The fault harness snapshots bootloader, active-slot, target-slot, and metadata memory after each simulated power cut and restarts from that snapshot.

- [ ] **Step 1: Add canonical vectors and make both implementations consume them**

Include zero and 256-byte packets, every command, ACK/NACK, header A/B, metadata generations 0/1/`0xFFFFFFFE`, and corrupt variants. Assert byte-for-byte encoded equality and identical accept/reject results.

- [ ] **Step 2: Add exhaustive simulated reset points**

For each erase, program, verify, and metadata commit callback, terminate the current update, reconstruct all modules from the retained byte arrays, and assert the previously active image is bootable. For a fully completed update, assert only the new image becomes pending.

- [ ] **Step 3: Run the full host suite**

Run: `cmake --build build/host && ctest --test-dir build/host --output-on-failure; .\.venv\Scripts\python.exe -m pytest pc_tool/tests -q`

Expected: all C and Python tests pass, including shared vectors and every injected reset point.

- [ ] **Step 4: Run sanitizers where supported**

Run a second CMake configuration with GCC `-fsanitize=address,undefined -fno-omit-frame-pointer`, then execute CTest.

Expected: zero sanitizer findings; if Windows ASan is unavailable in the installed GCC, record that limitation and retain UBSan plus strict warnings.

- [ ] **Step 5: Commit**

Run: `git add CMakeLists.txt tests pc_tool && git commit -m "test: add ota fault and compatibility coverage"`

### Task 11: Documentation and Automated Hardware Acceptance

**Files:** Create `docs/PROTOCOL.md`, `docs/HARDWARE-TEST.md`, and `scripts/hardware_acceptance.ps1`; modify `README.md`.

**Interfaces:** Hardware script accepts `-OtaPort COMx -DebugPort COMy -TivaWareRoot path`, captures timestamped logs under `artifacts/hardware/`, packages Slot B, performs update/reset/info, and stops at explicit prompts for ICDI flashing or power interruption.

- [ ] **Step 1: Document exact setup and byte layouts**

Document CCS import/headless build, `TIVAWARE_ROOT`, ICDI debug USB, UART1 wiring, adapter power warning, Python environment, packaging, CLI examples, every field offset/size, CRC coverage, commands, error codes, metadata transitions, and LED meanings.

- [ ] **Step 2: Implement the acceptance runner with fail-fast assertions**

The script must check COM ports and artifacts before touching the board, save `info-before.json`, `update.log`, UART0 boot logs, and `info-after.json`, and assert expected slot/version/state at each automatic stage. Manual prompts must name the exact action and expected LED/log result.

- [ ] **Step 3: Run documentation/static checks and a no-hardware dry run**

Run: `powershell -ExecutionPolicy Bypass -File scripts/hardware_acceptance.ps1 -OtaPort COM_DOES_NOT_EXIST -DebugPort COM_DOES_NOT_EXIST -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295 -DryRun`

Expected: validates artifacts and prints the complete sequence without opening a serial port or changing the board.

- [ ] **Step 4: Execute board acceptance with the user**

Flash bootloader and Slot A v1.0.0 through ICDI; update confirming Slot B v1.0.1 through UART1 and verify B active/A backup; update a non-confirming inactive image and verify three attempts followed by rollback; interrupt UART and power at the documented stages and verify the prior active image boots. Preserve all logs.

- [ ] **Step 5: Commit**

Run: `git add README.md docs scripts && git commit -m "docs: add phase 1 build and hardware acceptance"`

### Task 12: Final Verification and Phase 1 Evidence

**Files:** Create `docs/PHASE1-RESULTS.md`; modify only files required by failures found during verification.

**Interfaces:** Results document records exact tool versions, commands, test counts, artifact sizes, map addresses, hardware wiring, firmware versions, success/rollback logs, power-loss observations, and remaining limitations.

- [ ] **Step 1: Start from a clean generated-output state and run all host tests**

Run: `cmake -S . -B build/host -G Ninja; cmake --build build/host; ctest --test-dir build/host --output-on-failure; .\.venv\Scripts\python.exe -m pytest pc_tool/tests -q`

Expected: all commands exit zero.

- [ ] **Step 2: Build all CCS images and enforce size/layout checks**

Run: `powershell -ExecutionPolicy Bypass -File scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295`

Expected: bootloader is at most 32768 bytes; each payload is at most 112640 bytes; vector tables and metadata pages match the spec; all configurations exit zero.

- [ ] **Step 3: Re-run hardware acceptance and collect fresh logs**

Run: `powershell -ExecutionPolicy Bypass -File scripts/hardware_acceptance.ps1 -OtaPort COMx -DebugPort COMy -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295`

Expected: confirming update, non-confirming rollback, interrupted transfer, and interrupted metadata/update scenarios all pass.

- [ ] **Step 4: Write results and inspect repository state**

Record evidence in `docs/PHASE1-RESULTS.md`. Run `git diff --check`, `git status --short`, and review every diff to ensure generated build output, local CCS metadata, virtual environments, and serial logs are ignored.

- [ ] **Step 5: Commit final evidence**

Run: `git add docs/PHASE1-RESULTS.md && git commit -m "test: record phase 1 acceptance results"`

