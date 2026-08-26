#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_boot.h"

#define CHECK(condition) do { if (!(condition)) { (void)fprintf(stderr, "CHECK failed: %s (%s:%d)\n", #condition, __FILE__, __LINE__); return 1; } } while (0)

static int probe(void *context, ota_slot_t slot) { const int *valid = context; return valid[(unsigned)slot]; }
static ota_metadata_record_t active_a_pending_b(unsigned attempts)
{
    ota_metadata_record_t record;
    (void)memset(&record, 0, sizeof(record));
    record.slot_a.state = OTA_SLOT_ACTIVE;
    record.slot_b.state = OTA_SLOT_PENDING;
    record.slot_b.boot_count = attempts;
    record.active_slot = OTA_SLOT_A;
    record.pending_slot = OTA_SLOT_B;
    return record;
}
int main(void)
{
    const int images[2] = {1, 1};
    ota_metadata_record_t record = active_a_pending_b(2u);
    ota_boot_result_t result;
    ota_confirmation_t confirmation = {OTA_SLOT_B, {1u, 0u, 0u}};
    result = ota_boot_decide(&record, NULL, probe, (void *)images);
    CHECK(result.decision == OTA_BOOT_SLOT_B && result.commit_required);
    CHECK(record.slot_b.boot_count == 3u);
    result = ota_boot_decide(&record, NULL, probe, (void *)images);
    CHECK(result.decision == OTA_BOOT_SLOT_A && result.commit_required);
    CHECK(record.slot_b.state == OTA_SLOT_FAILED && record.pending_slot == OTA_SLOT_NONE);
    record = active_a_pending_b(0u);
    record.slot_b.version = confirmation.version;
    result = ota_boot_decide(&record, &confirmation, probe, (void *)images);
    CHECK(result.decision == OTA_BOOT_SLOT_B && result.commit_required);
    CHECK(record.slot_b.state == OTA_SLOT_ACTIVE && record.slot_a.state == OTA_SLOT_VALID);
    CHECK(record.active_slot == OTA_SLOT_B && record.pending_slot == OTA_SLOT_NONE);
    return 0;
}
