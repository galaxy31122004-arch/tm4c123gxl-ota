#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "inc/hw_memmap.h"

#include "esp_at_controller.h"
#include "secrets.h"

#define UART_BAUD_RATE 115200U
#define MQTT_HOST "thingsboard.cloud"
#define MQTT_PORT 1883U
#define MQTT_CLIENT_ID "TM4C123GXL"

static void uart_puts(uint32_t base, const char *text)
{
    while (*text != '\0') {
        UARTCharPut(base, (unsigned char)*text++);
    }
}

static void esp_tx(const char *data, size_t length, void *context)
{
    size_t index;
    (void)context;

    for (index = 0U; index < length; ++index) {
        UARTCharPut(UART1_BASE, (unsigned char)data[index]);
    }
}

static void debug_log(const char *message, void *context)
{
    (void)context;
    uart_puts(UART0_BASE, message);
    uart_puts(UART0_BASE, "\r\n");
}

static void uart0_init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0)) {
    }
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, SysCtlClockGet(), UART_BAUD_RATE,
                       UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                           UART_CONFIG_PAR_NONE);
    UARTEnable(UART0_BASE);
}

static void uart1_init(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART1)) {
    }
    GPIOPinConfigure(GPIO_PB0_U1RX);
    GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART1_BASE, SysCtlClockGet(), UART_BAUD_RATE,
                       UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                           UART_CONFIG_PAR_NONE);
    UARTEnable(UART1_BASE);
}

int main(void)
{
    static esp_at_controller_t controller;
    static const esp_at_controller_config_t config = {
        WIFI_SSID,
        WIFI_PASSWORD,
        THINGSBOARD_TOKEN,
        MQTT_HOST,
        MQTT_PORT,
        MQTT_CLIENT_ID,
    };
    uint32_t now_ms = 0U;

    SysCtlClockSet(SYSCTL_SYSDIV_5 | SYSCTL_USE_PLL |
                   SYSCTL_XTAL_16MHZ | SYSCTL_OSC_MAIN);
    uart0_init();
    uart1_init();
    debug_log("TM4C_ESP_AT_BOOTLOADER_RPC", NULL);

    if (WIFI_SSID[0] == '\0' || THINGSBOARD_TOKEN[0] == '\0') {
        debug_log("CONFIG_MISSING", NULL);
        while (1) {
        }
    }

    esp_at_controller_init(&controller, &config, esp_tx, debug_log, NULL,
                           now_ms);
    while (1) {
        while (UARTCharsAvail(UART1_BASE)) {
            int32_t received = UARTCharGetNonBlocking(UART1_BASE);
            if (received >= 0) {
                esp_at_controller_receive(&controller,
                                          (unsigned char)received, now_ms);
            }
        }
        esp_at_controller_tick(&controller, now_ms);
        SysCtlDelay(SysCtlClockGet() / 3000U);
        ++now_ms;
    }
}

void UARTIntHandler(void)
{
}

void ADC0SS1IntHandler(void)
{
}
