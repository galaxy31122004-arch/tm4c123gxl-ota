#include <stdio.h>
#include <stdint.h>
#include "bl_hal.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"failed: %s\n",#x); return 1; } } while(0)
int main(void) {
 CHECK(bl_flash_range_allowed(OTA_SLOT_B_START, OTA_FLASH_PAGE_SIZE, OTA_SLOT_A, BL_FLASH_ERASE));
 CHECK(bl_flash_range_allowed(OTA_SLOT_B_START+OTA_SLOT_SIZE-1,1,OTA_SLOT_A,BL_FLASH_PROGRAM));
 CHECK(!bl_flash_range_allowed(OTA_SLOT_A_START,1,OTA_SLOT_A,BL_FLASH_PROGRAM));
 CHECK(!bl_flash_range_allowed(OTA_SLOT_B_START-1,2,OTA_SLOT_A,BL_FLASH_PROGRAM));
 CHECK(!bl_flash_range_allowed(UINT32_MAX-1,4,OTA_SLOT_A,BL_FLASH_PROGRAM));
 CHECK(!bl_flash_range_allowed(OTA_SLOT_B_START,0,OTA_SLOT_A,BL_FLASH_PROGRAM));
 CHECK(!bl_flash_range_allowed(OTA_SLOT_B_START+1,OTA_FLASH_PAGE_SIZE,OTA_SLOT_A,BL_FLASH_ERASE));
 CHECK(bl_flash_range_allowed(OTA_METADATA_COPY0_START,OTA_FLASH_END-OTA_METADATA_COPY0_START,OTA_SLOT_A,BL_FLASH_ERASE));
 return 0;
}
