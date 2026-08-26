#include <stdint.h>
#include "ota_config.h"
typedef void (*bl_reset_handler_t)(void);
__attribute__((noreturn)) void bl_jump_to_image(uint32_t vector_address) {
    uint32_t msp = *(volatile uint32_t *)vector_address;
    uint32_t reset = *(volatile uint32_t *)(vector_address + 4u);
#if defined(__TI_ARM__)
    __asm volatile("cpsid i\n dsb\n isb" ::: "memory");
    *((volatile uint32_t *)0xE000ED08u) = vector_address;
    __asm volatile("msr msp, %0\n bx %1" :: "r"(msp), "r"(reset) : "memory");
#else
    (void)msp; (void)reset;
#endif
    for (;;) {}
}
