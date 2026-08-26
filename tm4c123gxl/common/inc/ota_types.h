#ifndef OTA_TYPES_H
#define OTA_TYPES_H

#include <stdint.h>

#if !defined(__STDC_VERSION__) || (__STDC_VERSION__ < 201112L)
#error "OTA firmware requires a C11 compiler"
#endif

#if defined(__GNUC__) || defined(__clang__)
#define OTA_PACKED __attribute__((packed))
#else
#define OTA_PACKED
#endif

typedef enum {
    OTA_SLOT_A = 0,
    OTA_SLOT_B = 1,
    OTA_SLOT_NONE = 0xff
} ota_slot_t;

typedef enum {
    OTA_SLOT_EMPTY = 0,
    OTA_SLOT_VALID = 1,
    OTA_SLOT_ACTIVE = 2,
    OTA_SLOT_PENDING = 3,
    OTA_SLOT_CONFIRMED = 4,
    OTA_SLOT_FAILED = 5
} ota_slot_state_t;

typedef struct OTA_PACKED {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} ota_version_t;

typedef struct OTA_PACKED {
    uint32_t magic;
    uint16_t schema_version;
    uint8_t target_slot;
    uint8_t reserved0;
    ota_version_t version;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t header_crc32;
    uint8_t reserved[6];
} ota_firmware_header_t;

typedef struct OTA_PACKED {
    uint8_t state;
    uint8_t reserved0[3];
    ota_version_t version;
    uint16_t reserved1;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t boot_count;
} ota_slot_record_t;

typedef struct OTA_PACKED {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t reserved0;
    uint32_t generation;
    ota_slot_record_t slot_a;
    ota_slot_record_t slot_b;
    uint8_t active_slot;
    uint8_t pending_slot;
    uint16_t last_error;
    uint32_t record_crc32;
    uint8_t reserved[12];
} ota_metadata_record_t;

_Static_assert(sizeof(ota_version_t) == 6u, "ota_version_t size must be stable");
_Static_assert(sizeof(ota_firmware_header_t) == 32u, "ota_firmware_header_t size must be stable");
_Static_assert(sizeof(ota_slot_record_t) == 24u, "ota_slot_record_t size must be stable");
_Static_assert(sizeof(ota_metadata_record_t) == 80u, "ota_metadata_record_t size must be stable");

#endif
