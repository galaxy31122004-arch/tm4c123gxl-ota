#include "bl_hal.h"
#include <stdbool.h>
#include <string.h>

static int range(uint32_t start, uint32_t end, uint32_t address, size_t length)
{
    uint32_t finish;
    if (length == 0u || length > UINT32_MAX || address < start || address > UINT32_MAX - (uint32_t)length) return 0;
    finish = address + (uint32_t)length;
    return finish <= end;
}

int bl_flash_range_allowed(uint32_t address, size_t length, ota_slot_t active, bl_flash_operation_t operation)
{
    uint32_t target = (active == OTA_SLOT_A) ? OTA_SLOT_B_START : OTA_SLOT_A_START;
    uint32_t target_end = target + OTA_SLOT_SIZE;
    int metadata = range(OTA_METADATA_COPY0_START, OTA_FLASH_END, address, length);
    if (operation == BL_FLASH_ERASE && ((address % OTA_FLASH_PAGE_SIZE) != 0u || (length % OTA_FLASH_PAGE_SIZE) != 0u)) return 0;
    return range(target, target_end, address, length) || metadata;
}

size_t bl_hal_flash_program_padded_length(size_t length)
{
    if (length == 0u || length > SIZE_MAX - 3u) return 0u;
    return (length + 3u) & ~(size_t)3u;
}

#if defined(__TI_ARM__) || defined(TARGET_IS_TM4C123_RB1)
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "driverlib/rom.h"
#include "inc/hw_flash.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
#include "driverlib/pin_map.h"
static uint32_t g_clock;
static uint32_t g_transport_uart;
static volatile uint32_t g_millis;

void SysTickIntHandler(void) { ++g_millis; }

void bl_hal_init(void) {
    g_transport_uart = UART1_BASE;
    g_millis = 0u;
    SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ);
    g_clock = SysCtlClockGet();
    SysTickPeriodSet(g_clock / UINT32_C(1000));
    SysTickIntEnable();
    SysTickEnable();
    IntMasterEnable();
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA); SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0); SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
    while (!SysCtlPeripheralReady(SYSCTL_PERIPH_UART0) || !SysCtlPeripheralReady(SYSCTL_PERIPH_UART1)) { }
    GPIOPinConfigure(GPIO_PA0_U0RX); GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinConfigure(GPIO_PB0_U1RX); GPIOPinConfigure(GPIO_PB1_U1TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTConfigSetExpClk(UART0_BASE, g_clock, OTA_UART1_BAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTConfigSetExpClk(UART1_BASE, g_clock, OTA_UART1_BAUD_RATE, UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE);
    UARTEnable(UART0_BASE);
    UARTEnable(UART1_BASE);
}
uint32_t bl_hal_millis(void) { return g_millis; }
void bl_hal_watchdog_service(void) { }
void bl_hal_reset(void) { HWREG(NVIC_APINT) = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ; for (;;) {} }
ota_error_t bl_hal_flash_read(uint32_t a, void *d, size_t n) { memcpy(d, (const void *)(uintptr_t)a, n); return OTA_ERROR_NONE; }
ota_error_t bl_hal_flash_erase(uint32_t a, size_t n, ota_slot_t s) { uint32_t p; if (!bl_flash_range_allowed(a,n,s,BL_FLASH_ERASE)) return OTA_ERROR_FLASH_ERASE; for (p=a;p<a+n;p+=OTA_FLASH_PAGE_SIZE) { HWREG(FLASH_FCMISC) = FLASH_FCMISC_AMISC | FLASH_FCMISC_VOLTMISC | FLASH_FCMISC_ERMISC; HWREG(FLASH_FMA) = p; HWREG(FLASH_FMC) = FLASH_FMC_WRKEY | FLASH_FMC_ERASE; while ((HWREG(FLASH_FMC) & FLASH_FMC_ERASE) != 0u) {} if ((HWREG(FLASH_FCRIS) & (FLASH_FCRIS_ARIS | FLASH_FCRIS_VOLTRIS | FLASH_FCRIS_ERRIS)) != 0u) return OTA_ERROR_FLASH_ERASE; } return OTA_ERROR_NONE; }
ota_error_t bl_hal_flash_program(uint32_t a, const void *src, size_t n, ota_slot_t s) {
    uint32_t words[OTA_PROTOCOL_MAX_PAYLOAD_SIZE / sizeof(uint32_t)];
    size_t padded = bl_hal_flash_program_padded_length(n);
    if (src == NULL || padded == 0u || padded > sizeof(words) || !bl_flash_range_allowed(a, padded, s, BL_FLASH_PROGRAM)) return OTA_ERROR_FLASH_PROGRAM;
    (void)memset(words, 0xff, sizeof(words));
    (void)memcpy(words, src, n);
    if (ROM_FlashProgram(words, a, (uint32_t)padded) != 0) return OTA_ERROR_FLASH_PROGRAM;
    return OTA_ERROR_NONE;
}
int bl_hal_uart1_read(uint8_t *b, uint32_t t) { uint32_t start=bl_hal_millis(); while (!UARTCharsAvail(UART0_BASE) && !UARTCharsAvail(UART1_BASE)) { if ((uint32_t)(bl_hal_millis()-start)>=t) return 0; } if (UARTCharsAvail(UART0_BASE)) g_transport_uart=UART0_BASE; else g_transport_uart=UART1_BASE; *b=(uint8_t)UARTCharGetNonBlocking(g_transport_uart); return 1; }
void bl_hal_uart1_write(uint8_t b) { UARTCharPut(g_transport_uart,b); }
void bl_hal_uart_wait_tx_complete(void) { while (UARTBusy(g_transport_uart)) {} }
void bl_hal_uart0_log(const char *m) { while (*m) UARTCharPut(UART0_BASE,*m++); }
#else
uint32_t bl_hal_millis(void) { return 0u; }
void bl_hal_watchdog_service(void) { }
void bl_hal_reset(void) { }
ota_error_t bl_hal_flash_read(uint32_t a, void *d, size_t n) { (void)a;(void)d;(void)n; return OTA_ERROR_FLASH_VERIFY; }
ota_error_t bl_hal_flash_erase(uint32_t a,size_t n,ota_slot_t s) { return bl_flash_range_allowed(a,n,s,BL_FLASH_ERASE)?OTA_ERROR_NONE:OTA_ERROR_FLASH_ERASE; }
ota_error_t bl_hal_flash_program(uint32_t a,const void *d,size_t n,ota_slot_t s) { (void)d; return bl_flash_range_allowed(a,n,s,BL_FLASH_PROGRAM)?OTA_ERROR_NONE:OTA_ERROR_FLASH_PROGRAM; }
int bl_hal_uart1_read(uint8_t *b,uint32_t t) {(void)b;(void)t;return 0;}
void bl_hal_uart1_write(uint8_t b){(void)b;}
void bl_hal_uart_wait_tx_complete(void){}
void bl_hal_uart0_log(const char *m){(void)m;}
#endif
