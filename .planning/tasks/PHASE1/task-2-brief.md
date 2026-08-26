# Task 2: Packet Codec and Streaming Parser

## Binding Constraints

- Packet format: `SOF(2) | protocol_ver(1) | cmd(1) | seq(2) | length(2) | payload(0..256) | CRC32(4)`.
- SOF is `0x55 0xAA`, protocol version is 1, all multibyte fields are little-endian.
- CRC-32/ISO-HDLC covers protocol version through the end of payload.
- Parser is bounded, timeout-aware, and resynchronizes after malformed input.
- Use existing Task 1 constants/types/CRC and strict host CMake settings.

## Files and Interfaces

Create `tm4c123gxl/common/inc/ota_protocol.h`, `tm4c123gxl/common/src/ota_protocol.c`, `tests/c/test_protocol.c`; modify `CMakeLists.txt`.

Produce:

```c
ota_protocol_result_t ota_packet_encode(const ota_packet_t *, uint8_t *, size_t, size_t *);
void ota_parser_init(ota_parser_t *);
ota_parse_result_t ota_parser_consume(ota_parser_t *, uint8_t, uint32_t, ota_packet_t *);
```

Parser results include `OTA_PARSE_MORE`, `OTA_PARSE_PACKET`, `OTA_PARSE_BAD_CRC`, `OTA_PARSE_BAD_LENGTH`, and `OTA_PARSE_TIMEOUT`.

## TDD Requirements

1. Write failing tests first for zero payload, 256-byte payload, little-endian fields, output capacity, invalid length, truncation at every byte, corrupt CRC, noise, timeout, and resynchronization.
2. Include a golden GET_INFO packet generated independently and assert exact bytes. Recalculate the golden CRC rather than copying the possibly incorrect illustrative bytes from the plan.
3. Implement explicit states for SOF0, SOF1, fixed header, payload, and CRC. Never index beyond 256 bytes.
4. Use wrap-safe `uint32_t` elapsed-time subtraction. On mismatched SOF1, retain `0x55` as a possible new SOF0.
5. Run focused `protocol` CTest then the full suite with strict warnings; commit subject `feat: add ota packet protocol`.

Do not modify plan/progress files or begin later tasks. Append full RED/GREEN evidence to `.planning/tasks/PHASE1/task-2-report.md` and report status succinctly.
