# Phase 1 Specification: TM4C123GXL OTA Bootloader

## 1. Scope

Phase 1 delivers a self-contained OTA bootloader for the TM4C123GH6PM on the
EK-TM4C123GXL LaunchPad. A PC and an external USB-UART adapter emulate the
future ESP32 transport. ESP32 and ThingsBoard integration are outside this
phase.

The deliverables are:

- A CCS/TivaWare bootloader project.
- Two CCS application configurations linked for Slot A and Slot B.
- A Python PC tool supporting `info`, `update`, and `reset`.
- Host-side unit and integration tests.
- Hardware test scripts and documented wiring/procedures.

Phase 1 is complete when a valid image can replace the inactive slot, boot,
confirm, and become active, while an unconfirmed image automatically rolls
back to the previously active image.

## 2. Hardware and Toolchain

- Target: TM4C123GH6PM, 256 KB Flash, 32 KB SRAM.
- Board: EK-TM4C123GXL LaunchPad.
- Toolchain: Code Composer Studio with TivaWare.
- UART0 (`PA0/U0RX`, `PA1/U0TX`): debug logging through the onboard ICDI.
- UART1 (`PB0/U1RX`, `PB1/U1TX`): OTA transport at 115200 baud, 8-N-1.
- USB-UART wiring: adapter TX to PB0, adapter RX to PB1, and common GND.
- The adapter power pin must remain disconnected when the board is powered
  from its DEBUG USB connector.

## 3. Repository Structure

```text
tm4c123gxl/
  common/
    inc/
    src/
  bootloader/
    inc/
    src/
    linker/
  application/
    inc/
    src/
    linker/
pc_tool/
tests/
docs/
```

Shared wire-format definitions are kept independent from TivaWare so their
behavior can be tested on the host. Hardware access remains behind UART and
Flash interfaces.

## 4. Memory Map

All partition boundaries are aligned to the TM4C Flash erase page size of
1 KB.

| Region | Start | End | Size |
| --- | ---: | ---: | ---: |
| Bootloader | `0x00000000` | `0x00007FFF` | 32 KB |
| Application Slot A | `0x00008000` | `0x00023BFF` | 111 KB |
| Application Slot B | `0x00023C00` | `0x0003F7FF` | 111 KB |
| Metadata copy 1 | `0x0003F800` | `0x0003FBFF` | 1 KB |
| Metadata copy 2 | `0x0003FC00` | `0x0003FFFF` | 1 KB |

The first 1 KB page of each application slot stores its firmware header. The
application vector table and payload begin at the following page, leaving a
maximum application payload of 110 KB. `bootloader_config.h` is the only
source of these addresses and sizes.

The common application source is built twice because its vector table and
absolute references must match its slot:

- Slot A payload starts at `0x00008400`.
- Slot B payload starts at `0x00024000`.

Linker assertions fail the build when an image exceeds its partition.

## 5. Firmware Image

Each distributable `.bin` contains a 1 KB header page followed by application
payload bytes. The meaningful header structure occupies the first 32 bytes of
that page and includes:

- Magic and header schema version.
- Target slot.
- Semantic firmware version represented by three unsigned 16-bit values.
- Payload length.
- Payload CRC32.
- Header CRC32.

The remaining header-page bytes are erased (`0xFF`). The header CRC covers the
32-byte structure with its own CRC field treated as zero. Reserved structure
fields must be zero when generated and when validated.
An image is valid only when the header, size, target slot, vector table, and
payload CRC all pass validation.

Vector validation requires the initial MSP to be aligned and within SRAM. The
Reset Handler must have its Thumb bit set and resolve inside the selected
slot's payload range.

## 6. UART Protocol

Packets use this little-endian format:

```text
SOF(2) | protocol_ver(1) | cmd(1) | seq(2) |
length(2) | payload(0..256) | CRC32(4)
```

- SOF is `0x55 0xAA`.
- Protocol version is `1`.
- CRC32 uses CRC-32/ISO-HDLC (polynomial `0x04C11DB7`, reflected input and
  output, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`). It covers bytes
  from `protocol_ver` through the end of payload.
- Maximum payload length is 256 bytes.
- The parser is a bounded state machine that recovers synchronization after
  malformed packets and abandons partial packets after a timeout.

Commands retain the values in `OTA-PLAN.md`:

| Value | Command |
| ---: | --- |
| `0x01` | `GET_INFO` |
| `0x02` | `START_UPDATE` |
| `0x03` | `DATA` |
| `0x04` | `END_UPDATE` |
| `0x05` | `ABORT` |
| `0x06` | `ACK` |
| `0x07` | `NACK` |
| `0x08` | `RESET` |

`GET_INFO` returns bootloader version, protocol version, both slot versions
and states, active and pending slots, pending boot count, maximum payload
size, update progress, and the most recent error code.

NACK packets carry the rejected command, sequence, and a stable error code.
The PC tool uses a one-second response timeout and retries each request no more
than three times. These values are centralized and configurable.

## 7. Update Flow

1. The PC calls `GET_INFO` and selects the inactive slot.
2. The PC tool validates the 1 KB header page locally. `START_UPDATE` supplies
   its 32-byte header structure, including target slot, firmware version,
   payload length, payload CRC, and header CRC. The bootloader rejects the
   active slot, oversized images, and malformed or incompatible headers.
3. After accepting the request, the bootloader invalidates and erases only the
   target slot, then ACKs readiness.
4. `DATA` packets contain only application payload bytes and arrive in
   ascending sequence order. Each accepted chunk is programmed at the target
   payload address and read back before ACK. A duplicate of the last accepted
   packet is ACKed without a second write. Other sequence errors are NACKed.
5. `END_UPDATE` requires the exact announced payload byte count. The
   bootloader reads back the complete payload and validates its CRC and vector
   table. It then programs and verifies the header page last, so an interrupted
   transfer cannot appear valid, and marks the target slot `PENDING`
   transactionally.
6. `RESET` starts the pending-boot process.

`ABORT`, a transfer timeout, disconnect, reset, or power loss leaves the old
active slot unchanged. An incomplete target slot is never bootable. A new
`START_UPDATE` safely erases and restarts that target.

## 8. Metadata and Power-Loss Safety

Metadata uses two independent 1 KB Flash pages. Each record contains:

- Magic and metadata schema version.
- Monotonically increasing generation number.
- State, version, size, CRC, and boot count for both slots.
- Active and pending slot identifiers.
- Last boot/update error.
- Record CRC32.

To commit a change, the bootloader erases the older copy, writes a complete
new record with generation incremented, and verifies it. The previous valid
copy remains untouched until the new copy is verified. At startup, the valid
record with the newest generation is selected. If one page is erased, partial,
or corrupt, the other is used. Generation comparison handles unsigned
wraparound.

If neither metadata page is valid, the bootloader validates both slots. It
chooses a valid Slot A as the initial active image, otherwise a valid Slot B;
if neither is valid it remains in update mode. It then creates fresh metadata.

Supported slot states are `EMPTY`, `VALID`, `ACTIVE`, `PENDING`, `CONFIRMED`,
and `FAILED`. Metadata transitions are performed only by the bootloader.

## 9. Boot, Confirmation, and Rollback

On reset, the bootloader validates metadata and the chosen image before every
jump. A pending image is tried at most three times. The attempt count is
committed before jumping, so reset or power loss during startup consumes an
attempt.

The application performs a short self-test, identifies its version through
UART0 and the board LED, waits approximately three seconds, then calls
`Boot_Confirm()`. This function writes a versioned, CRC-protected confirmation
mailbox into a fixed `.noinit` SRAM area and requests a software reset. The
bootloader startup code must preserve and inspect this mailbox before C runtime
initialization can clear it. A confirmation is accepted only when it identifies
the currently pending slot and version; the mailbox is then cleared.

An accepted confirmation moves the pending image to `ACTIVE` and retains the
previous image as a valid backup. If three attempts occur without a valid
confirmation, the pending slot becomes `FAILED` and the previous active image
is booted. A deliberately faulty application build omits confirmation to test
this path.

Before jumping, the bootloader disables interrupts and SysTick, returns used
peripherals to a known state, sets VTOR, loads MSP from the vector table, and
branches to the application's Reset Handler.

## 10. Flash Safety

All erase and program operations pass through a range-checking Flash driver.
The driver permits writes only to the inactive application slot and metadata
pages. Any range overlap with the bootloader or active slot is rejected before
calling TivaWare. Arithmetic is checked for overflow.

Writes are aligned and padded according to the TM4C Flash programming rules.
Every programmed range is read back. The watchdog remains serviced during
long erase and full-image verification operations, while stalled update
sessions return to a bootable decision after their configured timeout.

## 11. PC Tool

The Python CLI uses `pyserial` and provides:

```text
ota_tool info --port COMx
ota_tool update firmware.bin --port COMx
ota_tool reset --port COMx
```

`update` validates the packaged image before transmission, checks that its
target is inactive, displays byte and percentage progress, retries NACKed or
timed-out packets where appropriate, and reports stable protocol error names.
It never silently changes an image's linked target slot.

## 12. Error Handling

Errors are classified as framing, protocol version, length, packet CRC,
sequence, command state, slot, size, Flash erase/program/verify, image CRC,
header, vector table, metadata, confirmation, and boot errors. The bootloader
must remain responsive or select a previously validated image after every
recoverable error. It never marks an image pending or active before all
required verification succeeds.

## 13. Verification

Host unit tests cover:

- CRC32 known vectors and incremental calculation.
- Packet encode/decode, malformed lengths, CRC errors, timeout, and resync.
- Metadata record validation, newest-copy selection, torn writes, corruption,
  and generation wraparound.
- Boot decisions and all legal state transitions.
- Firmware header and vector-table validation.

Mock Flash/UART integration tests cover:

- Successful update.
- Packet loss, corruption, duplicate, and wrong sequence.
- Retry exhaustion and connection timeout.
- Oversized and wrong-slot firmware.
- Reset at each update stage.
- Power loss during target erase/write and either metadata-copy commit.
- Firmware CRC failure.
- Confirmation success and three-attempt rollback.

Hardware acceptance tests cover:

1. Flash bootloader and version 1.0.0 into Slot A.
2. Update version 1.0.1 into Slot B over UART1.
3. Verify Slot B, boot it, confirm it, and observe B active with A retained.
4. Install a valid but non-confirming image into the inactive slot.
5. Observe three failed attempts and automatic rollback to the prior active
   image.
6. Interrupt power or UART transfer at documented points and verify the prior
   active image still boots.

Build output includes `.out`, `.bin`, and linker map files. Automated host
tests and all CCS configurations must build successfully. Hardware procedures
record UART0 and PC-tool logs so acceptance results are auditable.

## 14. Documentation

The repository documents CCS project import, the required TivaWare location,
all build configurations, ICDI flashing, UART wiring, PC dependencies, CLI
usage, image packaging, the state model, error codes, and the complete
hardware acceptance checklist.
