#ifndef CLOUD_OTA_H
#define CLOUD_OTA_H

#include <stddef.h>
#include <stdint.h>

#include "bl_update.h"

typedef enum {
    CLOUD_OTA_OK = 0,
    CLOUD_OTA_READY_TO_REBOOT,
    CLOUD_OTA_INVALID,
    CLOUD_OTA_SIZE,
    CLOUD_OTA_HEADER,
    CLOUD_OTA_ENGINE,
    CLOUD_OTA_TRUNCATED
} cloud_ota_result_t;

typedef struct {
    bl_update_t *update;
    size_t remote_size;
    size_t package_received;
    size_t header_received;
    size_t chunk_length;
    uint16_t sequence;
    int started;
    ota_version_t expected_version;
    ota_firmware_header_t header;
    uint8_t chunk[OTA_PROTOCOL_MAX_PAYLOAD_SIZE];
    ota_packet_t request;
    ota_packet_t response;
} cloud_ota_t;

cloud_ota_result_t cloud_ota_begin(cloud_ota_t *cloud, bl_update_t *update,
                                   size_t remote_size,
                                   const ota_version_t *expected_version);
cloud_ota_result_t cloud_ota_write(cloud_ota_t *cloud,
                                   const unsigned char *data, size_t length);
cloud_ota_result_t cloud_ota_finish(cloud_ota_t *cloud);
void cloud_ota_abort(cloud_ota_t *cloud);
unsigned cloud_ota_progress(const cloud_ota_t *cloud);

#endif
