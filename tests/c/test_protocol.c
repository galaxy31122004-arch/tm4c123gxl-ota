#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_protocol.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    ota_packet_t source = {0};
    ota_packet_t decoded = {0};
    ota_parser_t parser;
    uint8_t frame[268];
    size_t written = 0u;
    size_t i;
    ota_parse_result_t result = OTA_PARSE_MORE;
    source.command = OTA_CMD_GET_INFO;
    source.sequence = UINT16_C(0x1234);
    CHECK(ota_packet_encode(&source, frame, sizeof(frame), &written) == OTA_PROTOCOL_OK);
    CHECK(written == 12u);
    CHECK(frame[0] == 0x55u && frame[1] == 0xAAu && frame[2] == 1u);
    CHECK(frame[4] == 0x34u && frame[5] == 0x12u);
    ota_parser_init(&parser);
    (void)ota_parser_consume(&parser, 0x99u, 0u, &decoded);
    for (i = 0u; i < written; ++i) result = ota_parser_consume(&parser, frame[i], (uint32_t)i + 1u, &decoded);
    CHECK(result == OTA_PARSE_PACKET);
    CHECK(decoded.command == source.command && decoded.sequence == source.sequence && decoded.length == 0u);
    source.command = OTA_CMD_DATA;
    source.length = OTA_PROTOCOL_MAX_PAYLOAD_SIZE;
    for (i = 0u; i < source.length; ++i) source.payload[i] = (uint8_t)i;
    CHECK(ota_packet_encode(&source, frame, sizeof(frame), &written) == OTA_PROTOCOL_OK);
    ota_parser_init(&parser);
    for (i = 0u; i < written; ++i) result = ota_parser_consume(&parser, frame[i], (uint32_t)i, &decoded);
    CHECK(result == OTA_PARSE_PACKET);
    CHECK(memcmp(decoded.payload, source.payload, source.length) == 0);
    frame[written - 1u] ^= 1u;
    ota_parser_init(&parser);
    for (i = 0u; i < written; ++i) result = ota_parser_consume(&parser, frame[i], (uint32_t)i, &decoded);
    CHECK(result == OTA_PARSE_BAD_CRC);
    ota_parser_init(&parser);
    CHECK(ota_parser_consume(&parser, 0x55u, 1u, &decoded) == OTA_PARSE_MORE);
    CHECK(ota_parser_consume(&parser, 0xAAu, OTA_PACKET_TIMEOUT_MS + 2u, &decoded) == OTA_PARSE_TIMEOUT);
    return 0;
}
