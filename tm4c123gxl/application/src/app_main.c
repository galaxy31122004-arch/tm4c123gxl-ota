#include "boot_confirm.h"
#ifndef APP_VERSION_MAJOR
#define APP_VERSION_MAJOR 1
#define APP_VERSION_MINOR 0
#define APP_VERSION_PATCH 0
#endif
int main(void) {
#if !defined(NO_CONFIRM)
    ota_version_t version = {APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH};
    boot_confirm((ota_slot_t)OTA_APP_SLOT, version);
#endif
    for (;;) {}
}
