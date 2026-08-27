#include "cloud_ota.h"

#include <string.h>

static cloud_ota_result_t submit(cloud_ota_t *cloud, uint8_t command,
                                 const void *payload, uint16_t length)
{
    memset(&cloud->request, 0, sizeof(cloud->request));
    cloud->request.command = command;
    cloud->request.sequence = cloud->sequence++;
    cloud->request.length = length;
    if (length != 0U) {
        memcpy(cloud->request.payload, payload, length);
    }
    bl_update_handle(cloud->update, &cloud->request, &cloud->response);
    return cloud->response.command == OTA_CMD_ACK ? CLOUD_OTA_OK :
                                                    CLOUD_OTA_ENGINE;
}

static cloud_ota_result_t start_from_header(cloud_ota_t *cloud)
{
    ota_slot_t target = (ota_slot_t)cloud->header.target_slot;

    if (cloud->header.version.major != cloud->expected_version.major ||
        cloud->header.version.minor != cloud->expected_version.minor ||
        cloud->header.version.patch != cloud->expected_version.patch ||
        cloud->header.payload_size + sizeof(cloud->header) !=
            cloud->remote_size ||
        ota_header_validate(&cloud->header, target) != OTA_IMAGE_OK) {
        return CLOUD_OTA_HEADER;
    }
    if (submit(cloud, OTA_CMD_START_UPDATE, &cloud->header,
               (uint16_t)sizeof(cloud->header)) != CLOUD_OTA_OK) {
        return CLOUD_OTA_ENGINE;
    }
    cloud->started = 1;
    return CLOUD_OTA_OK;
}

static cloud_ota_result_t flush_chunk(cloud_ota_t *cloud)
{
    cloud_ota_result_t result;

    if (cloud->chunk_length == 0U) {
        return CLOUD_OTA_OK;
    }
    result = submit(cloud, OTA_CMD_DATA, cloud->chunk,
                    (uint16_t)cloud->chunk_length);
    if (result == CLOUD_OTA_OK) {
        cloud->chunk_length = 0U;
    }
    return result;
}

cloud_ota_result_t cloud_ota_begin(cloud_ota_t *cloud, bl_update_t *update,
                                   size_t remote_size,
                                   const ota_version_t *expected_version)
{
    if (cloud == NULL || update == NULL || expected_version == NULL) {
        return CLOUD_OTA_INVALID;
    }
    memset(cloud, 0, sizeof(*cloud));
    if (remote_size <= sizeof(ota_firmware_header_t) ||
        remote_size > sizeof(ota_firmware_header_t) + OTA_SLOT_PAYLOAD_SIZE) {
        return CLOUD_OTA_SIZE;
    }
    cloud->update = update;
    cloud->remote_size = remote_size;
    cloud->expected_version = *expected_version;
    return CLOUD_OTA_OK;
}

cloud_ota_result_t cloud_ota_write(cloud_ota_t *cloud,
                                   const unsigned char *data, size_t length)
{
    if (cloud == NULL || cloud->update == NULL ||
        (data == NULL && length != 0U) ||
        length > cloud->remote_size - cloud->package_received) {
        return CLOUD_OTA_SIZE;
    }
    while (length != 0U) {
        if (cloud->header_received < sizeof(cloud->header)) {
            size_t needed = sizeof(cloud->header) - cloud->header_received;
            size_t accepted = length < needed ? length : needed;
            memcpy((uint8_t *)&cloud->header + cloud->header_received, data,
                   accepted);
            cloud->header_received += accepted;
            cloud->package_received += accepted;
            data += accepted;
            length -= accepted;
            if (cloud->header_received == sizeof(cloud->header)) {
                cloud_ota_result_t result = start_from_header(cloud);
                if (result != CLOUD_OTA_OK) {
                    return result;
                }
            }
        } else {
            size_t space = sizeof(cloud->chunk) - cloud->chunk_length;
            size_t accepted = length < space ? length : space;
            memcpy(cloud->chunk + cloud->chunk_length, data, accepted);
            cloud->chunk_length += accepted;
            cloud->package_received += accepted;
            data += accepted;
            length -= accepted;
            if (cloud->chunk_length == sizeof(cloud->chunk) &&
                flush_chunk(cloud) != CLOUD_OTA_OK) {
                return CLOUD_OTA_ENGINE;
            }
        }
    }
    return CLOUD_OTA_OK;
}

cloud_ota_result_t cloud_ota_finish(cloud_ota_t *cloud)
{
    if (cloud == NULL || cloud->update == NULL || cloud->started == 0 ||
        cloud->package_received != cloud->remote_size) {
        return CLOUD_OTA_TRUNCATED;
    }
    if (flush_chunk(cloud) != CLOUD_OTA_OK ||
        submit(cloud, OTA_CMD_END_UPDATE, NULL, 0U) != CLOUD_OTA_OK) {
        return CLOUD_OTA_ENGINE;
    }
    return CLOUD_OTA_READY_TO_REBOOT;
}

void cloud_ota_abort(cloud_ota_t *cloud)
{
    if (cloud != NULL && cloud->update != NULL && cloud->started != 0 &&
        cloud->update->state == BL_UPDATE_RECEIVING) {
        (void)submit(cloud, OTA_CMD_ABORT, NULL, 0U);
        cloud->started = 0;
        cloud->chunk_length = 0U;
    }
}

unsigned cloud_ota_progress(const cloud_ota_t *cloud)
{
    if (cloud == NULL || cloud->remote_size == 0U) {
        return 0U;
    }
    return (unsigned)((cloud->package_received * 100U) / cloud->remote_size);
}
