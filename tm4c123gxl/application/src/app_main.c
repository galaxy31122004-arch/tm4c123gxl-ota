#include "boot_confirm.h"
#include "ota_request.h"
#include "app_ota_handoff.h"
#ifndef APP_VERSION_MAJOR
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0
#endif

#if defined(APP_ENABLE_CLOUD_HANDOFF) && defined(__TI_ARM__)
#include <stdbool.h>
#include <stddef.h>

#include "driverlib/gpio.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "esp_at_controller.h"
#include "phase3_config.h"

static uint32_t g_app_millis;
static app_ota_handoff_t g_handoff;

static void app_tx(const char *data, size_t length, void *context)
{
    size_t index;
    (void)context;
    for (index = 0U; index < length; ++index) {
        UARTCharPut(UART1_BASE, (unsigned char)data[index]);
    }
}

static void app_log(const char *message, void *context)
{
    (void)context;
    while (*message != '\0') UARTCharPut(UART0_BASE, *message++);
    UARTCharPut(UART0_BASE, '\r'); UARTCharPut(UART0_BASE, '\n');
}

static int app_start_ota(const esp_at_rpc_version_t *version, void *context)
{
    (void)context;
    return app_ota_handoff_accept(&g_handoff, version, g_app_millis);
}

static void app_cloud_init(void)
{
    uint32_t clock;
    SysCtlClockSet(SYSCTL_SYSDIV_2_5 | SYSCTL_USE_PLL | SYSCTL_OSC_MAIN |
                   SYSCTL_XTAL_16MHZ);
    clock = SysCtlClockGet();
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0) ||
           !SysCtlPeripheralReady(SYSCTL_PERIPH_UART1)) { }
    GPIOPinConfigure(GPIO_PA0_U0RX); GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinConfigure(GPIO_PB0_U1RX); GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, clock, 115200U,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                        UART_CONFIG_PAR_NONE);
    UARTConfigSetExpClk(UART1_BASE, clock, 115200U,
                        UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                        UART_CONFIG_PAR_NONE);
    UARTEnable(UART0_BASE); UARTEnable(UART1_BASE);
}
#endif

int main(void) {
#if !defined(NO_CONFIRM)
    ota_version_t version = {APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH};
    boot_confirm((ota_slot_t)OTA_APP_SLOT, version);
#endif
#if defined(APP_ENABLE_CLOUD_HANDOFF) && defined(__TI_ARM__)
    {
        static esp_at_controller_t controller;
        const esp_at_controller_config_t config = {
            WIFI_SSID, WIFI_PASSWORD, THINGSBOARD_TOKEN, PHASE3_MQTT_HOST,
            PHASE3_MQTT_PORT, PHASE3_DEVICE_MODEL
        };
        app_cloud_init();
        app_ota_handoff_init(&g_handoff,
            (ota_version_t){APP_VERSION_MAJOR, APP_VERSION_MINOR,
                            APP_VERSION_PATCH});
        esp_at_controller_init(&controller, &config, app_tx, app_log, NULL,
                               g_app_millis);
        esp_at_controller_set_ota_start(&controller, app_start_ota, NULL);
        for (;;) {
            while (UARTCharsAvail(UART1_BASE)) {
                esp_at_controller_receive(&controller,
                    (unsigned char)UARTCharGetNonBlocking(UART1_BASE),
                    g_app_millis);
            }
            esp_at_controller_tick(&controller, g_app_millis);
            if (app_ota_handoff_should_reset(&g_handoff, g_app_millis,
                                             UARTBusy(UART1_BASE))) {
                HWREG(NVIC_APINT) = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
                for (;;) { }
            }
            SysCtlDelay(SysCtlClockGet() / 3000U);
            ++g_app_millis;
        }
    }
#else
    for (;;) {}
#endif
}
