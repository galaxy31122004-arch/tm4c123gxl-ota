--retain=g_pfnVectors
--entry_point=ResetISR
--stack_size=512
MEMORY
{
    FLASH (RX) : origin = 0x00008400, length = 0x0001B800
    SRAM (RWX) : origin = 0x20000000, length = 0x00007FA0
    OTA_REQUEST (RW) : origin = 0x20007FA0, length = 0x00000020
    BOOT_CONFIRM (RW) : origin = 0x20007FC0, length = 0x00000040
}
SECTIONS
{
    .intvecs : > 0x00008400
    .text : > FLASH
    .const : > FLASH
    .cinit : > FLASH
    .pinit : > FLASH
    .init_array : > FLASH
    .data : load = FLASH, run = SRAM
    .bss : > SRAM
    .stack : > SRAM
    .ota_request : > OTA_REQUEST, type=NOINIT
    .boot_confirm : > BOOT_CONFIRM, type=NOINIT
}

__STACK_TOP = 0x20007FA0;
