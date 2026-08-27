# TM4C123GXL OTA

Phase 1 implements an A/B OTA bootloader for TM4C123GH6PM using CCS and TivaWare. A Python PC tool emulates the future ESP32 transport over UART1.

- Design: `docs/PHASE1-SPEC.md`
- Protocol: `docs/PROTOCOL.md`
- Board test: `docs/HARDWARE-TEST.md`
- Roadmap: `docs/OTA-PLAN.md`
- Phase 2 contract: `docs/PHASE2-CONTRACT.md`
- Phase 2 dashboard template: `docs/thingsboard/dashboard.json`

Run the Phase 2 MQTT simulator without a broker:

```powershell
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario success
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario error
& .\.venv\Scripts\python.exe scripts/mqtt_simulator.py --scenario rollback
```
OTA firmware update system for TM4C123GXL using ESP32 ESP-AT and ThingsBoard
