#include "boot_confirm.h"
#include "ota_crc32.h"
#include <string.h>

#if defined(__TI_ARM__)
#define NOINIT __attribute__((section(".noinit")))
#else
#define NOINIT
#endif
static volatile boot_confirmation_mailbox_t g_mailbox NOINIT;

static uint32_t mailbox_crc(const boot_confirmation_mailbox_t *m) {
    boot_confirmation_mailbox_t copy = *m;
    copy.crc32 = 0u;
    return ota_crc32((const uint8_t *)&copy, sizeof(copy));
}
int boot_confirmation_validate(const boot_confirmation_mailbox_t *m, ota_slot_t *slot, ota_version_t *version) {
    if (!m || m->magic != BOOT_CONFIRM_MAGIC || m->schema != BOOT_CONFIRM_SCHEMA || m->reserved != 0u || m->slot > OTA_SLOT_B) return 0;
    if (mailbox_crc(m) != m->crc32) return 0;
    if (slot) *slot = (ota_slot_t)m->slot;
    if (version) *version = m->version;
    return 1;
}
int boot_confirmation_consume(ota_slot_t *slot, ota_version_t *version) {
    int ok = boot_confirmation_validate((const boot_confirmation_mailbox_t *)&g_mailbox, slot, version);
    if (ok) { boot_confirmation_mailbox_t consumed = g_mailbox; consumed.magic = BOOT_CONFIRMED_MAGIC; consumed.crc32 = 0u; memcpy((void *)&g_mailbox, &consumed, sizeof(consumed)); }
    else if (g_mailbox.magic != BOOT_CONFIRMED_MAGIC) memset((void *)&g_mailbox, 0xff, sizeof(g_mailbox));
    return ok;
}
void boot_confirmation_clear(void) { memset((void *)&g_mailbox, 0xff, sizeof(g_mailbox)); }
void boot_confirm(ota_slot_t slot, ota_version_t version) {
    boot_confirmation_mailbox_t m;
    if (g_mailbox.magic == BOOT_CONFIRMED_MAGIC && g_mailbox.slot == (uint8_t)slot && memcmp((const void *)&g_mailbox.version, &version, sizeof(version)) == 0) return;
    memset(&m, 0, sizeof(m));
    m.magic = BOOT_CONFIRM_MAGIC; m.schema = BOOT_CONFIRM_SCHEMA; m.slot = (uint8_t)slot; m.version = version; m.crc32 = mailbox_crc(&m);
    memcpy((void *)&g_mailbox, &m, sizeof(m));
#if defined(__arm__) || defined(__thumb__)
    __asm volatile("dsb\n isb" ::: "memory");
#endif
#if defined(__TI_ARM__)
    *((volatile uint32_t *)0xE000ED0Cu) = (UINT32_C(0x5FA) << 16) | (UINT32_C(1) << 2);
    for (;;) {}
#endif
}
