#include <stdint.h>
#include <stdio.h>

#include "ota_crc32.h"

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    const uint8_t input[] = "123456789";
    uint32_t crc;

    CHECK(ota_crc32(input, 9u) == UINT32_C(0xCBF43926));

    crc = ota_crc32_init();
    crc = ota_crc32_update(crc, input, 4u);
    crc = ota_crc32_update(crc, input + 4u, 5u);
    CHECK(ota_crc32_finish(crc) == UINT32_C(0xCBF43926));
    CHECK(ota_crc32(NULL, 0u) == UINT32_C(0x00000000));

    return 0;
}
