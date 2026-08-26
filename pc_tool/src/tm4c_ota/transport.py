import time

from .protocol import ACK, NACK, HEADER, CRC, MAX_PAYLOAD_SIZE, Packet, PacketError, SOF


class RetryExhausted(TimeoutError):
    pass


class ProtocolNack(RuntimeError):
    def __init__(self, command, sequence, error):
        super().__init__(f"NACK command={command:#x} sequence={sequence} error={error}")
        self.command, self.sequence, self.error = command, sequence, error


class SerialTransport:
    def __init__(self, serial, timeout=1.0, retries=3, max_buffer=1024):
        self.serial = serial
        self.timeout = timeout
        self.retries = retries
        self.max_buffer = max_buffer
        self._buffer = bytearray()

    def _next_packet(self, deadline):
        while time.monotonic() <= deadline:
            marker = self._buffer.find(SOF)
            if marker < 0:
                self._buffer[:] = self._buffer[-1:]
            elif marker:
                del self._buffer[:marker]
            if len(self._buffer) >= HEADER.size:
                length = int.from_bytes(self._buffer[6:8], "little")
                if length > MAX_PAYLOAD_SIZE:
                    del self._buffer[0]
                    continue
                total = HEADER.size + length + CRC.size
                if len(self._buffer) >= total:
                    raw = bytes(self._buffer[:total])
                    del self._buffer[:total]
                    try:
                        return Packet.decode(raw)
                    except PacketError:
                        continue
            chunk = self.serial.read(max(1, self.max_buffer - len(self._buffer)))
            if chunk:
                self._buffer.extend(chunk)
                if len(self._buffer) > self.max_buffer:
                    del self._buffer[:-self.max_buffer]
            elif self.timeout == 0:
                break
        return None

    def request(self, packet):
        for _ in range(self.retries + 1):
            self.serial.write(packet.encode())
            reply = self._next_packet(time.monotonic() + self.timeout)
            if reply is None:
                continue
            if reply.command == NACK and reply.sequence == packet.sequence and len(reply.payload) >= 2 and reply.payload[0] == packet.command:
                raise ProtocolNack(packet.command, packet.sequence, reply.payload[1])
            if reply.command == ACK and reply.sequence == packet.sequence and reply.payload[:1] == bytes((packet.command,)):
                return reply
        raise RetryExhausted(f"no response after {self.retries + 1} attempts")
