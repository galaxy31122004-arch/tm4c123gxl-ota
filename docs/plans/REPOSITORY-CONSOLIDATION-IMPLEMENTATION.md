# Repository Consolidation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use axon:subagent-driven-development (recommended) or axon:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate the tested ESP-AT firmware, make `docs/OTA-PLAN.md` the single roadmap, record completed physical fault tests, verify the repository, and push the consolidated result.

**Architecture:** Keep the Phase 1 bootloader and its 32 KiB boundary unchanged. Add the ESP-AT/ThingsBoard adapter as a separate bootloader-limited firmware, while `docs/OTA-PLAN.md` owns phase order and result documents contain evidence only.

**Tech Stack:** TM4C123GH6PM, TI ARM Clang 5.1.1 LTS, TivaWare 2.2.0.295, CMake/CTest, Python/pytest, ESP-AT 4.1.1.0, MQTT, ThingsBoard Cloud, Git/GitHub.

## Global Constraints

- Never commit `secrets.h`, Wi-Fi credentials, ThingsBoard tokens, generated binaries, NVS backups, or serial probe output.
- Preserve all local ESP32 NVS and firmware backup files on disk.
- Keep the ESP-AT firmware inside Flash `0x00000000..0x00007FFF`; Slot A starts at `0x00008000`.
- Mark cloud RPC PASS only after a real ThingsBoard request receives a response with the matching request ID.
- Do not rewrite or force-push published `main` history.

---

### Task 1: Preserve Local Artifacts and Integrate ESP-AT

**Files:**
- Modify: `.gitignore`
- Merge: branch `task/esp-at-rpc`

**Interfaces:**
- Consumes: tested commits `78c0c13..61bc3a0`
- Produces: ESP-AT controller, parser, linker, tests, and credential template on `main`

- [ ] **Step 1: Compare conflicting untracked files**

Run `git status --short` and compare every untracked path under `tm4c_uart_esp32_bridge/` with `task/esp-at-rpc`. Move only conflicting untracked copies to `artifacts/pre-merge/`; do not delete them.

- [ ] **Step 2: Extend ignore rules**

Ignore `.tmp_*`, `build-*`, `*.egg-info/`, serial probe scripts, ESP32/NVS backup artifacts, `tm4c_uart_esp32_bridge/secrets.h`, `build-ti/`, and linker maps while keeping source, tests, and templates tracked.

- [ ] **Step 3: Rebase the unpublished task branch**

Run `git -C .worktrees/esp-at-rpc rebase main`. Expected: clean rebase because the branch and roadmap-spec commit touch different files.

- [ ] **Step 4: Fast-forward main**

Run `git merge --ff-only task/esp-at-rpc`. Expected: all five ESP-AT commits land on `main` without conflicts.

### Task 2: Establish One Canonical Roadmap

**Files:**
- Modify: `docs/OTA-PLAN.md`
- Modify: `docs/PHASE1-RESULTS.md`
- Modify: `docs/PHASE2-PLAN.md`
- Modify: `docs/PHASE2-RESULTS.md`
- Modify: `docs/plans/ESP-AT-RPC-IMPLEMENTATION.md`

**Interfaces:**
- Consumes: user-confirmed physical power-loss and cable-disconnection rollback results
- Produces: one ordered Phase 1 -> Phase 2 -> Phase 3 execution path

- [ ] **Step 1: Add canonical status to OTA-PLAN**

At the top of `docs/OTA-PLAN.md`, define Phase 1 as PASS, Phase 2 telemetry as PASS with RPC as its only remaining gate, and Phase 3 as cloud firmware download plus UART OTA end-to-end.

- [ ] **Step 2: Record physical fault evidence**

Replace the two unverified fault statements in `docs/PHASE1-RESULTS.md` with the user's observed result: both physical interruption cases rolled back to the previous active slot.

- [ ] **Step 3: Remove competing status ownership**

Add a canonical-roadmap notice to Phase 2 and historical implementation-plan documents. Keep contract and evidence content, but point current ordering/status to `docs/OTA-PLAN.md`.

- [ ] **Step 4: Fix documentation defects**

Remove the stray build-log line/code fence in `docs/PHASE1-RESULTS.md` and replace mojibake text in `docs/PHASE2-PLAN.md` with ASCII-compatible Vietnamese or English.

### Task 3: Verify Both Repository and Firmware

**Files:**
- Verify: `CMakeLists.txt`
- Verify: `tests/c/`
- Verify: `tests/python/`
- Verify: `tm4c_uart_esp32_bridge/bootloader_rpc.cmd`

**Interfaces:**
- Consumes: consolidated `main`
- Produces: fresh host, firmware-build, memory-layout, and hardware evidence

- [ ] **Step 1: Run C tests**

Configure/build with CMake and run `ctest --output-on-failure`. Expected: 13/13 PASS including ESP-AT parser/controller.

- [ ] **Step 2: Run Python tests**

Run `.venv\Scripts\python.exe -m pytest tests/python pc_tool/tests -q`. Expected: all tests PASS, including the 32 KiB linker assertions.

- [ ] **Step 3: Build TI firmware**

Compile and link `bridge_uart.c`, `esp_at_controller.c`, `esp_at_rpc.c`, and `startup_ccs.c` with TI ARM Clang 5.1.1 LTS and TivaWare 2.2.0.295. Expected: `.out` produced with no linker errors.

- [ ] **Step 4: Verify memory budget**

Read the linker map and require Flash usage below `0x8000`, SRAM below `0x7FC0`, vectors at `0x00000000`, and no section overlap with Slot A.

- [ ] **Step 5: Retain hardware evidence**

Confirm the latest COM7 capture contains `UART0_READY`, `UART1_READY`, `TM4C_ESP_AT_BOOTLOADER_RPC`, and `MQTT_RPC_READY` without credential values.

### Task 4: Close the ThingsBoard RPC Gate

**Files:**
- Modify after verification: `docs/OTA-PLAN.md`
- Modify after verification: `docs/PHASE2-RESULTS.md`

**Interfaces:**
- Consumes: ThingsBoard `GET_INFO` server-side RPC
- Produces: matching response containing app version, bootloader version, and active slot

- [ ] **Step 1: Send a real GET_INFO request**

Use the existing ThingsBoard dashboard/debug RPC control to send `{"method":"GET_INFO","params":{}}` to the connected device.

- [ ] **Step 2: Verify device and cloud evidence**

Require COM7 to report `RPC_GET_INFO_RESPONSE_SENT` and ThingsBoard to display the response for the same request ID. Do not accept simulator-only evidence.

- [ ] **Step 3: Update status**

If both observations pass, mark the Phase 2 RPC gate and Phase 2 overall status PASS. Otherwise retain PENDING and record the exact blocker.

### Task 5: Final Repository Gate and Push

**Files:**
- Verify: entire repository

**Interfaces:**
- Consumes: completed integration and evidence
- Produces: clean, reproducible `origin/main`

- [ ] **Step 1: Inspect staged scope**

Run `git status --short`, `git diff --check`, and a credential-pattern scan. Expected: no secret or generated hardware artifact is tracked.

- [ ] **Step 2: Commit documentation and ignore rules**

Commit only the canonical roadmap, result corrections, reference notices, and `.gitignore` changes.

- [ ] **Step 3: Re-run verification after the final commit**

Repeat CTest, pytest, and `git diff --check`. Expected: all commands exit zero.

- [ ] **Step 4: Push main**

Run `git push origin main`, then verify `origin/main` resolves to the local `main` commit.
