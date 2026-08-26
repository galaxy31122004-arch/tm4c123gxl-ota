#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_image.h"
#include "ota_crc32.h"
#include "ota_config.h"

#define CHECK(condition) do { if (!(condition)) { (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); return 1; } } while (0)

static uint8_t image[OTA_SLOT_HEADER_SIZE + 16u];

static int read_image(void *context, uint32_t address, uint8_t *destination, size_t length)
{
    const uint32_t base = *(const uint32_t *)context;
    if (address < base || length > sizeof(image) || address - base > sizeof(image) - length) {
        return -1;
    }
    (void)memcpy(destination, image + address - base, length);
    return 0;
}

static void write_u32(uint8_t *buffer, uint32_t value)
{
    buffer[0] = (uint8_t)value;
    buffer[1] = (uint8_t)(value >> 8u);
    buffer[2] = (uint8_t)(value >> 16u);
    buffer[3] = (uint8_t)(value >> 24u);
}

int main(void)
{
    ota_firmware_header_t header;
    ota_reader_t reader;
    ota_image_info_t info;
    uint32_t base = OTA_SLOT_A_START;

    CHECK(ota_vector_validate(UINT32_C(0x20008000), UINT32_C(0x00008401), OTA_SLOT_A) == OTA_IMAGE_OK);
    CHECK(ota_vector_validate(UINT32_C(0x1ffffffc), UINT32_C(0x00008401), OTA_SLOT_A) == OTA_IMAGE_BAD_MSP);
    CHECK(ota_vector_validate(UINT32_C(0x20008000), UINT32_C(0x00024001), OTA_SLOT_A) == OTA_IMAGE_BAD_RESET);

    (void)memset(&header, 0, sizeof(header));
    header.magic = OTA_FIRMWARE_MAGIC;
    header.schema_version = OTA_FIRMWARE_SCHEMA_VERSION;
    header.target_slot = OTA_SLOT_A;
    header.version.major = 1u;
    header.payload_size = 16u;
    (void)memset(image, 0xff, sizeof(image));
    write_u32(image + OTA_SLOT_HEADER_SIZE, UINT32_C(0x20008000));
    write_u32(image + OTA_SLOT_HEADER_SIZE + 4u, UINT32_C(0x00008401));
    header.payload_crc32 = ota_crc32(image + OTA_SLOT_HEADER_SIZE, 16u);
    header.header_crc32 = ota_header_crc32(&header);
    CHECK(ota_header_validate(&header, OTA_SLOT_A) == OTA_IMAGE_OK);
    header.reserved0 = 1u;
    CHECK(ota_header_validate(&header, OTA_SLOT_A) == OTA_IMAGE_BAD_HEADER);
    header.reserved0 = 0u;
    header.header_crc32 = ota_header_crc32(&header);
    (void)memcpy(image, &header, sizeof(header));
    reader.read = read_image;
    reader.context = &base;
    CHECK(ota_image_validate(reader, OTA_SLOT_A, &info) == OTA_IMAGE_OK);
    CHECK(info.header.payload_size == 16u);
    image[OTA_SLOT_HEADER_SIZE + 12u] = 1u;
    CHECK(ota_image_validate(reader, OTA_SLOT_A, &info) == OTA_IMAGE_BAD_PAYLOAD_CRC);
    return 0;
}
