#include "bl_hal.h"
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

#if defined(__TI_ARM__) || defined(TARGET_IS_TM4C123_RB1)
#include "driverlib/flash.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/uart.h"
#include "inc/hw_memmap.h"
#include "inc/hw_nvic.h"
#include "inc/hw_types.h"
static uint32_t g_clock;
void bl_hal_init(void) { g_clock = SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_OSC_MAIN|SYSCTL_XTAL_16MHZ); }
uint32_t bl_hal_millis(void) { static uint32_t ms; return ++ms; }
void bl_hal_watchdog_service(void) { }
void bl_hal_reset(void) { HWREG(NVIC_APINT) = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ; for (;;) {} }
ota_error_t bl_hal_flash_read(uint32_t a, void *d, size_t n) { memcpy(d, (const void *)(uintptr_t)a, n); return OTA_ERROR_NONE; }
ota_error_t bl_hal_flash_erase(uint32_t a, size_t n, ota_slot_t s) { if (!bl_flash_range_allowed(a,n,s,BL_FLASH_ERASE)) return OTA_ERROR_FLASH_ERASE; for (uint32_t p=a;p<a+n;p+=OTA_FLASH_PAGE_SIZE) if (FlashErase(p)!=0) return OTA_ERROR_FLASH_ERASE; return OTA_ERROR_NONE; }
ota_error_t bl_hal_flash_program(uint32_t a, const void *src, size_t n, ota_slot_t s) { if (!bl_flash_range_allowed(a,n,s,BL_FLASH_PROGRAM)) return OTA_ERROR_FLASH_PROGRAM; if (FlashProgram((uint32_t *)src,a,(uint32_t)n)!=0) return OTA_ERROR_FLASH_PROGRAM; return OTA_ERROR_NONE; }
int bl_hal_uart1_read(uint8_t *b, uint32_t t) { uint32_t start=bl_hal_millis(); while (!UARTCharsAvail(UART1_BASE) && bl_hal_millis()-start<t) {} if (!UARTCharsAvail(UART1_BASE)) return 0; *b=(uint8_t)UARTCharGetNonBlocking(UART1_BASE); return 1; }
void bl_hal_uart1_write(uint8_t b) { UARTCharPut(UART1_BASE,b); }
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
void bl_hal_uart0_log(const char *m){(void)m;}
#endif
