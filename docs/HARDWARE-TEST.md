# TM4C123GXL Phase 1 Hardware Test

## Wiring

- Power/program through the LaunchPad `DEBUG` USB connector.
- USB-UART TX to PB0/U1RX; RX to PB1/U1TX; GND to GND.
- Do not connect the adapter power pin.
- UART0 debug output is available through the ICDI virtual COM port.

## Build and Tool

```powershell
powershell -ExecutionPolicy Bypass -File scripts/build_ccs.ps1 -TivaWareRoot C:\ti\TivaWare_C_Series-2.2.0.295
py -3.10 -m venv .venv
.\.venv\Scripts\python.exe -m pip install -e ".\pc_tool[test]"
.\.venv\Scripts\tm4c-ota.exe --port COM5 info
.\.venv\Scripts\tm4c-ota.exe --port COM5 update artifacts\slot_b_v1.0.1.bin
.\.venv\Scripts\tm4c-ota.exe --port COM5 reset
```

Replace COM5 with the external USB-UART port. Import the bootloader and both application projects in CCS for ICDI flashing. Map files must place vectors at `0x00000000`, `0x00008400`, and `0x00024000`.

## Acceptance

1. Flash bootloader and confirming Slot A v1.0.0.
2. Require GET_INFO to report A active.
3. OTA confirming Slot B v1.0.1, reset, and require B active/A backup.
4. OTA a Slot A build with `NO_CONFIRM=1`; require rollback to B after three attempts.
5. Disconnect UART after START_UPDATE and midway through DATA; prior active image must boot.
6. Remove power during erase, DATA, and metadata commit; prior active image must remain bootable.

Preserve UART0 and PC-tool logs under `artifacts/hardware/`.
