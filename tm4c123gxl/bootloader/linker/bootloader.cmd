MEMORY { FLASH (RX) : origin = 0x00000000, length = 0x00008000; SRAM (RWX) : origin = 0x20000000, length = 0x00007FC0; NOINIT (RW) : origin = 0x20007FC0, length = 0x40 }
SECTIONS { .intvecs : > FLASH .text : > FLASH .const : > FLASH .cinit : > FLASH .data : > SRAM .bss : > SRAM .noinit : > NOINIT }
ASSERT(ORIGIN(FLASH) == 0x00000000, "boot flash origin")
ASSERT(LENGTH(FLASH) == 0x8000, "bootloader exceeds 32KB")
