--retain=g_pfnVectors
--entry_point=ResetISR
--stack_size=512
MEMORY
{
    FLASH (RX) : origin = 0x00024000, length = 0x0001B800
    SRAM (RWX) : origin = 0x20000000, length = 0x00007FC0
    NOINIT (RW) : origin = 0x20007FC0, length = 0x00000040
}
SECTIONS
{
    .intvecs : > 0x00024000
    .text : > FLASH
    .const : > FLASH
    .cinit : > FLASH
    .pinit : > FLASH
    .init_array : > FLASH
    .data : > SRAM
    .bss : > SRAM
    .stack : > SRAM
    .noinit : > NOINIT, type=NOINIT
}
