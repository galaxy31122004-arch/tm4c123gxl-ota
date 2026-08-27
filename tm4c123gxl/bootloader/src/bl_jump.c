#include <stdint.h>
#include "ota_config.h"

#define SYSTICK_CONTROL_ADDRESS UINT32_C(0xE000E010)
#define NVIC_INTERRUPT_CONTROL_ADDRESS UINT32_C(0xE000ED04)
#define NVIC_INTERRUPT_CONTROL_PENDSTCLR UINT32_C(0x02000000)

void bl_jump_quiesce_systick(volatile uint32_t *control, volatile uint32_t *interrupt_control)
{
    *control = 0u;
    *interrupt_control = NVIC_INTERRUPT_CONTROL_PENDSTCLR;
}

typedef void (*bl_reset_handler_t)(void);
__attribute__((noreturn)) void bl_jump_to_image(uint32_t vector_address) {
    uint32_t msp = *(volatile uint32_t *)(uintptr_t)vector_address;
    uint32_t reset = *(volatile uint32_t *)(uintptr_t)(vector_address + 4u);
#if defined(__TI_ARM__)
    __asm volatile("cpsid i\n dsb\n isb" ::: "memory");
    bl_jump_quiesce_systick((volatile uint32_t *)(uintptr_t)SYSTICK_CONTROL_ADDRESS,
                            (volatile uint32_t *)(uintptr_t)NVIC_INTERRUPT_CONTROL_ADDRESS);
    __asm volatile("dsb\n isb" ::: "memory");
    *((volatile uint32_t *)0xE000ED08u) = vector_address;
    __asm volatile("msr msp, %0\n bx %1" :: "r"(msp), "r"(reset) : "memory");
#else
    (void)msp; (void)reset;
#endif
    for (;;) {}
}
