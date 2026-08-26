#include "ota_crc32.h"
#include "ota_config.h"

uint32_t ota_crc32_init(void)
{
    return OTA_CRC32_INITIAL_VALUE;
}

uint32_t ota_crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    size_t index;
    unsigned int bit;

    for (index = 0u; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & UINT32_C(1)) != 0u) {
                crc = (crc >> 1u) ^ OTA_CRC32_REFLECTED_POLYNOMIAL;
            } else {
                crc >>= 1u;
            }
        }
    }

    return crc;
}

uint32_t ota_crc32_finish(uint32_t crc)
{
    return crc ^ OTA_CRC32_FINAL_XOR;
}
