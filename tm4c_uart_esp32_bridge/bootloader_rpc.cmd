--retain=g_pfnVectors
--stack_size=2048

MEMORY
{
    FLASH (RX) : origin = 0x00000000, length = 0x00008000
    SRAM (RWX) : origin = 0x20000000, length = 0x00007FC0
    NOINIT (RW) : origin = 0x20007FC0, length = 0x00000040
}

SECTIONS
{
    .intvecs : > 0x00000000
    .text : > FLASH
    .const : > FLASH
    .rodata : > FLASH
    .cinit : > FLASH
    .pinit : > FLASH
    .init_array : > FLASH
    .data : > SRAM
    .bss : > SRAM
    .sysmem : > SRAM
    .stack : > SRAM
    .noinit : > NOINIT, type=NOINIT
}

__STACK_TOP = 0x20007FC0;
