#include <string.h>

#include "bl_hal.h"
#include "bl_main.h"
#include "bl_update.h"
#include "ota_boot.h"
#include "boot_confirm.h"

extern void bl_hal_init(void);
extern void bl_jump_to_image(uint32_t vector_address);

static int flash_read(void *context, uint32_t address, uint8_t *destination, size_t length)
{ (void)context; return bl_hal_flash_read(address, destination, length) == OTA_ERROR_NONE ? 0 : -1; }
static int flash_erase(void *context, uint32_t address, size_t length, ota_slot_t active)
{ (void)context; return bl_hal_flash_erase(address, length, active) == OTA_ERROR_NONE ? 0 : -1; }
static int flash_program(void *context, uint32_t address, const uint8_t *source, size_t length, ota_slot_t active)
{ (void)context; return bl_hal_flash_program(address, source, length, active) == OTA_ERROR_NONE ? 0 : -1; }
static void reset_target(void *context)
{ (void)context; bl_hal_uart_wait_tx_complete(); bl_hal_reset(); }
static ota_metadata_result_t metadata_read(void *c, unsigned copy, ota_metadata_record_t *r)
{ return flash_read(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, (uint8_t *)r, sizeof(*r)) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static ota_metadata_result_t metadata_erase(void *c, unsigned copy)
{ return flash_erase(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, OTA_METADATA_COPY_SIZE, OTA_SLOT_NONE) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static ota_metadata_result_t metadata_program(void *c, unsigned copy, const ota_metadata_record_t *r)
{ return flash_program(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, (const uint8_t *)r, sizeof(*r), OTA_SLOT_NONE) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static int image_probe(void *context, ota_slot_t slot)
{ ota_image_info_t info; return ota_image_validate((ota_reader_t){ flash_read, context }, slot, &info) == OTA_IMAGE_OK; }
static void set_slot_from_image(ota_slot_record_t *record, const ota_image_info_t *info, ota_slot_state_t state)
{ (void)memset(record, 0, sizeof(*record)); record->state = (uint8_t)state; record->version = info->header.version; record->payload_size = info->header.payload_size; record->payload_crc32 = info->header.payload_crc32; }
static void reconstruct_metadata(bl_services_t *services)
{
    ota_image_info_t a; ota_image_info_t b;
    int valid_a = ota_image_validate((ota_reader_t){ flash_read, NULL }, OTA_SLOT_A, &a) == OTA_IMAGE_OK;
    int valid_b = ota_image_validate((ota_reader_t){ flash_read, NULL }, OTA_SLOT_B, &b) == OTA_IMAGE_OK;
    (void)memset(&services->metadata, 0, sizeof(services->metadata));
    services->metadata.magic = OTA_METADATA_MAGIC; services->metadata.schema_version = OTA_METADATA_SCHEMA_VERSION;
    services->metadata.active_slot = OTA_SLOT_NONE; services->metadata.pending_slot = OTA_SLOT_NONE;
    if (valid_a) { set_slot_from_image(&services->metadata.slot_a, &a, OTA_SLOT_ACTIVE); services->metadata.active_slot = OTA_SLOT_A; }
    else if (valid_b) { set_slot_from_image(&services->metadata.slot_b, &b, OTA_SLOT_ACTIVE); services->metadata.active_slot = OTA_SLOT_B; }
    if (valid_b && services->metadata.active_slot != OTA_SLOT_B) set_slot_from_image(&services->metadata.slot_b, &b, OTA_SLOT_VALID);
    ota_metadata_finalize(&services->metadata); services->metadata_copy = 0u;
    if (ota_metadata_commit(&services->metadata_io, &services->metadata, 1u) == OTA_METADATA_OK) {
        ++services->metadata.generation;
        ota_metadata_finalize(&services->metadata);
    }
}
static void uart_response(const ota_packet_t *packet)
{
    uint8_t frame[268]; size_t length; size_t index;
    if (ota_packet_encode(packet, frame, sizeof(frame), &length) == OTA_PROTOCOL_OK)
        for (index = 0u; index < length; ++index) bl_hal_uart1_write(frame[index]);
}
void bl_services_init(bl_services_t *services)
{
    if (services == NULL) return;
    (void)memset(services, 0, sizeof(*services));
    services->read = flash_read;
    services->erase = flash_erase;
    services->program = flash_program;
    services->metadata_io = (ota_metadata_io_t){ metadata_read, metadata_erase, metadata_program, NULL };
    services->reset = reset_target;
}
void bl_main(void)
{
    ota_metadata_record_t metadata; unsigned selected = 0u; bl_services_t services; bl_update_t update; ota_parser_t parser; ota_packet_t request, response;
    ota_confirmation_t confirmation; ota_confirmation_t *confirmation_ptr = NULL; ota_slot_t confirmed_slot; ota_version_t confirmed_version;
    ota_boot_result_t boot_result; uint32_t update_window_start;
    bl_hal_init();
    bl_hal_uart0_log("BL_READY\r\n");
    bl_services_init(&services);
    if (ota_metadata_load(&services.metadata_io, &metadata, &selected) == OTA_METADATA_OK) { services.metadata = metadata; services.metadata_copy = selected; }
    else reconstruct_metadata(&services);
    if (boot_confirmation_consume(&confirmed_slot, &confirmed_version) != 0) {
        confirmation.slot = confirmed_slot; confirmation.version = confirmed_version; confirmation_ptr = &confirmation;
    }
    boot_result = ota_boot_decide(&services.metadata, confirmation_ptr, image_probe, NULL);
    if (boot_result.commit_required) { ota_metadata_finalize(&services.metadata); (void)ota_metadata_commit(&services.metadata_io, &services.metadata, services.metadata_copy); }
    bl_update_init(&update, &services); ota_parser_init(&parser); update_window_start = bl_hal_millis();
    for (;;) { uint8_t byte; uint32_t now = bl_hal_millis(); bl_update_poll(&update, now); bl_hal_watchdog_service();
        if (bl_hal_uart1_read(&byte, 10u) != 0 && ota_parser_consume(&parser, byte, now, &request) == OTA_PARSE_PACKET) { bl_update_handle(&update, &request, &response); bl_update_note_activity(&update, bl_hal_millis()); uart_response(&response); if (update.state == BL_UPDATE_READY_TO_BOOT) { boot_confirmation_clear(); bl_hal_uart_wait_tx_complete(); bl_hal_reset(); } }
        if (update.state == BL_UPDATE_IDLE && (uint32_t)(now - update_window_start) >= UINT32_C(60000)) {
            if (boot_result.decision == OTA_BOOT_SLOT_A) bl_jump_to_image(OTA_SLOT_A_PAYLOAD_START);
            if (boot_result.decision == OTA_BOOT_SLOT_B) bl_jump_to_image(OTA_SLOT_B_PAYLOAD_START);
            update_window_start = now;
        }
    }
}
int main(void) { bl_main(); return 0; }
