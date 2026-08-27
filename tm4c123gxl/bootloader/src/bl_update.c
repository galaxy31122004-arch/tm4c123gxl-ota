#include "bl_update.h"

#include <string.h>

#include "ota_crc32.h"

static void put_u16(uint8_t *p, uint16_t value);
static void put_u32(uint8_t *p, uint32_t value);

static uint32_t slot_start(ota_slot_t slot)
{
    return slot == OTA_SLOT_A ? OTA_SLOT_A_START : OTA_SLOT_B_START;
}

static uint32_t slot_payload_start(ota_slot_t slot)
{
    return slot == OTA_SLOT_A ? OTA_SLOT_A_PAYLOAD_START : OTA_SLOT_B_PAYLOAD_START;
}

static ota_slot_record_t *slot_record(ota_metadata_record_t *record, ota_slot_t slot)
{
    return slot == OTA_SLOT_A ? &record->slot_a : &record->slot_b;
}

static void nack(ota_packet_t *response, const ota_packet_t *request, bl_update_error_t error)
{
    response->command = OTA_CMD_NACK;
    response->sequence = request->sequence;
    response->length = 3u;
    response->payload[0] = request->command;
    response->payload[1] = (uint8_t)request->sequence;
    response->payload[2] = (uint8_t)error;
}

static void ack(ota_packet_t *response, const ota_packet_t *request)
{
    response->command = OTA_CMD_ACK;
    response->sequence = request->sequence;
    response->length = 1u;
    response->payload[0] = request->command;
}

static void fail(bl_update_t *update, ota_packet_t *response, const ota_packet_t *request, bl_update_error_t error)
{
    update->last_error = error;
    nack(response, request, error);
}

static int read_payload_crc(const bl_update_t *update, ota_slot_t slot, uint32_t size, uint32_t *result)
{
    uint8_t buffer[OTA_PROTOCOL_MAX_PAYLOAD_SIZE];
    uint32_t address = slot_payload_start(slot);
    uint32_t remaining = size;
    uint32_t crc = ota_crc32_init();
    while (remaining != 0u) {
        const size_t length = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        if (update->services.read(update->services.context, address, buffer, length) != 0) return 0;
        crc = ota_crc32_update(crc, buffer, length);
        address += (uint32_t)length;
        remaining -= (uint32_t)length;
    }
    *result = ota_crc32_finish(crc);
    return 1;
}

void bl_update_init(bl_update_t *update, const bl_services_t *services)
{
    if (update == NULL || services == NULL) return;
    (void)memset(update, 0, sizeof(*update));
    update->services = *services;
    update->state = BL_UPDATE_IDLE;
}

void bl_update_handle(bl_update_t *update, const ota_packet_t *request, ota_packet_t *response)
{
    ota_slot_t target;
    uint32_t crc;
    uint8_t readback[OTA_PROTOCOL_MAX_PAYLOAD_SIZE];

    if (update == NULL || request == NULL || response == NULL) return;
    (void)memset(response, 0, sizeof(*response));
    if (request->command == OTA_CMD_GET_INFO) {
        response->command = OTA_CMD_ACK; response->sequence = request->sequence;
        response->length = 33u;
        response->payload[0] = OTA_CMD_GET_INFO;
        put_u16(&response->payload[1], 1u); put_u16(&response->payload[3], 0u); put_u16(&response->payload[5], 0u);
        put_u16(&response->payload[7], update->services.metadata.slot_a.version.major); put_u16(&response->payload[9], update->services.metadata.slot_a.version.minor); put_u16(&response->payload[11], update->services.metadata.slot_a.version.patch);
        put_u16(&response->payload[13], update->services.metadata.slot_b.version.major); put_u16(&response->payload[15], update->services.metadata.slot_b.version.minor); put_u16(&response->payload[17], update->services.metadata.slot_b.version.patch);
        response->payload[19] = update->services.metadata.slot_a.state; response->payload[20] = update->services.metadata.slot_b.state;
        response->payload[21] = update->services.metadata.active_slot; response->payload[22] = update->services.metadata.pending_slot;
        response->payload[23] = update->services.metadata.pending_slot == OTA_SLOT_A ? (uint8_t)update->services.metadata.slot_a.boot_count : (uint8_t)update->services.metadata.slot_b.boot_count;
        response->payload[24] = OTA_PROTOCOL_VERSION;
        put_u32(&response->payload[25], OTA_SLOT_PAYLOAD_SIZE); put_u32(&response->payload[29], update->received);
        return;
    }
    if (request->command == OTA_CMD_RESET) {
        ack(response, request);
        if (update->services.reset != NULL) update->services.reset(update->services.context);
        return;
    }
    if (request->command == OTA_CMD_ABORT) {
        if (update->state != BL_UPDATE_RECEIVING) { fail(update, response, request, BL_ERROR_STATE); return; }
        update->state = BL_UPDATE_IDLE; update->received = 0u; update->have_last = 0u; ack(response, request); return;
    }
    if (request->command == OTA_CMD_START_UPDATE) {
        if (request->length != sizeof(ota_firmware_header_t)) { fail(update, response, request, BL_ERROR_HEADER); return; }
        (void)memcpy(&update->header, request->payload, sizeof(update->header));
        target = (ota_slot_t)update->header.target_slot;
        if (ota_header_validate(&update->header, target) != OTA_IMAGE_OK) { fail(update, response, request, BL_ERROR_HEADER); return; }
        if (target == update->services.metadata.active_slot) { fail(update, response, request, BL_ERROR_SLOT); return; }
        /* Invalidate the header immediately.  Erase payload pages lazily so
         * START_UPDATE can acknowledge before the host transport times out. */
        if (update->services.erase == NULL || update->services.program == NULL || update->services.read == NULL ||
            update->services.erase(update->services.context, slot_start(target), OTA_FLASH_PAGE_SIZE, update->services.metadata.active_slot) != 0) {
            fail(update, response, request, BL_ERROR_FLASH); return;
        }
        update->state = BL_UPDATE_RECEIVING; update->received = 0u; update->next_sequence = request->sequence + 1u;
        update->have_last = 0u; update->last_error = BL_ERROR_NONE; ack(response, request); return;
    }
    if (request->command == OTA_CMD_DATA) {
        if (update->state != BL_UPDATE_RECEIVING) { fail(update, response, request, BL_ERROR_STATE); return; }
        crc = ota_crc32(request->payload, request->length);
        if (update->have_last && request->sequence == update->last_sequence && request->length == update->last_length && crc == update->last_crc) { ack(response, request); return; }
        if (request->sequence != update->next_sequence || request->length == 0u || request->length > update->header.payload_size - update->received) { fail(update, response, request, BL_ERROR_SEQUENCE); return; }
        target = (ota_slot_t)update->header.target_slot;
        if (((update->received % OTA_FLASH_PAGE_SIZE) == 0u &&
             update->services.erase(update->services.context, slot_payload_start(target) + update->received, OTA_FLASH_PAGE_SIZE, update->services.metadata.active_slot) != 0) ||
            update->services.program(update->services.context, slot_payload_start(target) + update->received, request->payload, request->length, update->services.metadata.active_slot) != 0 ||
            update->services.read(update->services.context, slot_payload_start(target) + update->received, readback, request->length) != 0 ||
            memcmp(readback, request->payload, request->length) != 0) { fail(update, response, request, BL_ERROR_FLASH); return; }
        update->received += request->length; update->last_sequence = request->sequence; update->last_length = request->length; update->last_crc = crc;
        update->have_last = 1u; ++update->next_sequence; ack(response, request); return;
    }
    if (request->command == OTA_CMD_END_UPDATE) {
        target = (ota_slot_t)update->header.target_slot;
        if (update->state != BL_UPDATE_RECEIVING || update->received != update->header.payload_size) { fail(update, response, request, BL_ERROR_STATE); return; }
        if (!read_payload_crc(update, target, update->received, &crc) || crc != update->header.payload_crc32) { fail(update, response, request, BL_ERROR_VERIFY); return; }
        /* Validate the actual vector table before committing the header page. */
        if (update->services.read(update->services.context, slot_payload_start(target), readback, 8u) != 0 ||
            ota_vector_validate((uint32_t)readback[0] | ((uint32_t)readback[1] << 8u) | ((uint32_t)readback[2] << 16u) | ((uint32_t)readback[3] << 24u),
                                (uint32_t)readback[4] | ((uint32_t)readback[5] << 8u) | ((uint32_t)readback[6] << 16u) | ((uint32_t)readback[7] << 24u), target) != OTA_IMAGE_OK ||
            update->services.program(update->services.context, slot_start(target), (const uint8_t *)&update->header, sizeof(update->header), update->services.metadata.active_slot) != 0) { fail(update, response, request, BL_ERROR_VERIFY); return; }
        *slot_record(&update->services.metadata, target) = (ota_slot_record_t){ OTA_SLOT_PENDING, {0u,0u,0u}, update->header.version, 0u, update->header.payload_size, update->header.payload_crc32, 0u };
        update->services.metadata.pending_slot = (uint8_t)target;
        ota_metadata_finalize(&update->services.metadata);
        if (ota_metadata_commit(&update->services.metadata_io, &update->services.metadata, update->services.metadata_copy) != OTA_METADATA_OK) { fail(update, response, request, BL_ERROR_FLASH); return; }
        update->state = BL_UPDATE_READY_TO_BOOT; ack(response, request); return;
    }
    fail(update, response, request, BL_ERROR_COMMAND);
}

void bl_update_poll(bl_update_t *update, uint32_t now_ms)
{
    if (update != NULL && update->state == BL_UPDATE_RECEIVING && now_ms - update->last_activity_ms >= OTA_PACKET_TIMEOUT_MS) {
        update->state = BL_UPDATE_IDLE; update->last_error = BL_ERROR_TIMEOUT; update->have_last = 0u;
    }
}

static void put_u16(uint8_t *p, uint16_t value) { p[0] = (uint8_t)value; p[1] = (uint8_t)(value >> 8u); }
static void put_u32(uint8_t *p, uint32_t value) { p[0]=(uint8_t)value; p[1]=(uint8_t)(value>>8u); p[2]=(uint8_t)(value>>16u); p[3]=(uint8_t)(value>>24u); }

void bl_update_note_activity(bl_update_t *update, uint32_t now_ms)
{
    if (update != NULL) update->last_activity_ms = now_ms;
}
