#ifndef OTA_IMAGE_H
#define OTA_IMAGE_H

#include <stddef.h>
#include <stdint.h>

#include "ota_types.h"

#define OTA_FIRMWARE_MAGIC UINT32_C(0x3141544f)
#define OTA_FIRMWARE_SCHEMA_VERSION UINT16_C(1)

typedef enum {
    OTA_IMAGE_OK = 0,
    OTA_IMAGE_BAD_HEADER,
    OTA_IMAGE_BAD_MSP,
    OTA_IMAGE_BAD_RESET,
    OTA_IMAGE_BAD_PAYLOAD_CRC,
    OTA_IMAGE_READ_ERROR
} ota_image_result_t;

typedef int (*ota_reader_read_fn)(void *context, uint32_t address, uint8_t *destination, size_t length);

typedef struct {
    ota_reader_read_fn read;
    void *context;
} ota_reader_t;

typedef struct {
    ota_firmware_header_t header;
    uint32_t payload_address;
} ota_image_info_t;

uint32_t ota_header_crc32(const ota_firmware_header_t *header);
ota_image_result_t ota_header_validate(const ota_firmware_header_t *header, ota_slot_t slot);
ota_image_result_t ota_vector_validate(uint32_t initial_msp, uint32_t reset_vector, ota_slot_t slot);
ota_image_result_t ota_image_validate(ota_reader_t reader, ota_slot_t slot, ota_image_info_t *info);

#endif
