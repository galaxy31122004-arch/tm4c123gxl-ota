#ifndef OTA_CRC32_H
#define OTA_CRC32_H

#include <stddef.h>
#include <stdint.h>

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "OTA firmware requires a C11 compiler"
#endif

uint32_t ota_crc32_init(void);
uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, size_t length);
uint32_t ota_crc32_finish(uint32_t crc);

static inline uint32_t ota_crc32(const uint8_t *data, size_t length)
{
    return ota_crc32_finish(ota_crc32_update(ota_crc32_init(), data, length));
}

#endif
