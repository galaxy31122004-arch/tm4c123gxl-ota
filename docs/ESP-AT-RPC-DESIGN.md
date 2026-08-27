# ESP-AT ThingsBoard RPC Design

## Scope

This milestone proves a real round trip from ThingsBoard Cloud through an
ESP32 running ESP-AT to the TM4C123GXL and back. It implements `GET_INFO` only.
Firmware download, flash transfer, slot selection, TLS, and the remaining RPC
methods are outside this milestone.

## Runtime Architecture

```text
ThingsBoard Cloud (thingsboard.cloud:1883)
        |
        | MQTT
        v
ESP32 ESP-AT v4.1.1.0
        |
        | UART1, 115200 8N1
        v
TM4C123GXL
        |
        | UART0 diagnostic log
        v
COM7
```

The TM4C is the ESP-AT host. UART0 remains a diagnostic console; it is no
longer a transparent byte bridge while the MQTT controller is running.

## Configuration

The broker is `thingsboard.cloud` on plain MQTT port `1883`. The ThingsBoard
device access token is the MQTT username. MQTT password is empty.

`secrets.h` contains the Wi-Fi SSID, Wi-Fi password, and ThingsBoard token.
Only `secrets.example.h` is tracked. The real `secrets.h` is ignored by Git.

## Startup State Machine

The controller performs these states in order:

1. Synchronize with ESP-AT using `AT` and disable command echo using `ATE0`.
2. Set station mode and join the configured access point.
3. Configure the ESP-AT MQTT client with the ThingsBoard token as username.
4. Connect to `thingsboard.cloud:1883`.
5. Subscribe to `v1/devices/me/rpc/request/+`.
6. Enter the online state and process unsolicited MQTT receive records.

Every command has a bounded timeout. An `ERROR`, timeout, Wi-Fi disconnect, or
MQTT disconnect moves the controller to a retry state and prints a short error
on COM7. No secret value is printed.

## RPC Contract

ThingsBoard server-side RPC requests use:

```json
{"method":"GET_INFO","params":{}}
```

For compatibility with the existing Phase 2 draft, the parser also accepts:

```json
{"command":"GET_INFO"}
```

The ESP-AT receive record contains the request ID in its topic:

```text
+MQTTSUBRECV:0,"v1/devices/me/rpc/request/42",<length>,<payload>
```

The response is published to `v1/devices/me/rpc/response/42`:

```json
{"app_version":"1.0.0","bootloader_version":"1.0.0","active_slot":"A"}
```

Malformed records and unsupported methods are ignored after a diagnostic log.
Input, topic, payload, and command buffers have fixed compile-time limits.

## Test Boundary

Parsing, state transitions, command generation, request ID extraction, and
response generation are implemented in portable C and covered by host tests.
The CCS build then verifies the TivaWare UART adapter. Final acceptance is a
real `GET_INFO` request from ThingsBoard with a matching response visible in
the dashboard or RPC debug view.

## Architecture Constraint

ESP-AT is a modem controlled by the TM4C; it does not independently execute the
Phase 1 UART bootloader protocol. This milestone is valid for cloud RPC. Before
real firmware transfer, the project must choose either TM4C-managed download and
self-programming or custom ESP32 firmware that drives the existing bootloader.
