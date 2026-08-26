#ifndef OTA_BOOT_H
#define OTA_BOOT_H

#include "ota_config.h"
#include "ota_metadata.h"

typedef struct { ota_slot_t slot; ota_version_t version; } ota_confirmation_t;
typedef int (*ota_image_probe_fn)(void *context, ota_slot_t slot);
typedef enum { OTA_BOOT_STAY = 0, OTA_BOOT_SLOT_A, OTA_BOOT_SLOT_B } ota_boot_decision_t;
typedef struct { ota_boot_decision_t decision; int commit_required; } ota_boot_result_t;

ota_boot_result_t ota_boot_decide(ota_metadata_record_t *record, const ota_confirmation_t *confirmation, ota_image_probe_fn probe, void *context);

#endif
