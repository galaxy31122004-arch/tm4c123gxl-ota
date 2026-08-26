import binascii
import pathlib
import struct
import subprocess
import sys


ROOT = pathlib.Path(__file__).parents[2]
PACKAGER = ROOT / "scripts" / "package_image.py"


def test_packager_creates_valid_header_page(tmp_path):
    payload = struct.pack("<II", 0x20008000, 0x00008401) + b"payload"
    source = tmp_path / "app.bin"
    output = tmp_path / "firmware.bin"
    source.write_bytes(payload)

    subprocess.run([sys.executable, str(PACKAGER), "--slot", "A", "--version", "1.2.3", "--input", str(source), "--output", str(output)], check=True)

    image = output.read_bytes()
    header = bytearray(image[:32])
    assert len(image) == 1024 + len(payload)
    assert image[32:1024] == b"\xff" * (1024 - 32)
    assert struct.unpack_from("<I", header, 14)[0] == len(payload)
    assert struct.unpack_from("<I", header, 18)[0] == binascii.crc32(payload)
    expected_crc = struct.unpack_from("<I", header, 22)[0]
    header[22:26] = b"\0" * 4
    assert binascii.crc32(header) == expected_crc
