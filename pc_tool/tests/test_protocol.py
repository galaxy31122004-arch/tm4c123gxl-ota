import binascii
import struct

import pytest

from tm4c_ota.client import OtaClient, ProtocolNack
from tm4c_ota.protocol import ACK, DATA, END_UPDATE, GET_INFO, NACK, Packet, START_UPDATE
from tm4c_ota.transport import RetryExhausted, SerialTransport


class FakeSerial:
    def __init__(self, responses=(), chunks=()):
        self.responses = list(responses)
        self.chunks = list(chunks)
        self.writes = []
        self.timeout = None

    def write(self, data):
        self.writes.append(bytes(data))
        if self.responses:
            self.chunks.extend(self.responses.pop(0))
        return len(data)

    def read(self, size=1):
        if not self.chunks:
            return b""
        chunk = self.chunks.pop(0)
        return chunk[:size]


def response(command, sequence, payload=b""):
    return Packet(ACK, sequence, bytes((command,)) + payload).encode()


def test_packet_golden_get_info_bytes():
    encoded = Packet(GET_INFO, 0x1234).encode()
    assert encoded == b"\x55\xaa\x01\x01\x34\x12\x00\x00" + binascii.crc32(b"\x01\x01\x34\x12\x00\x00").to_bytes(4, "little")
    assert Packet.decode(encoded) == Packet(GET_INFO, 0x1234)


def test_transport_recovers_from_noise_bad_crc_and_fragmented_reply():
    good = response(GET_INFO, 0)
    bad = bytearray(good)
    bad[-1] ^= 1
    serial = FakeSerial(responses=[[b"noise", bytes(bad), good[:3], good[3:]]])
    assert SerialTransport(serial).request(Packet(GET_INFO, 0)).command == ACK


def test_transport_defaults_and_semantic_nack_do_not_retry():
    nack = Packet(NACK, 0, bytes((GET_INFO, 0, 9))).encode()
    serial = FakeSerial(responses=[[nack]])
    transport = SerialTransport(serial)
    assert transport.timeout == 1.0
    assert transport.retries == 3
    with pytest.raises(ProtocolNack):
        transport.request(Packet(GET_INFO, 0))
    assert len(serial.writes) == 1


def test_transport_retries_timeouts_then_exhausts():
    serial = FakeSerial()
    with pytest.raises(RetryExhausted):
        SerialTransport(serial, timeout=0.0, retries=2).request(Packet(GET_INFO, 0))
    assert len(serial.writes) == 3


def test_client_info_and_reset():
    info = struct.pack("<9H6BII", 1, 0, 0, 1, 0, 0, 0, 0, 0, 2, 0, 0, 255, 0, 1, 112640, 0)
    serial = FakeSerial(responses=[[response(GET_INFO, 0, info)], [response(8, 1)]])
    client = OtaClient(SerialTransport(serial))
    assert client.get_info()["active_slot"] == 0
    client.reset()


def test_update_retries_lost_data_ack_with_same_packet():
    def info(slot_b_state=0):
        return struct.pack("<9H6BII", 1, 0, 0, 1, 0, 0, 0, 0, 0, 2, slot_b_state, 0, 255, 0, 1, 112640, 0)

    payload = struct.pack("<II", 0x20008000, 0x00024001) + b"payload"
    header = bytearray(struct.pack("<IHBBHHHIII6s", 0x3141544F, 1, 1, 0, 1, 2, 3, len(payload), binascii.crc32(payload), 0, b"\0" * 6))
    struct.pack_into("<I", header, 22, binascii.crc32(header))
    image = bytes(header) + b"\xff" * (1024 - len(header)) + payload
    serial = FakeSerial(responses=[
        [response(GET_INFO, 0, info())],
        [response(START_UPDATE, 1)],
        [],
        [response(DATA, 2)],
        [response(END_UPDATE, 3)],
        [response(GET_INFO, 4, info(3))],
    ])
    client = OtaClient(SerialTransport(serial, timeout=0.0, retries=3))
    client.update(image)
    data_writes = [Packet.decode(raw) for raw in serial.writes if Packet.decode(raw).command == DATA]
    assert data_writes == [Packet(DATA, 2, payload), Packet(DATA, 2, payload)]


def test_update_rejects_bad_header_crc_before_serial_write():
    image = b"\0" * 1024
    serial = FakeSerial()
    with pytest.raises(ValueError):
        OtaClient(SerialTransport(serial)).update(image)
    assert serial.writes == []
