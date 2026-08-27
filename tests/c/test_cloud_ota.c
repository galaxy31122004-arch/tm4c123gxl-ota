#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cloud_ota.h"
#include "ota_crc32.h"

#define FLASH_BYTES OTA_FLASH_END

typedef struct {
    uint8_t memory[FLASH_BYTES];
    ota_metadata_record_t copies[2];
    unsigned erases;
} fake_t;

static int rd(void *c, uint32_t a, uint8_t *d, size_t n)
{ memcpy(d, ((fake_t *)c)->memory + a, n); return 0; }
static int er(void *c, uint32_t a, size_t n, ota_slot_t active)
{ (void)active; ++((fake_t *)c)->erases; memset(((fake_t *)c)->memory + a, 0xff, n); return 0; }
static int wr(void *c, uint32_t a, const uint8_t *s, size_t n, ota_slot_t active)
{ (void)active; memcpy(((fake_t *)c)->memory + a, s, n); return 0; }
static ota_metadata_result_t mr(void *c, unsigned i, ota_metadata_record_t *r)
{ *r = ((fake_t *)c)->copies[i]; return OTA_METADATA_OK; }
static ota_metadata_result_t me(void *c, unsigned i)
{ memset(&((fake_t *)c)->copies[i], 0xff, sizeof(ota_metadata_record_t)); return OTA_METADATA_OK; }
static ota_metadata_result_t mp(void *c, unsigned i, const ota_metadata_record_t *r)
{ ((fake_t *)c)->copies[i] = *r; return OTA_METADATA_OK; }

static void setup(fake_t *fake, bl_update_t *update)
{
    bl_services_t services;
    memset(fake, 0xff, sizeof(*fake));
    fake->erases = 0U;
    memset(&services, 0, sizeof(services));
    services.read = rd; services.erase = er; services.program = wr;
    services.context = fake;
    services.metadata_io = (ota_metadata_io_t){mr, me, mp, fake};
    services.metadata.magic = OTA_METADATA_MAGIC;
    services.metadata.schema_version = OTA_METADATA_SCHEMA_VERSION;
    services.metadata.slot_a.state = OTA_SLOT_ACTIVE;
    services.metadata.active_slot = OTA_SLOT_A;
    services.metadata.pending_slot = OTA_SLOT_NONE;
    ota_metadata_finalize(&services.metadata);
    bl_update_init(update, &services);
}

static ota_firmware_header_t make_header(const uint8_t *payload, size_t length)
{
    ota_firmware_header_t header = {0};
    header.magic = OTA_FIRMWARE_MAGIC;
    header.schema_version = OTA_FIRMWARE_SCHEMA_VERSION;
    header.target_slot = OTA_SLOT_B;
    header.version = (ota_version_t){1U, 2U, 3U};
    header.payload_size = (uint32_t)length;
    header.payload_crc32 = ota_crc32(payload, length);
    header.header_crc32 = ota_header_crc32(&header);
    return header;
}

static void test_success_and_truncated_stream(void)
{
    uint8_t payload[300] = {0x00U, 0x80U, 0x00U, 0x20U, 0x01U, 0x40U, 0x02U};
    uint8_t package[OTA_SLOT_HEADER_SIZE + sizeof(payload)];
    ota_firmware_header_t header = make_header(payload, sizeof(payload));
    fake_t fake;
    bl_update_t update;
    cloud_ota_t cloud;
    ota_version_t expected = {1U, 2U, 3U};

    memset(package, 0xff, sizeof(package));
    memcpy(package, &header, sizeof(header));
    memcpy(package + OTA_SLOT_HEADER_SIZE, payload, sizeof(payload));
    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update, sizeof(package), &expected) == CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, package, 17U) == CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, package + 17U, sizeof(package) - 17U) == CLOUD_OTA_OK);
    assert(cloud_ota_finish(&cloud) == CLOUD_OTA_READY_TO_REBOOT);
    assert(update.state == BL_UPDATE_READY_TO_BOOT);

    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update, sizeof(package), &expected) == CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, package, sizeof(package) - 1U) == CLOUD_OTA_OK);
    assert(cloud_ota_finish(&cloud) == CLOUD_OTA_TRUNCATED);
}

static void test_rejects_size_before_erase(void)
{
    fake_t fake;
    bl_update_t update;
    cloud_ota_t cloud;
    ota_version_t expected = {1U, 2U, 3U};

    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update,
                           OTA_SLOT_HEADER_SIZE +
                           OTA_SLOT_PAYLOAD_SIZE + 1U, &expected) == CLOUD_OTA_SIZE);
    assert(fake.erases == 0U);
}

static void test_rejects_unrequested_version_before_erase(void)
{
    uint8_t payload[8] = {0x00U, 0x80U, 0x00U, 0x20U, 0x01U, 0x40U, 0x02U};
    ota_firmware_header_t header = make_header(payload, sizeof(payload));
    ota_version_t expected = {1U, 2U, 4U};
    fake_t fake;
    bl_update_t update;
    cloud_ota_t cloud;

    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update,
                           OTA_SLOT_HEADER_SIZE + sizeof(payload), &expected) ==
           CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, (const unsigned char *)&header,
                           sizeof(header)) == CLOUD_OTA_HEADER);
    assert(fake.erases == 0U);
}

static void test_abort_and_bad_payload_retain_active_slot(void)
{
    uint8_t payload[16] = {0x00U, 0x80U, 0x00U, 0x20U, 0x01U, 0x40U, 0x02U};
    uint8_t package[OTA_SLOT_HEADER_SIZE + sizeof(payload)];
    ota_firmware_header_t header = make_header(payload, sizeof(payload));
    ota_version_t expected = {1U, 2U, 3U};
    fake_t fake;
    bl_update_t update;
    cloud_ota_t cloud;

    memset(package, 0xff, sizeof(package));
    memcpy(package, &header, sizeof(header));
    memcpy(package + OTA_SLOT_HEADER_SIZE, payload, sizeof(payload));
    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update, sizeof(package), &expected) ==
           CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, package, sizeof(header)) == CLOUD_OTA_OK);
    assert(update.state == BL_UPDATE_RECEIVING);
    cloud_ota_abort(&cloud);
    assert(update.state == BL_UPDATE_IDLE);
    assert(update.services.metadata.active_slot == OTA_SLOT_A);

    package[OTA_SLOT_HEADER_SIZE + 8U] ^= 0x5aU;
    setup(&fake, &update);
    assert(cloud_ota_begin(&cloud, &update, sizeof(package), &expected) ==
           CLOUD_OTA_OK);
    assert(cloud_ota_write(&cloud, package, sizeof(package)) == CLOUD_OTA_OK);
    assert(cloud_ota_finish(&cloud) == CLOUD_OTA_ENGINE);
    assert(update.services.metadata.active_slot == OTA_SLOT_A);
    assert(update.services.metadata.pending_slot == OTA_SLOT_NONE);
}

int main(void)
{
    test_success_and_truncated_stream();
    test_rejects_size_before_erase();
    test_rejects_unrequested_version_before_erase();
    test_abort_and_bad_payload_retain_active_slot();
    puts("cloud_ota tests passed");
    return 0;
}
