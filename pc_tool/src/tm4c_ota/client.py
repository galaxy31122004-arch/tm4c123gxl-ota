import binascii
import struct
import sys

from .protocol import DATA, END_UPDATE, GET_INFO, RESET, START_UPDATE, Packet
from .transport import ProtocolNack

HEADER = struct.Struct("<IHBBHHHIII6s")
HEADER_PAGE_SIZE = 1024
MAGIC = 0x3141544F
SCHEMA_VERSION = 1
SLOT_PENDING = 3


class OtaClient:
    def __init__(self, transport):
        self.transport = transport
        self._sequence = 0

    def _request(self, command, payload=b""):
        packet = Packet(command, self._sequence, payload)
        self._sequence = (self._sequence + 1) & 0xFFFF
        return self.transport.request(packet)

    def get_info(self):
        payload = self._request(GET_INFO).payload[1:]
        if len(payload) < 24:
            raise ValueError("GET_INFO response is too short")
        values = struct.unpack_from("<6H6BII", payload)
        return {"bootloader_version": values[0:3], "slot_a_version": values[3:6], "slot_a_state": values[6], "slot_b_state": values[7], "active_slot": values[8], "pending_slot": values[9], "pending_boot_count": values[10], "protocol_version": values[11], "maximum_payload_size": values[12], "update_progress": values[13]}

    def update(self, image):
        if len(image) < HEADER_PAGE_SIZE:
            raise ValueError("image lacks a complete header page")
        magic, schema, target, reserved, major, minor, patch, size, payload_crc, header_crc, reserved_tail = HEADER.unpack(image[:32])
        crc_header = bytearray(image[:32])
        crc_header[22:26] = b"\0" * 4
        if (magic != MAGIC or schema != SCHEMA_VERSION or reserved != 0 or any(reserved_tail) or size == 0 or
                size > 110 * 1024 or len(image) != HEADER_PAGE_SIZE + size or header_crc != binascii.crc32(crc_header) or
                image[32:HEADER_PAGE_SIZE] != b"\xff" * (HEADER_PAGE_SIZE - 32) or
                payload_crc != binascii.crc32(image[HEADER_PAGE_SIZE:])):
            raise ValueError("invalid image header")
        msp, reset = struct.unpack_from("<II", image, HEADER_PAGE_SIZE)
        payload_start = 0x00008400 if target == 0 else 0x00024000
        if msp & 3 or not 0x20000000 <= msp <= 0x20008000 or not reset & 1 or not payload_start <= (reset & ~1) < payload_start + 110 * 1024:
            raise ValueError("invalid image vector table")
        info = self.get_info()
        if target == info["active_slot"]:
            raise ValueError("image targets the active slot")
        self._request(START_UPDATE, image[:32])
        payload = image[HEADER_PAGE_SIZE:]
        for offset in range(0, len(payload), 256):
            self._request(DATA, payload[offset:offset + 256])
            print(f"{min(offset + 256, len(payload))}/{len(payload)}", file=sys.stderr)
        self._request(END_UPDATE)
        updated = self.get_info()
        state = updated["slot_a_state"] if target == 0 else updated["slot_b_state"]
        if state != SLOT_PENDING:
            raise ValueError("target slot was not marked pending")

    def reset(self):
        self._request(RESET)


__all__ = ["OtaClient", "ProtocolNack"]
