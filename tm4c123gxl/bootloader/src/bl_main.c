#include <string.h>

#include "bl_hal.h"
#include "bl_update.h"
#include "ota_boot.h"

extern void bl_hal_init(void);
extern void bl_jump_to_image(uint32_t vector_address);

static int flash_read(void *context, uint32_t address, uint8_t *destination, size_t length)
{ (void)context; return bl_hal_flash_read(address, destination, length) == OTA_ERROR_NONE ? 0 : -1; }
static int flash_erase(void *context, uint32_t address, size_t length, ota_slot_t active)
{ (void)context; return bl_hal_flash_erase(address, length, active) == OTA_ERROR_NONE ? 0 : -1; }
static int flash_program(void *context, uint32_t address, const uint8_t *source, size_t length, ota_slot_t active)
{ (void)context; return bl_hal_flash_program(address, source, length, active) == OTA_ERROR_NONE ? 0 : -1; }
static ota_metadata_result_t metadata_read(void *c, unsigned copy, ota_metadata_record_t *r)
{ return flash_read(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, (uint8_t *)r, sizeof(*r)) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static ota_metadata_result_t metadata_erase(void *c, unsigned copy)
{ return flash_erase(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, OTA_METADATA_COPY_SIZE, OTA_SLOT_NONE) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static ota_metadata_result_t metadata_program(void *c, unsigned copy, const ota_metadata_record_t *r)
{ return flash_program(c, copy == 0u ? OTA_METADATA_COPY0_START : OTA_METADATA_COPY1_START, (const uint8_t *)r, sizeof(*r), OTA_SLOT_NONE) == 0 ? OTA_METADATA_OK : OTA_METADATA_IO_ERROR; }
static int image_probe(void *context, ota_slot_t slot)
{ ota_image_info_t info; return ota_image_validate((ota_reader_t){ flash_read, context }, slot, &info) == OTA_IMAGE_OK; }
static void uart_response(const ota_packet_t *packet)
{
    uint8_t frame[268]; size_t length; size_t index;
    if (ota_packet_encode(packet, frame, sizeof(frame), &length) == OTA_PROTOCOL_OK)
        for (index = 0u; index < length; ++index) bl_hal_uart1_write(frame[index]);
}
void bl_main(void)
{
    ota_metadata_record_t metadata; unsigned selected = 0u; bl_services_t services; bl_update_t update; ota_parser_t parser; ota_packet_t request, response;
    bl_hal_init();
    (void)memset(&services, 0, sizeof(services));
    services.read = flash_read; services.erase = flash_erase; services.program = flash_program;
    services.metadata_io = (ota_metadata_io_t){ metadata_read, metadata_erase, metadata_program, NULL };
    if (ota_metadata_load(&services.metadata_io, &metadata, &selected) == OTA_METADATA_OK) { services.metadata = metadata; services.metadata_copy = selected; }
    bl_update_init(&update, &services); ota_parser_init(&parser);
    for (;;) { uint8_t byte; uint32_t now = bl_hal_millis(); bl_update_poll(&update, now); bl_hal_watchdog_service();
        if (bl_hal_uart1_read(&byte, 10u) != 0 && ota_parser_consume(&parser, byte, now, &request) == OTA_PARSE_PACKET) { bl_update_handle(&update, &request, &response); uart_response(&response); if (update.state == BL_UPDATE_READY_TO_BOOT) bl_hal_reset(); }
    }
}
int main(void) { bl_main(); return 0; }
