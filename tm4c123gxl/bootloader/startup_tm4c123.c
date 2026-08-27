#include <stdint.h>

extern void bl_main(void);
extern uint8_t __boot_load_start;
extern uint8_t __boot_run_start;
extern uint8_t __boot_size;
extern uint8_t __boot_bss_start;
extern uint8_t __boot_bss_end;
extern void SysTickIntHandler(void);
extern uint32_t __STACK_TOP;

void ResetISR(void);
static void NmiISR(void) { for (;;) {} }
static void FaultISR(void) { for (;;) {} }
static void DefaultISR(void) { for (;;) {} }

__attribute__((section(".intvecs")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))((uint32_t)&__STACK_TOP), ResetISR, NmiISR, FaultISR,
    DefaultISR, DefaultISR, DefaultISR, 0, 0, 0, 0, DefaultISR, DefaultISR,
    0, DefaultISR, SysTickIntHandler
};

__attribute__((section(".startup"), noreturn)) void ResetISR(void)
{
    const uint8_t *source = &__boot_load_start;
    uint8_t *destination = &__boot_run_start;
    uint32_t remaining = (uint32_t)(uintptr_t)&__boot_size;
    uint8_t *bss = &__boot_bss_start;

    while (remaining-- != 0u) *destination++ = *source++;
    while (bss < &__boot_bss_end) *bss++ = 0u;
    bl_main();
    for (;;) {}
}
