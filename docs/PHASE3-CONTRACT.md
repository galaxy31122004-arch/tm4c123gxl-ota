# Phase 3 Cloud Firmware Contract

## Scope

Phase 3 connects the Phase 1 A/B bootloader to ThingsBoard Cloud through the
ESP32 ESP-AT v4.1.1.0 firmware. The Phase 1 flash layout, image format, update
protocol, rollback rules, and boot confirmation contract remain authoritative.

## Boot Flow

The bootloader opens a bounded cloud OTA service window after reset. A valid
`START_OTA` request starts the update flow; otherwise the bootloader jumps to a
valid application when the window expires. Every network operation has a finite
deadline and cannot indefinitely prevent application boot.

## ThingsBoard RPC

The accepted native RPC envelope is:

```json
{"method":"START_OTA","params":{"version":"1.0.1"}}
```

The version contains exactly three unsigned 16-bit decimal components. Firmware
title is fixed to `TM4C123GXL`. RPC input cannot override the host, access token,
download URL, destination slot, or flash address. Existing native and legacy
`GET_INFO` requests remain supported; no legacy `START_OTA` form is accepted.

## Firmware Download

The bootloader constructs this endpoint internally:

```text
https://thingsboard.cloud/api/v1/{device-token}/firmware?title=TM4C123GXL&version={version}
```

The token comes only from an ignored `secrets.h` and must never appear in logs.
The installed ESP-AT firmware provides `HTTPGETSIZE`, `HTTPCGET`, and
`HTTPURLCFG`. HTTPS transport is used now; certificate provisioning and stricter
certificate validation are deferred and must be tracked before production use.

## Image Stream

The downloaded object is the Phase 1 packaged image: a 32-byte
`ota_firmware_header_t` followed by the application payload. The reported remote
size must equal `32 + payload_size`. The header is validated before erase, and
payload data is passed directly to `bl_update_handle` in chunks no larger than
256 bytes. `+HTTPCGET:<size>,` switches the parser into exact-length binary mode;
binary bytes are never parsed as lines and the complete image is never buffered
in RAM.

## State, Retry, And Rollback

Progress and terminal state use the existing Phase 2 telemetry contract.
`SUCCESS` is reported only after the new application confirms boot. Download and
update failures use finite retries (maximum three), restart from byte zero, leave
the previous active slot bootable, and never mark an invalid candidate active.

## Secrets And Tests

Live credentials are excluded from source control. Host tests use dummy values.
Hardware-only acceptance items remain pending until their board observations are
recorded in the Phase 3 results document.
