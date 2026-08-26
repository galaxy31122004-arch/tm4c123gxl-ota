import binascii
import struct
from dataclasses import dataclass

SOF = b"\x55\xaa"
PROTOCOL_VERSION = 1
MAX_PAYLOAD_SIZE = 256
HEADER = struct.Struct("<2sBBHH")
CRC = struct.Struct("<I")
GET_INFO, START_UPDATE, DATA, END_UPDATE, ABORT, ACK, NACK, RESET = range(1, 9)


class PacketError(ValueError):
    pass


@dataclass(frozen=True)
class Packet:
    command: int
    sequence: int
    payload: bytes = b""

    def encode(self):
        if not 0 <= self.command <= 0xFF or not 0 <= self.sequence <= 0xFFFF or len(self.payload) > MAX_PAYLOAD_SIZE:
            raise PacketError("packet field out of range")
        body = struct.pack("<BBHH", PROTOCOL_VERSION, self.command, self.sequence, len(self.payload)) + self.payload
        return SOF + body + CRC.pack(binascii.crc32(body))

    @classmethod
    def decode(cls, data):
        if len(data) < HEADER.size + CRC.size:
            raise PacketError("truncated packet")
        sof, version, command, sequence, length = HEADER.unpack_from(data)
        expected = HEADER.size + length + CRC.size
        if sof != SOF or version != PROTOCOL_VERSION or length > MAX_PAYLOAD_SIZE or len(data) != expected:
            raise PacketError("invalid packet framing")
        expected_crc = CRC.unpack_from(data, HEADER.size + length)[0]
        if binascii.crc32(data[2:-4]) != expected_crc:
            raise PacketError("packet CRC mismatch")
        return cls(command, sequence, data[HEADER.size:-CRC.size])
