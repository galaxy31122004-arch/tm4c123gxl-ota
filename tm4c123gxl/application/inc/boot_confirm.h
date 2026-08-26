#ifndef BOOT_CONFIRM_H
#define BOOT_CONFIRM_H
#include "ota_types.h"
#include <stdint.h>
#define BOOT_CONFIRM_MAGIC UINT32_C(0x434F4E46)
#define BOOT_CONFIRM_SCHEMA UINT16_C(1)
typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint8_t slot;
    uint8_t reserved;
    ota_version_t version;
    uint32_t crc32;
    uint8_t padding[44];
} boot_confirmation_mailbox_t;
_Static_assert(sizeof(boot_confirmation_mailbox_t) == 64u, "mailbox size");
void boot_confirm(ota_slot_t slot, ota_version_t version);
int boot_confirmation_validate(const boot_confirmation_mailbox_t *mailbox, ota_slot_t *slot, ota_version_t *version);
int boot_confirmation_consume(ota_slot_t *slot, ota_version_t *version);
#endif
