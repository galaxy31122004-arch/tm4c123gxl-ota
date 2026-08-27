#ifndef OTA_REQUEST_H
#define OTA_REQUEST_H

#include <stdint.h>

#include "ota_types.h"

#define OTA_REQUEST_MAGIC UINT32_C(0x4F544152)
#define OTA_REQUEST_SCHEMA UINT16_C(1)

typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint16_t reserved;
    ota_version_t version;
    uint32_t crc32;
    uint8_t padding[12];
} ota_request_mailbox_t;

_Static_assert(sizeof(ota_request_mailbox_t) == 32U, "OTA request mailbox size");

int ota_request_store(ota_version_t version);
int ota_request_validate(const ota_request_mailbox_t *mailbox,
                         ota_version_t *version);
int ota_request_consume(ota_version_t *version);
void ota_request_clear(void);

#endif
