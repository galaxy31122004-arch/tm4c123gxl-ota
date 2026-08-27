# TM4C ESP-AT ThingsBoard RPC firmware

This CCS source runs at `0x00000000` and is link-limited to the Phase 1
bootloader region `0x00000000..0x00007FFF` (32 KB). It must not overlap Slot A,
which begins at `0x00008000`.

## Wiring

- COM7/UART0: PA0 RX, PA1 TX, 115200 8N1 diagnostic output.
- ESP-AT UART1: PB0 RX from ESP32 GPIO17 TX.
- ESP-AT UART1: PB1 TX to ESP32 GPIO16 RX.
- Common ground and 3.3 V logic.

## CCS sources

Use exactly one startup file and these application sources:

- `bridge_uart.c`
- `esp_at_controller.c`
- `esp_at_rpc.c`
- the existing `startup_ccs.c`

Use `bootloader_rpc.cmd` instead of the original `uart_echo_ccs.cmd`. Copy
`secrets.example.h` to `secrets.h` and enter the Wi-Fi SSID, password, and
ThingsBoard device token. Never commit `secrets.h`.

On reset, COM7 first prints `TM4C_ESP_AT_BOOTLOADER_RPC`. A successful session
prints `MQTT_RPC_READY` after TM4C publishes the initial `IDLE` telemetry to
ThingsBoard. Then send a `GET_INFO` server-side RPC to verify the reverse path.
