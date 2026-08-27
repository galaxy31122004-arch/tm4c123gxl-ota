#include "ota_request.h"

#include <string.h>

#include "ota_crc32.h"

#if defined(__TI_ARM__)
#define OTA_REQUEST_NOINIT __attribute__((section(".ota_request")))
#else
#define OTA_REQUEST_NOINIT
#endif

static volatile ota_request_mailbox_t g_ota_request OTA_REQUEST_NOINIT;

static uint32_t request_crc(const ota_request_mailbox_t *mailbox)
{
    ota_request_mailbox_t copy = *mailbox;
    copy.crc32 = 0U;
    return ota_crc32((const uint8_t *)&copy, sizeof(copy));
}

int ota_request_validate(const ota_request_mailbox_t *mailbox,
                         ota_version_t *version)
{
    if (mailbox == NULL || mailbox->magic != OTA_REQUEST_MAGIC ||
        mailbox->schema != OTA_REQUEST_SCHEMA || mailbox->reserved != 0U ||
        (mailbox->version.major == 0U && mailbox->version.minor == 0U &&
         mailbox->version.patch == 0U) || request_crc(mailbox) != mailbox->crc32) {
        return 0;
    }
    if (version != NULL) {
        *version = mailbox->version;
    }
    return 1;
}

int ota_request_store(ota_version_t version)
{
    ota_request_mailbox_t mailbox;
    if (version.major == 0U && version.minor == 0U && version.patch == 0U) {
        return 0;
    }
    (void)memset(&mailbox, 0, sizeof(mailbox));
    mailbox.magic = OTA_REQUEST_MAGIC;
    mailbox.schema = OTA_REQUEST_SCHEMA;
    mailbox.version = version;
    mailbox.crc32 = request_crc(&mailbox);
    (void)memcpy((void *)&g_ota_request, &mailbox, sizeof(mailbox));
#if defined(__arm__) || defined(__thumb__)
    __asm volatile("dsb\n isb" ::: "memory");
#endif
    return 1;
}

int ota_request_consume(ota_version_t *version)
{
    int valid = ota_request_validate((const ota_request_mailbox_t *)&g_ota_request,
                                     version);
    ota_request_clear();
    return valid;
}

void ota_request_clear(void)
{
    (void)memset((void *)&g_ota_request, 0xFF, sizeof(g_ota_request));
}
