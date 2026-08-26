# TM4C OTA UART Protocol

All multibyte values are little-endian. UART1 uses PB0/U1RX and PB1/U1TX at 115200 baud, 8-N-1.

## Frame

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | SOF: `55 AA` |
| 2 | 1 | Protocol version: `01` |
| 3 | 1 | Command |
| 4 | 2 | Sequence |
| 6 | 2 | Payload length, 0 through 256 |
| 8 | N | Payload |
| 8+N | 4 | CRC-32/ISO-HDLC |

CRC covers protocol version through payload, using reflected polynomial `0xEDB88320`, initial `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.

| Value | Name | Request payload |
| ---: | --- | --- |
| `01` | GET_INFO | Empty |
| `02` | START_UPDATE | 32-byte firmware header |
| `03` | DATA | Application payload bytes |
| `04` | END_UPDATE | Empty |
| `05` | ABORT | Empty |
| `06` | ACK | Original command and sequence |
| `07` | NACK | Original command, sequence, error |
| `08` | RESET | Empty |

The host sends application bytes only. The bootloader writes the header page after complete payload size, CRC, and vector validation. DATA starts at sequence zero. A byte-identical duplicate of the last accepted DATA packet is ACKed without another Flash write. The host waits one second and retries at most three times.

Update order is GET_INFO, START_UPDATE, DATA chunks, END_UPDATE, then RESET. END_UPDATE marks the new slot PENDING transactionally. Three unconfirmed boots mark it FAILED and select the previous ACTIVE slot. An interrupted transfer has no valid header and cannot boot.
