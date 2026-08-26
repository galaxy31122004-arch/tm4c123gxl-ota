#ifndef OTA_METADATA_H
#define OTA_METADATA_H

#include "ota_types.h"

#define OTA_METADATA_MAGIC UINT32_C(0x4F54414D)
#define OTA_METADATA_SCHEMA_VERSION UINT16_C(1)

typedef enum { OTA_METADATA_OK = 0, OTA_METADATA_INVALID_ARGUMENT, OTA_METADATA_NO_VALID_COPY, OTA_METADATA_IO_ERROR, OTA_METADATA_VERIFY_ERROR } ota_metadata_result_t;
typedef ota_metadata_result_t (*ota_metadata_read_fn)(void *, unsigned, ota_metadata_record_t *);
typedef ota_metadata_result_t (*ota_metadata_erase_fn)(void *, unsigned);
typedef ota_metadata_result_t (*ota_metadata_program_fn)(void *, unsigned, const ota_metadata_record_t *);
typedef struct { ota_metadata_read_fn read; ota_metadata_erase_fn erase; ota_metadata_program_fn program; void *context; } ota_metadata_io_t;

int ota_metadata_record_valid(const ota_metadata_record_t *record);
void ota_metadata_finalize(ota_metadata_record_t *record);
ota_metadata_result_t ota_metadata_load(const ota_metadata_io_t *io, ota_metadata_record_t *record, unsigned *selected_copy);
ota_metadata_result_t ota_metadata_commit(const ota_metadata_io_t *io, const ota_metadata_record_t *record, unsigned current_copy);

#endif
