"""ThingsBoard MQTT contract shared by the ESP32 bridge and simulator."""

from __future__ import annotations

from datetime import date
from typing import Any, Mapping

DEVICE_NAME = "TM4C123GXL-OTA-01"
TELEMETRY_TOPIC = "v1/devices/me/telemetry"
ATTRIBUTES_TOPIC = "v1/devices/me/attributes"
RPC_REQUEST_TOPIC = "v1/devices/me/rpc/request/{request_id}"
RPC_RESPONSE_TOPIC = "v1/devices/me/rpc/response/{request_id}"

OTA_STATES = (
    "IDLE",
    "CHECKING",
    "DOWNLOADING",
    "DOWNLOADED",
    "TRANSFERRING",
    "VERIFYING",
    "REBOOTING",
    "CONFIRMING",
    "SUCCESS",
    "ROLLBACK",
    "ERROR",
)

ERROR_CODES = {
    0x00: "SUCCESS",
    0x01: "DOWNLOAD_ERROR",
    0x02: "DOWNLOAD_CRC_ERROR",
    0x03: "TM4C_TIMEOUT",
    0x04: "TM4C_NACK",
    0x05: "FIRMWARE_TOO_LARGE",
    0x06: "FIRMWARE_INVALID",
    0x07: "VERIFY_FAILED",
    0x08: "CONFIRM_TIMEOUT",
    0x09: "ROLLBACK",
}

_TRANSITIONS = {
    "IDLE": {"CHECKING"},
    "CHECKING": {"DOWNLOADING", "ERROR"},
    "DOWNLOADING": {"DOWNLOADED", "ERROR"},
    "DOWNLOADED": {"TRANSFERRING", "ERROR"},
    "TRANSFERRING": {"VERIFYING", "ERROR"},
    "VERIFYING": {"REBOOTING", "SUCCESS", "ERROR"},
    "REBOOTING": {"CONFIRMING", "ROLLBACK", "ERROR"},
    "CONFIRMING": {"SUCCESS", "ROLLBACK", "ERROR"},
    "SUCCESS": {"IDLE"},
    "ROLLBACK": {"IDLE"},
    "ERROR": {"IDLE"},
}


class ContractError(ValueError):
    """Raised when a payload violates the Phase 2 contract."""


def _require(payload: Mapping[str, Any], key: str, expected: type | tuple[type, ...]) -> Any:
    if key not in payload or not isinstance(payload[key], expected):
        raise ContractError(f"{key} must be {expected}")
    return payload[key]


def validate_telemetry(payload: Mapping[str, Any]) -> None:
    """Validate a complete or partial ThingsBoard telemetry payload."""
    if not isinstance(payload, Mapping):
        raise ContractError("telemetry must be an object")
    if "ota_state" in payload:
        state = _require(payload, "ota_state", str)
        if state not in OTA_STATES:
            raise ContractError(f"unknown ota_state: {state}")
    if "ota_progress" in payload:
        progress = _require(payload, "ota_progress", int)
        if not 0 <= progress <= 100:
            raise ContractError("ota_progress must be between 0 and 100")
    if "active_slot" in payload:
        slot = _require(payload, "active_slot", str)
        if slot not in {"A", "B"}:
            raise ContractError("active_slot must be A or B")
    for key in ("app_version", "bootloader_version", "last_update"):
        if key in payload:
            _require(payload, key, str)
    if "device_uptime" in payload:
        uptime = _require(payload, "device_uptime", int)
        if uptime < 0:
            raise ContractError("device_uptime must be non-negative")
    if "ota_error" in payload:
        error = _require(payload, "ota_error", int)
        if error not in ERROR_CODES:
            raise ContractError(f"unknown ota_error: {error}")


def validate_firmware_metadata(payload: Mapping[str, Any]) -> None:
    required = ("firmware_version", "firmware_size", "firmware_checksum", "firmware_file", "release_date")
    for key in required:
        _require(payload, key, str if key != "firmware_size" else int)
    if payload["firmware_size"] <= 0:
        raise ContractError("firmware_size must be positive")
    try:
        date.fromisoformat(payload["release_date"])
    except ValueError as exc:
        raise ContractError("release_date must be YYYY-MM-DD") from exc


def validate_transition(current: str, next_state: str) -> None:
    if current not in OTA_STATES or next_state not in OTA_STATES:
        raise ContractError("state is not in OTA_STATES")
    if next_state not in _TRANSITIONS[current]:
        raise ContractError(f"invalid OTA transition: {current} -> {next_state}")


def build_telemetry(**values: Any) -> dict[str, Any]:
    """Build and validate a telemetry object from named contract fields."""
    validate_telemetry(values)
    return dict(values)
