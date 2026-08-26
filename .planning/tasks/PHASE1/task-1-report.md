# Task 1 Report: Host Build, Configuration, Types, and CRC

## Status

`DONE_WITH_CONCERNS`

Commit: `c5b78a3 feat: add ota memory model and crc`

## Implementation Summary

- Added a CMake host build with CTest targets `crc32` and `config`, C11 as a
  required language level, and GCC/Clang flags `-Wall -Wextra -Werror
  -Wpedantic`.
- Added a single memory/protocol configuration header covering the 256 KB
  Flash map, 32 KB SRAM map, A/B slot/header/payload boundaries, metadata
  pages, UART1 framing, protocol limits, CRC parameters, one-second timeouts,
  and three-attempt retry/rollback limits.
- Added packed wire/state records with C11 compile-time size assertions:
  `ota_version_t` (6), `ota_firmware_header_t` (32), `ota_slot_record_t`
  (24), and `ota_metadata_record_t` (80) bytes.
- Added incremental CRC-32/ISO-HDLC APIs and the specified convenience
  wrapper. The implementation uses the reflected `0xEDB88320` byte loop,
  `0xFFFFFFFF` initialization, and final XOR.
- Added real-code tests for partition boundary arithmetic, fixed layouts,
  configured protocol/retry values, the CRC known vector, incremental CRC,
  and the empty input.

## Files Changed

- `CMakeLists.txt`
- `cmake/tm4c_host.cmake`
- `tm4c123gxl/common/inc/ota_config.h`
- `tm4c123gxl/common/inc/ota_types.h`
- `tm4c123gxl/common/inc/ota_crc32.h`
- `tm4c123gxl/common/src/ota_crc32.c`
- `tests/c/test_crc32.c`
- `tests/c/test_config.c`

## TDD Evidence

The test sources and CMake test targets were created before any production
headers or CRC source.

### RED Command and Output

Command run before production code:

```powershell
cmake -S . -B build/host -G Ninja
```

Observed output:

```text
-- The C compiler identification is GNU 16.1.0
-- Detecting C compiler ABI info
```

The command did not reach the test compilation. The installed Ninja process
blocked during CMake's compiler-ABI try-compile, before it could report the
expected missing `ota_config.h` and `ota_crc32.h` errors. This was reproduced
from a removed, clean `build/host` directory and when invoking the generated
Ninja try-compile directly; `ninja -n -v` printed commands, but normal Ninja
execution blocked before launching `gcc`.

### GREEN Commands and Output

The strict CMake/CTest project was verified using the available MinGW Make
backend because the Ninja executable is blocked by the host environment:

```powershell
cmake -S . -B build/make -G 'MinGW Makefiles'
cmake --build build/make --clean-first
ctest --test-dir build/make -R '^crc32$' --output-on-failure
ctest --test-dir build/make --output-on-failure
```

Final build and test output:

```text
[ 16%] Building C object CMakeFiles/ota_common.dir/tm4c123gxl/common/src/ota_crc32.c.obj
[ 33%] Linking C static library libota_common.a
[ 33%] Built target ota_common
[ 50%] Building C object CMakeFiles/crc32.dir/tests/c/test_crc32.c.obj
[ 66%] Linking C executable crc32.exe
[ 66%] Built target crc32
[ 83%] Building C object CMakeFiles/config.dir/tests/c/test_config.c.obj
[100%] Linking C executable config.exe
[100%] Built target config

Test project D:/TM4AC/OTA Phase 1/.worktrees/phase1-implementation/build/make
    Start 1: crc32
1/1 Test #1: crc32 ............................   Passed    0.06 sec

100% tests passed, 0 tests failed out of 1

Test project D:/TM4AC/OTA Phase 1/.worktrees/phase1-implementation/build/make
    Start 1: crc32
1/2 Test #1: crc32 ............................   Passed    0.01 sec
    Start 2: config
2/2 Test #2: config ...........................   Passed    0.05 sec

100% tests passed, 0 tests failed out of 2
```

Also run:

```powershell
git diff --check
git diff --cached --check
```

Both completed with no whitespace errors.

## Self-Review

- Verified all required Flash partition equations, including both metadata
  pages ending at exclusive Flash end `0x00040000`.
- Verified Slot A/B payload starts are `0x00008400` and `0x00024000`, with a
  110 KB payload limit.
- Verified the required `123456789` CRC vector and split-update result both
  equal `0xCBF43926`; the empty input produces zero.
- Confirmed only the eight Task 1 implementation/test files were committed.
  The coordinator-owned progress file and implementation plan were not
  modified.

## Concern

The required `-G Ninja` configure/build/CTest sequence cannot complete in
this host environment because the installed Ninja executable blocks in
CMake's compiler ABI try-compile before any project source compiles. The
equivalent strict CMake build and both CTests pass with `MinGW Makefiles`, and
direct strict GCC compilation/runs also passed. Consequently, the expected
compiler-level RED missing-header output and a final Ninja-backed CTest run
could not be captured.

## Follow-up: UART Assignment Constants

Follow-up commit: `dc42e54 fix: define uart assignments`

Added portable numeric configuration identifiers for the UART assignments:

- `OTA_DEBUG_UART_INSTANCE = 0` for UART0 debug output.
- `OTA_OTA_UART_INSTANCE = 1` for UART1 OTA transport.
- `OTA_OTA_UART_RX_GPIO_PORT_INDEX = 1` and
  `OTA_OTA_UART_RX_GPIO_PIN_INDEX = 0` for PB0 / UART1 RX.
- `OTA_OTA_UART_TX_GPIO_PORT_INDEX = 1` and
  `OTA_OTA_UART_TX_GPIO_PIN_INDEX = 1` for PB1 / UART1 TX.

The identifiers deliberately use host-independent numeric port/pin indices;
they do not include TivaWare headers or hardware-library symbols.

### Follow-up RED Command and Output

```powershell
cmake --build build/make --target config --clean-first
```

The build failed as intended before the constants were added, with errors
including:

```text
error: 'OTA_DEBUG_UART_INSTANCE' undeclared
error: 'OTA_OTA_UART_INSTANCE' undeclared
error: 'OTA_OTA_UART_RX_GPIO_PORT_INDEX' undeclared
error: 'OTA_GPIO_PORT_B_INDEX' undeclared
error: 'OTA_OTA_UART_TX_GPIO_PIN_INDEX' undeclared
```

### Follow-up GREEN Commands and Output

```powershell
cmake --build build/make --target config
ctest --test-dir build/make -R '^config$' --output-on-failure
cmake --build build/make
ctest --test-dir build/make --output-on-failure
```

Focused result:

```text
[100%] Built target config
1/1 Test #2: config ... Passed
100% tests passed, 0 tests failed out of 1
```

Full result:

```text
[100%] Built target config
1/2 Test #1: crc32 ... Passed
2/2 Test #2: config ... Passed
100% tests passed, 0 tests failed out of 2
```
