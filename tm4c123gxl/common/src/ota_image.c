#include "ota_image.h"

#include <string.h>

#include "ota_config.h"
#include "ota_crc32.h"

enum {
    OTA_HEADER_CRC_OFFSET = 22u,
    OTA_VECTOR_SIZE = 8u,
    OTA_CRC_BUFFER_SIZE = 256u
};

static uint32_t slot_payload_start(ota_slot_t slot)
{
    return slot == OTA_SLOT_A ? OTA_SLOT_A_PAYLOAD_START : OTA_SLOT_B_PAYLOAD_START;
}

uint32_t ota_header_crc32(const ota_firmware_header_t *header)
{
    uint8_t bytes[sizeof(*header)];

    if (header == NULL) {
        return 0u;
    }
    (void)memcpy(bytes, header, sizeof(bytes));
    (void)memset(bytes + OTA_HEADER_CRC_OFFSET, 0, sizeof(header->header_crc32));
    return ota_crc32(bytes, sizeof(bytes));
}

ota_image_result_t ota_header_validate(const ota_firmware_header_t *header, ota_slot_t slot)
{
    size_t index;

    if (header == NULL || (slot != OTA_SLOT_A && slot != OTA_SLOT_B) || header->magic != OTA_FIRMWARE_MAGIC ||
        header->schema_version != OTA_FIRMWARE_SCHEMA_VERSION || header->target_slot != (uint8_t)slot ||
        header->reserved0 != 0u || header->payload_size == 0u || header->payload_size > OTA_SLOT_PAYLOAD_SIZE ||
        header->header_crc32 != ota_header_crc32(header)) {
        return OTA_IMAGE_BAD_HEADER;
    }
    for (index = 0u; index < sizeof(header->reserved); ++index) {
        if (header->reserved[index] != 0u) {
            return OTA_IMAGE_BAD_HEADER;
        }
    }
    return OTA_IMAGE_OK;
}

ota_image_result_t ota_vector_validate(uint32_t initial_msp, uint32_t reset_vector, ota_slot_t slot)
{
    const uint32_t payload_start = slot_payload_start(slot);
    const uint32_t reset_address = reset_vector & ~UINT32_C(1);

    if ((initial_msp & UINT32_C(3)) != 0u || initial_msp < OTA_SRAM_START || initial_msp > OTA_SRAM_END) {
        return OTA_IMAGE_BAD_MSP;
    }
    if ((slot != OTA_SLOT_A && slot != OTA_SLOT_B) || (reset_vector & UINT32_C(1)) == 0u ||
        reset_address < payload_start || reset_address >= payload_start + OTA_SLOT_PAYLOAD_SIZE) {
        return OTA_IMAGE_BAD_RESET;
    }
    return OTA_IMAGE_OK;
}

ota_image_result_t ota_image_validate(ota_reader_t reader, ota_slot_t slot, ota_image_info_t *info)
{
    ota_firmware_header_t header;
    uint8_t vector[OTA_VECTOR_SIZE];
    uint8_t buffer[OTA_CRC_BUFFER_SIZE];
    uint32_t address;
    uint32_t remaining;
    uint32_t crc;

    if (reader.read == NULL || info == NULL || (slot != OTA_SLOT_A && slot != OTA_SLOT_B)) {
        return OTA_IMAGE_READ_ERROR;
    }
    address = slot == OTA_SLOT_A ? OTA_SLOT_A_START : OTA_SLOT_B_START;
    if (reader.read(reader.context, address, (uint8_t *)&header, sizeof(header)) != 0) {
        return OTA_IMAGE_READ_ERROR;
    }
    if (ota_header_validate(&header, slot) != OTA_IMAGE_OK) {
        return OTA_IMAGE_BAD_HEADER;
    }
    address = slot_payload_start(slot);
    if (reader.read(reader.context, address, vector, sizeof(vector)) != 0) {
        return OTA_IMAGE_READ_ERROR;
    }
    {
        const ota_image_result_t vector_result = ota_vector_validate(
            (uint32_t)vector[0] | ((uint32_t)vector[1] << 8u) | ((uint32_t)vector[2] << 16u) | ((uint32_t)vector[3] << 24u),
            (uint32_t)vector[4] | ((uint32_t)vector[5] << 8u) | ((uint32_t)vector[6] << 16u) | ((uint32_t)vector[7] << 24u), slot);
        if (vector_result != OTA_IMAGE_OK) {
            return vector_result;
        }
    }
    crc = ota_crc32_init();
    remaining = header.payload_size;
    while (remaining != 0u) {
        const size_t length = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        if (reader.read(reader.context, address, buffer, length) != 0) {
            return OTA_IMAGE_READ_ERROR;
        }
        crc = ota_crc32_update(crc, buffer, length);
        address += (uint32_t)length;
        remaining -= (uint32_t)length;
    }
    if (ota_crc32_finish(crc) != header.payload_crc32) {
        return OTA_IMAGE_BAD_PAYLOAD_CRC;
    }
    info->header = header;
    info->payload_address = slot_payload_start(slot);
    return OTA_IMAGE_OK;
}
