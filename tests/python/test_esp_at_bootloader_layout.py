from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def test_esp_at_linker_is_confined_to_phase1_bootloader_region():
    linker = (ROOT / "tm4c_uart_esp32_bridge" / "bootloader_rpc.cmd").read_text(
        encoding="ascii"
    )

    assert "origin = 0x00000000, length = 0x00008000" in linker
    assert "origin = 0x20000000, length = 0x00007FC0" in linker
    assert "origin = 0x20007FC0, length = 0x00000040" in linker
    assert ".intvecs : > 0x00000000" in linker
    assert "--stack_size=2048" in linker
    assert "__STACK_TOP = 0x20007FC0" in linker

    application = (ROOT / "tm4c_uart_esp32_bridge" / "bridge_uart.c").read_text(
        encoding="ascii"
    )
    assert "static esp_at_controller_t controller;" in application


def test_phase1_slot_a_still_starts_after_bootloader():
    config = (ROOT / "tm4c123gxl" / "common" / "inc" / "ota_config.h").read_text(
        encoding="ascii"
    )

    assert "#define OTA_BOOTLOADER_SIZE (UINT32_C(32) * OTA_KIBIBYTE)" in config
    assert "#define OTA_SLOT_A_START (OTA_BOOTLOADER_START + OTA_BOOTLOADER_SIZE)" in config
