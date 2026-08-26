#include <stdint.h>
extern int main(void);
void ResetISR(void) {
#if defined(__TI_ARM__)
    __asm(" .global _c_int00\n b.w _c_int00");
#else
    (void)main(); for (;;) {}
#endif
}
__attribute__((section(".intvecs"))) const uintptr_t g_pfnVectors[] = { (uintptr_t)0x20007FC0u, (uintptr_t)ResetISR };
