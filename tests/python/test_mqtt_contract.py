import pytest

from tm4c_ota.mqtt_contract import (
    ContractError,
    OTA_STATES,
    build_telemetry,
    validate_firmware_metadata,
    validate_telemetry,
    validate_transition,
)


def test_valid_telemetry_and_firmware_metadata_are_accepted():
    telemetry = build_telemetry(
        ota_state="TRANSFERRING",
        ota_progress=60,
        app_version="1.0.0",
        bootloader_version="1.0.0",
        active_slot="A",
        device_uptime=123,
        last_update="2026-08-27T00:00:00Z",
        ota_error=0,
    )
    validate_telemetry(telemetry)
    validate_firmware_metadata(
        {
            "firmware_version": "1.0.1",
            "firmware_size": 376,
            "firmware_checksum": "0x1234abcd",
            "firmware_file": "tm4c123gxl_v1.0.1.bin",
            "release_date": "2026-08-27",
        }
    )


def test_invalid_progress_and_slot_are_rejected():
    with pytest.raises(ContractError):
        validate_telemetry({"ota_state": "TRANSFERRING", "ota_progress": 101})
    with pytest.raises(ContractError):
        validate_telemetry({"ota_state": "IDLE", "active_slot": "C"})


def test_state_machine_rejects_skipping_transfer_and_accepts_success_flow():
    assert OTA_STATES[0] == "IDLE"
    validate_transition("IDLE", "CHECKING")
    validate_transition("CHECKING", "DOWNLOADING")
    validate_transition("DOWNLOADING", "DOWNLOADED")
    validate_transition("DOWNLOADED", "TRANSFERRING")
    validate_transition("TRANSFERRING", "VERIFYING")
    validate_transition("VERIFYING", "SUCCESS")
    with pytest.raises(ContractError):
        validate_transition("IDLE", "SUCCESS")
