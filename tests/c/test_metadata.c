#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_metadata.h"

#define CHECK(condition) do { if (!(condition)) { (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); return 1; } } while (0)

typedef struct { ota_metadata_record_t copies[2]; unsigned fail_erase; unsigned fail_program; unsigned fail_read; } fake_flash_t;

static ota_metadata_result_t read_copy(void *context, unsigned copy, ota_metadata_record_t *record)
{
    fake_flash_t *flash = context;
    if (flash->fail_read != 0u) return OTA_METADATA_IO_ERROR;
    *record = flash->copies[copy];
    return OTA_METADATA_OK;
}
static ota_metadata_result_t erase_copy(void *context, unsigned copy)
{
    fake_flash_t *flash = context;
    if (flash->fail_erase != 0u) return OTA_METADATA_IO_ERROR;
    (void)memset(&flash->copies[copy], 0xff, sizeof(flash->copies[copy]));
    return OTA_METADATA_OK;
}
static ota_metadata_result_t program_copy(void *context, unsigned copy, const ota_metadata_record_t *record)
{
    fake_flash_t *flash = context;
    if (flash->fail_program != 0u) return OTA_METADATA_IO_ERROR;
    flash->copies[copy] = *record;
    return OTA_METADATA_OK;
}
static ota_metadata_record_t valid(unsigned generation)
{
    ota_metadata_record_t record;
    (void)memset(&record, 0, sizeof(record));
    record.magic = OTA_METADATA_MAGIC;
    record.schema_version = OTA_METADATA_SCHEMA_VERSION;
    record.generation = generation;
    record.slot_a.state = OTA_SLOT_ACTIVE;
    record.slot_b.state = OTA_SLOT_VALID;
    record.active_slot = OTA_SLOT_A;
    record.pending_slot = OTA_SLOT_NONE;
    ota_metadata_finalize(&record);
    return record;
}

int main(void)
{
    fake_flash_t flash = {0};
    ota_metadata_io_t io = {read_copy, erase_copy, program_copy, &flash};
    ota_metadata_record_t loaded;
    ota_metadata_record_t next;
    unsigned selected = 0u;
    flash.copies[0] = valid(4u);
    flash.copies[1] = valid(5u);
    CHECK(ota_metadata_load(&io, &loaded, &selected) == OTA_METADATA_OK);
    CHECK(selected == 1u && loaded.generation == 5u);
    flash.copies[1].record_crc32 ^= UINT32_C(1);
    CHECK(ota_metadata_load(&io, &loaded, &selected) == OTA_METADATA_OK);
    CHECK(selected == 0u);
    flash.copies[0] = valid(UINT32_C(0xfffffffe));
    flash.copies[1] = valid(1u);
    CHECK(ota_metadata_load(&io, &loaded, &selected) == OTA_METADATA_OK);
    CHECK(selected == 1u);

    flash.copies[0] = valid(4u);
    flash.copies[1] = valid(5u);
    next = flash.copies[1];
    CHECK(ota_metadata_commit(&io, &next, 1u) == OTA_METADATA_OK);
    CHECK(flash.copies[0].generation == 6u);
    CHECK(ota_metadata_record_valid(&flash.copies[0]));
    flash.fail_program = 1u;
    CHECK(ota_metadata_commit(&io, &next, 0u) == OTA_METADATA_IO_ERROR);
    CHECK(ota_metadata_record_valid(&flash.copies[0]));
    return 0;
}
