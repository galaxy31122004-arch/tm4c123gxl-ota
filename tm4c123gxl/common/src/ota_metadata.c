#include "ota_metadata.h"

#include <string.h>

#include "ota_crc32.h"

static int slot_is_valid(uint8_t slot) { return (slot == OTA_SLOT_A) || (slot == OTA_SLOT_B) || (slot == OTA_SLOT_NONE); }
static int state_is_valid(uint8_t state) { return state <= OTA_SLOT_FAILED; }
static uint32_t record_crc(const ota_metadata_record_t *record)
{
    ota_metadata_record_t copy = *record;
    copy.record_crc32 = 0u;
    return ota_crc32((const uint8_t *)&copy, sizeof(copy));
}
static int newer(uint32_t left, uint32_t right) { return (int32_t)(left - right) > 0; }

int ota_metadata_record_valid(const ota_metadata_record_t *record)
{
    if ((record == NULL) || (record->magic != OTA_METADATA_MAGIC) || (record->schema_version != OTA_METADATA_SCHEMA_VERSION) ||
        (record->reserved0 != 0u) || !state_is_valid(record->slot_a.state) || !state_is_valid(record->slot_b.state) ||
        !slot_is_valid(record->active_slot) || !slot_is_valid(record->pending_slot) ||
        (memcmp(record->reserved, (const uint8_t[12]){0}, sizeof(record->reserved)) != 0) ||
        (memcmp(record->slot_a.reserved0, (const uint8_t[3]){0}, sizeof(record->slot_a.reserved0)) != 0) ||
        (record->slot_a.reserved1 != 0u) ||
        (memcmp(record->slot_b.reserved0, (const uint8_t[3]){0}, sizeof(record->slot_b.reserved0)) != 0) ||
        (record->slot_b.reserved1 != 0u)) return 0;
    return record->record_crc32 == record_crc(record);
}

void ota_metadata_finalize(ota_metadata_record_t *record) { if (record != NULL) record->record_crc32 = record_crc(record); }

ota_metadata_result_t ota_metadata_load(const ota_metadata_io_t *io, ota_metadata_record_t *record, unsigned *selected_copy)
{
    ota_metadata_record_t copies[2];
    int valid[2] = {0, 0};
    unsigned index;
    if ((io == NULL) || (io->read == NULL) || (record == NULL) || (selected_copy == NULL)) return OTA_METADATA_INVALID_ARGUMENT;
    for (index = 0u; index < 2u; ++index) {
        if (io->read(io->context, index, &copies[index]) == OTA_METADATA_OK) valid[index] = ota_metadata_record_valid(&copies[index]);
    }
    if ((valid[0] == 0) && (valid[1] == 0)) return OTA_METADATA_NO_VALID_COPY;
    index = ((valid[1] != 0) && ((valid[0] == 0) || newer(copies[1].generation, copies[0].generation))) ? 1u : 0u;
    *record = copies[index]; *selected_copy = index;
    return OTA_METADATA_OK;
}

ota_metadata_result_t ota_metadata_commit(const ota_metadata_io_t *io, const ota_metadata_record_t *record, unsigned current_copy)
{
    ota_metadata_record_t next;
    ota_metadata_record_t verified;
    unsigned destination;
    if ((io == NULL) || (io->read == NULL) || (io->erase == NULL) || (io->program == NULL) || (record == NULL) || (current_copy > 1u)) return OTA_METADATA_INVALID_ARGUMENT;
    destination = current_copy ^ 1u;
    next = *record; next.generation = record->generation + 1u; ota_metadata_finalize(&next);
    if (io->erase(io->context, destination) != OTA_METADATA_OK) return OTA_METADATA_IO_ERROR;
    if (io->program(io->context, destination, &next) != OTA_METADATA_OK) return OTA_METADATA_IO_ERROR;
    if (io->read(io->context, destination, &verified) != OTA_METADATA_OK) return OTA_METADATA_IO_ERROR;
    if ((memcmp(&next, &verified, sizeof(next)) != 0) || !ota_metadata_record_valid(&verified)) return OTA_METADATA_VERIFY_ERROR;
    return OTA_METADATA_OK;
}
