#include "ota_boot.h"

#include <string.h>

static ota_slot_record_t *slot_record(ota_metadata_record_t *record, ota_slot_t slot) { return (slot == OTA_SLOT_A) ? &record->slot_a : &record->slot_b; }
static ota_slot_t other_slot(ota_slot_t slot) { return (slot == OTA_SLOT_A) ? OTA_SLOT_B : OTA_SLOT_A; }
static ota_boot_decision_t decision_for(ota_slot_t slot) { return (slot == OTA_SLOT_A) ? OTA_BOOT_SLOT_A : OTA_BOOT_SLOT_B; }
static int version_equal(const ota_version_t *left, const ota_version_t *right) { return memcmp(left, right, sizeof(*left)) == 0; }
static int bootable(ota_image_probe_fn probe, void *context, ota_slot_t slot) { return (probe != NULL) && ((slot == OTA_SLOT_A) || (slot == OTA_SLOT_B)) && (probe(context, slot) != 0); }

ota_boot_result_t ota_boot_decide(ota_metadata_record_t *record, const ota_confirmation_t *confirmation, ota_image_probe_fn probe, void *context)
{
    ota_boot_result_t result = {OTA_BOOT_STAY, 0};
    ota_slot_t active;
    ota_slot_t pending;
    ota_slot_record_t *active_record;
    ota_slot_record_t *pending_record;
    if ((record == NULL) || (probe == NULL)) return result;
    active = (ota_slot_t)record->active_slot;
    pending = (ota_slot_t)record->pending_slot;
    if ((pending == OTA_SLOT_A) || (pending == OTA_SLOT_B)) {
        pending_record = slot_record(record, pending);
        if ((confirmation != NULL) && (confirmation->slot == pending) && version_equal(&confirmation->version, &pending_record->version) && bootable(probe, context, pending)) {
            active_record = slot_record(record, active);
            if ((active == OTA_SLOT_A) || (active == OTA_SLOT_B)) active_record->state = OTA_SLOT_VALID;
            pending_record->state = OTA_SLOT_ACTIVE; pending_record->boot_count = 0u; record->active_slot = (uint8_t)pending; record->pending_slot = OTA_SLOT_NONE;
            result.decision = decision_for(pending); result.commit_required = 1; return result;
        }
        if (!bootable(probe, context, pending) || (pending_record->boot_count >= OTA_PENDING_BOOT_MAX_ATTEMPTS)) {
            pending_record->state = OTA_SLOT_FAILED; record->pending_slot = OTA_SLOT_NONE; result.commit_required = 1;
        } else {
            ++pending_record->boot_count; result.decision = decision_for(pending); result.commit_required = 1; return result;
        }
    }
    active = (ota_slot_t)record->active_slot;
    if (bootable(probe, context, active)) { result.decision = decision_for(active); return result; }
    if ((other_slot(active) == OTA_SLOT_A) || (other_slot(active) == OTA_SLOT_B)) {
        ota_slot_t backup = other_slot(active);
        if (bootable(probe, context, backup)) { result.decision = decision_for(backup); return result; }
    }
    return result;
}
