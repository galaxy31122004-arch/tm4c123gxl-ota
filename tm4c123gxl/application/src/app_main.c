#include "boot_confirm.h"
int main(void) {
#if !defined(NO_CONFIRM)
    ota_version_t version = {1u, 0u, 0u};
    boot_confirm((ota_slot_t)OTA_APP_SLOT, version);
#endif
    for (;;) {}
}
