#include <stdint.h>
extern int main(void);
void ResetISR(void) { (void)main(); for (;;) {} }
__attribute__((section(".intvecs"))) const uintptr_t g_pfnVectors[] = { (uintptr_t)0x20007FC0u, (uintptr_t)ResetISR };
