--retain=g_pfnVectors
--stack_size=2048

MEMORY
{
    FLASH (RX) : origin = 0x00000000, length = 0x00008000
    SRAM (RWX) : origin = 0x20000000, length = 0x00007FA0
    OTA_REQUEST (RW) : origin = 0x20007FA0, length = 0x00000020
    BOOT_CONFIRM (RW) : origin = 0x20007FC0, length = 0x00000040
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
    .ota_request : > OTA_REQUEST, type=NOINIT
    .boot_confirm : > BOOT_CONFIRM, type=NOINIT
}

__STACK_TOP = 0x20007FA0;
