#ifndef BL_UPDATE_H
#define BL_UPDATE_H

#include "ota_image.h"
#include "ota_metadata.h"
#include "ota_protocol.h"

typedef enum {
    BL_UPDATE_IDLE = 0,
    BL_UPDATE_RECEIVING,
    BL_UPDATE_READY_TO_BOOT
} bl_update_state_t;

typedef enum {
    BL_ERROR_NONE = 0,
    BL_ERROR_COMMAND = 1,
    BL_ERROR_STATE = 2,
    BL_ERROR_SLOT = 3,
    BL_ERROR_SIZE = 4,
    BL_ERROR_HEADER = 5,
    BL_ERROR_SEQUENCE = 6,
    BL_ERROR_FLASH = 7,
    BL_ERROR_VERIFY = 8,
    BL_ERROR_TIMEOUT = 9
} bl_update_error_t;

typedef int (*bl_update_read_fn)(void *context, uint32_t address, uint8_t *destination, size_t length);
typedef int (*bl_update_erase_fn)(void *context, uint32_t address, size_t length, ota_slot_t active);
typedef int (*bl_update_program_fn)(void *context, uint32_t address, const uint8_t *source, size_t length, ota_slot_t active);
typedef void (*bl_update_reset_fn)(void *context);

typedef struct {
    bl_update_read_fn read;
    bl_update_erase_fn erase;
    bl_update_program_fn program;
    ota_metadata_io_t metadata_io;
    unsigned metadata_copy;
    ota_metadata_record_t metadata;
    bl_update_reset_fn reset;
    void *context;
} bl_services_t;

typedef struct {
    bl_services_t services;
    bl_update_state_t state;
    ota_firmware_header_t header;
    uint32_t received;
    uint16_t next_sequence;
    uint16_t last_sequence;
    uint16_t last_length;
    uint32_t last_crc;
    uint32_t last_activity_ms;
    uint8_t have_last;
    bl_update_error_t last_error;
} bl_update_t;

void bl_update_init(bl_update_t *update, const bl_services_t *services);
void bl_update_handle(bl_update_t *update, const ota_packet_t *request, ota_packet_t *response);
void bl_update_poll(bl_update_t *update, uint32_t now_ms);

#endif
