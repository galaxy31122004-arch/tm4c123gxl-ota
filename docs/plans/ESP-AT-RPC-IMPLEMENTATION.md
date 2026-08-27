# ESP-AT ThingsBoard RPC Implementation Plan

> Historical execution plan. Current ordering and phase status are maintained
> in `docs/OTA-PLAN.md`. Tasks 1-3 and hardware deployment/build/startup are
> complete; the real ThingsBoard `GET_INFO` round-trip and its evidence commit
> remain pending.

> **For agentic workers:** REQUIRED SUB-SKILL: Use axon:subagent-driven-development (recommended) or axon:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make TM4C123GXL use ESP-AT to connect to ThingsBoard Cloud and complete a real `GET_INFO` RPC round trip.

**Architecture:** Keep protocol parsing and the AT/MQTT state machine in portable C with injected UART/time callbacks. A thin TivaWare application binds the controller to UART1 and sends redacted diagnostics to UART0. The tracked source is deployed into the existing `D:\TM4AC\uart_echo_clone` CCS project after host tests pass.

**Tech Stack:** C11 host tests with CMake/CTest, TI ARM Clang 5.1.1.LTS, TivaWare 2.2.0.295, ESP-AT v4.1.1.0, ThingsBoard MQTT.

## Global Constraints

- MQTT endpoint is exactly `thingsboard.cloud:1883`; TLS is deferred.
- UART0 and UART1 use 115200 baud, 8 data bits, no parity, one stop bit.
- Secrets are never committed or printed.
- Only `GET_INFO` is implemented in this milestone.
- No dynamic allocation is used on the TM4C.
- Existing Phase 1 bootloader source and binaries are not modified.

---

### Task 1: Portable ESP-AT RPC Parser

**Files:**
- Create: `tm4c_uart_esp32_bridge/esp_at_rpc.h`
- Create: `tm4c_uart_esp32_bridge/esp_at_rpc.c`
- Create: `tests/c/test_esp_at_rpc.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: one complete ESP-AT text line.
- Produces: `esp_at_rpc_parse(const char *, esp_at_rpc_request_t *)` and `esp_at_rpc_build_response(const esp_at_rpc_request_t *, char *, size_t)`.

- [ ] **Step 1: Write failing parser tests**

Cover extraction of request ID `42`, native `{"method":"GET_INFO"}`, legacy
`{"command":"GET_INFO"}`, malformed lengths, oversized IDs, and unsupported
methods. Assert the response topic and exact JSON body.

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cmake --build build-audit-mingw --target esp_at_rpc && ctest --test-dir build-audit-mingw -R esp_at_rpc --output-on-failure`

Expected: build failure because `esp_at_rpc.h` does not exist.

- [ ] **Step 3: Implement the bounded parser**

Define fixed limits for topic, request ID, method, and payload. Parse the
comma-delimited ESP-AT prefix without modifying string literals, validate the
declared payload length, accept only `GET_INFO`, and generate:

```text
topic: v1/devices/me/rpc/response/<request_id>
body:  {"app_version":"1.0.0","bootloader_version":"1.0.0","active_slot":"A"}
```

- [ ] **Step 4: Register and run the test**

Add an `esp_at_rpc` static library and test executable to `CMakeLists.txt` with
strict warnings. Run the focused command again.

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Commit the parser**

```powershell
git add CMakeLists.txt tests/c/test_esp_at_rpc.c tm4c_uart_esp32_bridge/esp_at_rpc.c tm4c_uart_esp32_bridge/esp_at_rpc.h
git commit -m "feat: parse ThingsBoard RPC from ESP-AT"
```

### Task 2: ESP-AT MQTT Controller

**Files:**
- Create: `tm4c_uart_esp32_bridge/esp_at_controller.h`
- Create: `tm4c_uart_esp32_bridge/esp_at_controller.c`
- Create: `tests/c/test_esp_at_controller.c`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `esp_at_controller_init`, received UART bytes, and monotonic ticks.
- Produces: AT commands through an injected transmit callback and status through an injected log callback.
- Uses: `esp_at_rpc_parse` and `esp_at_rpc_build_response` from Task 1.

- [ ] **Step 1: Write failing state-machine tests**

Use fake transmit, clock, and log callbacks. Assert the exact ordered commands
for synchronization, echo disable, station mode, AP join, MQTT configuration,
broker connection, subscription, and RPC response publication. Assert timeout
retry and that logs never contain SSID password or token.

- [ ] **Step 2: Run the focused test and verify failure**

Run: `cmake --build build-audit-mingw --target esp_at_controller && ctest --test-dir build-audit-mingw -R esp_at_controller --output-on-failure`

Expected: build failure because `esp_at_controller.h` does not exist.

- [ ] **Step 3: Implement the non-blocking controller**

Implement explicit states `SYNC`, `ECHO_OFF`, `WIFI_MODE`, `WIFI_JOIN`,
`MQTT_CONFIG`, `MQTT_CONNECT`, `MQTT_SUBSCRIBE`, `ONLINE`, and `RETRY`. Feed
received bytes into a bounded line buffer, advance only on expected `OK` or
connection indications, and publish a parsed `GET_INFO` response with
`AT+MQTTPUB`.

- [ ] **Step 4: Register and run controller tests**

Link the controller test to `esp_at_rpc`, enable strict warnings, and run the
focused test command.

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 5: Commit the controller**

```powershell
git add CMakeLists.txt tests/c/test_esp_at_controller.c tm4c_uart_esp32_bridge/esp_at_controller.c tm4c_uart_esp32_bridge/esp_at_controller.h
git commit -m "feat: control ESP-AT ThingsBoard MQTT session"
```

### Task 3: TivaWare Application and Secret Handling

**Files:**
- Replace: `tm4c_uart_esp32_bridge/bridge_uart.c`
- Create: `tm4c_uart_esp32_bridge/secrets.example.h`
- Modify: `.gitignore`
- Modify: `tm4c_uart_esp32_bridge/README.md`

**Interfaces:**
- Consumes: controller callbacks from Task 2 and macros `WIFI_SSID`, `WIFI_PASSWORD`, `THINGSBOARD_TOKEN`.
- Produces: CCS application entry point using UART0 diagnostics and UART1 ESP-AT transport.

- [ ] **Step 1: Add the tracked configuration template**

Create `secrets.example.h` with empty string macros and ignore only
`tm4c_uart_esp32_bridge/secrets.h`. Document that the user creates the local
header and keeps the access token out of Git.

- [ ] **Step 2: Bind the controller to TivaWare**

Keep the proven PA0/PA1 and PB0/PB1 setup. Replace transparent forwarding with
a polling loop that feeds UART1 bytes to the controller, calls its tick
function, transmits commands on UART1, and writes redacted state messages to
UART0. Retain empty `UARTIntHandler` and `ADC0SS1IntHandler` definitions because
the existing startup table references them.

- [ ] **Step 3: Run all host tests**

Run: `cmake --build build-audit-mingw && ctest --test-dir build-audit-mingw --output-on-failure`

Expected: all prior 11 tests plus `esp_at_rpc` and `esp_at_controller` pass.

- [ ] **Step 4: Commit the application source**

```powershell
git add .gitignore tm4c_uart_esp32_bridge
git commit -m "feat: add TM4C ESP-AT MQTT application"
```

### Task 4: CCS Deployment and Hardware Acceptance

**Files:**
- Copy into: `D:\TM4AC\uart_echo_clone\bridge_uart.c`
- Copy into: `D:\TM4AC\uart_echo_clone\esp_at_controller.c`
- Copy into: `D:\TM4AC\uart_echo_clone\esp_at_controller.h`
- Copy into: `D:\TM4AC\uart_echo_clone\esp_at_rpc.c`
- Copy into: `D:\TM4AC\uart_echo_clone\esp_at_rpc.h`
- Create locally: `D:\TM4AC\uart_echo_clone\secrets.h`
- Modify: `docs/PHASE2-CONTRACT.md`
- Modify: `docs/PHASE2-RESULTS.md`

**Interfaces:**
- Consumes: the tested portable modules and the user's local credentials.
- Produces: `uart_echo_clone.out`, `uart_echo_clone.bin`, and cloud RPC evidence.

- [ ] **Step 1: Deploy only required source files**

Copy the five tracked source/header files into the CCS project. Create
`secrets.h` locally with the actual credentials. Confirm CCS includes exactly
one `main`, one startup source, the three application `.c` files, the existing
linker command file, and the TivaWare driver library.

- [ ] **Step 2: Build the CCS project**

Run the existing CCS Debug build. Expected output ends with:

```text
Finished building target: "uart_echo_clone.out"
```

- [ ] **Step 3: Flash and inspect COM7**

Flash through ICDI, reset both boards, and capture COM7 at 115200 8N1. Expected
states are Wi-Fi connected, MQTT connected, and RPC subscribed; no credential
value may appear.

- [ ] **Step 4: Verify real ThingsBoard RPC**

Send `GET_INFO` from the ThingsBoard RPC widget. Expected response contains
`app_version=1.0.0`, `bootloader_version=1.0.0`, and `active_slot=A`, with the
same request ID used in the response topic.

- [ ] **Step 5: Record evidence and commit docs**

Update the Phase 2 contract to use native `method`/`params`, retain documented
legacy compatibility, and mark RPC round-trip PASS only after the hardware
response is observed.

```powershell
git add docs/PHASE2-CONTRACT.md docs/PHASE2-RESULTS.md
git commit -m "docs: verify ThingsBoard RPC round trip"
```
