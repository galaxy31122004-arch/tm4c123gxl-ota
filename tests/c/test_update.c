#include <stdio.h>
#include <string.h>

#include "bl_update.h"
#include "ota_crc32.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)
#define FLASH_BYTES (OTA_FLASH_END)
typedef struct { uint8_t memory[FLASH_BYTES]; unsigned writes; unsigned header_writes; ota_metadata_record_t copies[2]; } fake_t;
static int rd(void *c, uint32_t a, uint8_t *d, size_t n) { fake_t *f=c; if (a > FLASH_BYTES || n > FLASH_BYTES-a) return -1; memcpy(d, f->memory+a, n); return 0; }
static int er(void *c, uint32_t a, size_t n, ota_slot_t active) { fake_t *f=c; (void)active; memset(f->memory+a, 0xff, n); return 0; }
static int wr(void *c, uint32_t a, const uint8_t *s, size_t n, ota_slot_t active) { fake_t *f=c; (void)active; memcpy(f->memory+a,s,n); ++f->writes; if (a == OTA_SLOT_B_START) ++f->header_writes; return 0; }
static ota_metadata_result_t mr(void*c,unsigned i,ota_metadata_record_t*r){*r=((fake_t*)c)->copies[i];return OTA_METADATA_OK;}
static ota_metadata_result_t me(void*c,unsigned i){memset(&((fake_t*)c)->copies[i],0xff,sizeof(ota_metadata_record_t));return OTA_METADATA_OK;}
static ota_metadata_result_t mp(void*c,unsigned i,const ota_metadata_record_t*r){((fake_t*)c)->copies[i]=*r;return OTA_METADATA_OK;}
static ota_firmware_header_t header(const uint8_t *p, size_t n) { ota_firmware_header_t h={0}; h.magic=OTA_FIRMWARE_MAGIC; h.schema_version=OTA_FIRMWARE_SCHEMA_VERSION; h.target_slot=OTA_SLOT_B; h.version=(ota_version_t){1,0,0}; h.payload_size=(uint32_t)n; h.payload_crc32=ota_crc32(p,n); h.header_crc32=ota_header_crc32(&h); return h; }
static void call(bl_update_t *u,uint8_t cmd,uint16_t seq,const void *p,uint16_t n,ota_packet_t *r){ota_packet_t q={0};q.command=cmd;q.sequence=seq;q.length=n;if(n)memcpy(q.payload,p,n);bl_update_handle(u,&q,r);}
int main(void) {
 fake_t f; bl_services_t s; bl_update_t u; ota_packet_t r; uint8_t payload[16]={0,0x80,0,0x20,1,0x40,2,0}; ota_firmware_header_t h=header(payload,sizeof(payload));
 memset(&f,0xff,sizeof(f)); f.writes=0u; f.header_writes=0u; memset(&s,0,sizeof(s)); s.read=rd;s.erase=er;s.program=wr;s.context=&f;s.metadata_io=(ota_metadata_io_t){mr,me,mp,&f};s.metadata_copy=0;s.metadata.magic=OTA_METADATA_MAGIC;s.metadata.schema_version=OTA_METADATA_SCHEMA_VERSION;s.metadata.slot_a.state=OTA_SLOT_ACTIVE;s.metadata.active_slot=OTA_SLOT_A;s.metadata.pending_slot=OTA_SLOT_NONE;ota_metadata_finalize(&s.metadata);
 bl_update_init(&u,&s); call(&u,OTA_CMD_START_UPDATE,0,&h,sizeof(h),&r); CHECK(r.command==OTA_CMD_ACK && u.state==BL_UPDATE_RECEIVING);
 call(&u,OTA_CMD_DATA,1,payload,sizeof(payload),&r); CHECK(r.command==OTA_CMD_ACK && f.header_writes==0);
 { unsigned writes=f.writes; call(&u,OTA_CMD_DATA,1,payload,sizeof(payload),&r); CHECK(r.command==OTA_CMD_ACK && f.writes==writes); }
 call(&u,OTA_CMD_END_UPDATE,2,NULL,0,&r); CHECK(r.command==OTA_CMD_ACK && f.header_writes==1 && u.services.metadata.slot_b.state==OTA_SLOT_PENDING);
 h.target_slot = OTA_SLOT_A; h.header_crc32 = ota_header_crc32(&h); call(&u,OTA_CMD_START_UPDATE,3,&h,sizeof(h),&r); CHECK(r.command==OTA_CMD_NACK && r.payload[2]==BL_ERROR_SLOT);
 return 0;
}
