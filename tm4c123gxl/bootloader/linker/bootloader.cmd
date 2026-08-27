--retain=g_pfnVectors
--entry_point=ResetISR
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
    .startup : > FLASH
    GROUP
    {
        .text
        .const
        .rodata
        .data
        .cinit
        .pinit
        .init_array
    } load = FLASH, run = SRAM,
      LOAD_START(__boot_load_start), RUN_START(__boot_run_start), SIZE(__boot_size)
    .bss : > SRAM, RUN_START(__boot_bss_start), RUN_END(__boot_bss_end)
    .stack : > SRAM
    .noinit : > NOINIT, type=NOINIT
}

__STACK_TOP = 0x20007FC0;
