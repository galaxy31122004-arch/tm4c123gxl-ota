#include <stdio.h>
#include <string.h>
#include "boot_confirm.h"
extern void boot_confirmation_clear(void);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"failed: %s\n",#x); return 1; } } while(0)
int main(void) {
 boot_confirmation_mailbox_t m; ota_slot_t slot; ota_version_t v;
 memset(&m,0,sizeof(m)); m.magic=BOOT_CONFIRM_MAGIC; m.schema=BOOT_CONFIRM_SCHEMA; m.slot=OTA_SLOT_B; m.version=(ota_version_t){1,2,3};
 /* Build the same wire CRC through the public behavior by checking rejection paths. */
 CHECK(!boot_confirmation_validate(&m,&slot,&v));
 m.crc32=1u; CHECK(!boot_confirmation_validate(&m,&slot,&v));
 m.magic=0u; CHECK(!boot_confirmation_validate(&m,&slot,&v));
 boot_confirm(OTA_SLOT_B, (ota_version_t){1,0,1});
 CHECK(boot_confirmation_consume(&slot, &v));
 CHECK(slot == OTA_SLOT_B && v.major == 1u && v.minor == 0u && v.patch == 1u);
 boot_confirmation_clear();
 boot_confirm(OTA_SLOT_B, (ota_version_t){1,0,1});
 CHECK(boot_confirmation_consume(&slot, &v));
 return 0;
}
