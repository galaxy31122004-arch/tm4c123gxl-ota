#include <stdint.h>
#include <stdio.h>

extern void bl_jump_quiesce_systick(volatile uint32_t *control, volatile uint32_t *interrupt_control);

#define CHECK(condition) do { if (!(condition)) { (void)fprintf(stderr, "CHECK failed: %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)
#define NVIC_INT_CTRL_PENDSTCLR UINT32_C(0x02000000)

int main(void)
{
    uint32_t systick_control = UINT32_C(7);
    uint32_t interrupt_control = 0u;

    bl_jump_quiesce_systick(&systick_control, &interrupt_control);

    CHECK(systick_control == 0u);
    CHECK(interrupt_control == NVIC_INT_CTRL_PENDSTCLR);
    return 0;
}
