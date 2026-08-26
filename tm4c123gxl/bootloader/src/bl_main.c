#include "bl_hal.h"
void bl_main(void) { bl_hal_watchdog_service(); for (;;) { } }
int main(void) { bl_main(); return 0; }
