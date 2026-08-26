#include <stdint.h>
#include <stdio.h>

#include "ota_config.h"
#include "ota_types.h"

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); \
            return 1; \
        } \
    } while (0)

int main(void)
{
    CHECK(OTA_BOOTLOADER_START + OTA_BOOTLOADER_SIZE == OTA_SLOT_A_START);
    CHECK(OTA_SLOT_A_START + OTA_SLOT_SIZE == OTA_SLOT_B_START);
    CHECK(OTA_SLOT_B_START + OTA_SLOT_SIZE == OTA_METADATA_COPY0_START);
    CHECK(OTA_METADATA_COPY1_START + OTA_FLASH_PAGE_SIZE == OTA_FLASH_END);
    CHECK(OTA_SLOT_A_PAYLOAD_START == UINT32_C(0x00008400));
    CHECK(OTA_SLOT_B_PAYLOAD_START == UINT32_C(0x00024000));
    CHECK(OTA_SLOT_PAYLOAD_SIZE == UINT32_C(110) * UINT32_C(1024));
    CHECK(OTA_PROTOCOL_SOF0 == UINT8_C(0x55));
    CHECK(OTA_PROTOCOL_SOF1 == UINT8_C(0xAA));
    CHECK(OTA_PROTOCOL_VERSION == UINT8_C(1));
    CHECK(OTA_PROTOCOL_MAX_PAYLOAD_SIZE == UINT16_C(256));
    CHECK(OTA_UART1_BAUD_RATE == UINT32_C(115200));
    CHECK(OTA_RESPONSE_TIMEOUT_MS == UINT32_C(1000));
    CHECK(OTA_MAX_RETRIES == UINT32_C(3));
    CHECK(OTA_PENDING_BOOT_MAX_ATTEMPTS == UINT32_C(3));
    CHECK(sizeof(ota_version_t) == 6u);
    CHECK(sizeof(ota_firmware_header_t) == 32u);
    CHECK(sizeof(ota_slot_record_t) == 24u);
    CHECK(sizeof(ota_metadata_record_t) == 80u);

    return 0;
}
