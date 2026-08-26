#include "ota_protocol.h"

#include <string.h>

#include "ota_crc32.h"

static void put_u16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8u);
}

static uint16_t get_u16(const uint8_t *input)
{
    return (uint16_t)((uint16_t)input[0] | ((uint16_t)input[1] << 8u));
}

static uint32_t get_u32(const uint8_t *input)
{
    return (uint32_t)input[0] | ((uint32_t)input[1] << 8u) |
           ((uint32_t)input[2] << 16u) | ((uint32_t)input[3] << 24u);
}

ota_protocol_result_t ota_packet_encode(const ota_packet_t *packet,
                                        uint8_t *output,
                                        size_t capacity,
                                        size_t *written)
{
    size_t frame_size;
    uint32_t crc;
    if ((packet == NULL) || (output == NULL) || (written == NULL)) {
        return OTA_PROTOCOL_INVALID_ARGUMENT;
    }
    if (packet->length > OTA_PROTOCOL_MAX_PAYLOAD_SIZE) {
        return OTA_PROTOCOL_BAD_LENGTH;
    }
    frame_size = 12u + packet->length;
    if (capacity < frame_size) {
        return OTA_PROTOCOL_NO_SPACE;
    }
    output[0] = OTA_PROTOCOL_SOF0;
    output[1] = OTA_PROTOCOL_SOF1;
    output[2] = OTA_PROTOCOL_VERSION;
    output[3] = packet->command;
    put_u16(&output[4], packet->sequence);
    put_u16(&output[6], packet->length);
    if (packet->length != 0u) {
        (void)memcpy(&output[8], packet->payload, packet->length);
    }
    crc = ota_crc32(&output[2], 6u + packet->length);
    output[8u + packet->length] = (uint8_t)crc;
    output[9u + packet->length] = (uint8_t)(crc >> 8u);
    output[10u + packet->length] = (uint8_t)(crc >> 16u);
    output[11u + packet->length] = (uint8_t)(crc >> 24u);
    *written = frame_size;
    return OTA_PROTOCOL_OK;
}

void ota_parser_init(ota_parser_t *parser)
{
    if (parser != NULL) {
        (void)memset(parser, 0, sizeof(*parser));
        parser->state = OTA_PARSER_SOF0;
    }
}

static void parser_restart(ota_parser_t *parser, uint8_t byte)
{
    parser->state = (byte == OTA_PROTOCOL_SOF0) ? OTA_PARSER_SOF1 : OTA_PARSER_SOF0;
    parser->index = 0u;
}

ota_parse_result_t ota_parser_consume(ota_parser_t *parser,
                                      uint8_t byte,
                                      uint32_t now_ms,
                                      ota_packet_t *packet)
{
    ota_parse_result_t timeout_result = OTA_PARSE_MORE;
    if ((parser == NULL) || (packet == NULL)) {
        return OTA_PARSE_BAD_LENGTH;
    }
    if ((parser->state != OTA_PARSER_SOF0) &&
        ((uint32_t)(now_ms - parser->last_byte_ms) > OTA_PACKET_TIMEOUT_MS)) {
        parser_restart(parser, byte);
        parser->last_byte_ms = now_ms;
        return OTA_PARSE_TIMEOUT;
    }
    parser->last_byte_ms = now_ms;
    switch (parser->state) {
    case OTA_PARSER_SOF0:
        if (byte == OTA_PROTOCOL_SOF0) parser->state = OTA_PARSER_SOF1;
        break;
    case OTA_PARSER_SOF1:
        if (byte == OTA_PROTOCOL_SOF1) {
            parser->state = OTA_PARSER_HEADER;
            parser->index = 0u;
        } else {
            parser_restart(parser, byte);
        }
        break;
    case OTA_PARSER_HEADER:
        parser->header[parser->index++] = byte;
        if (parser->index == sizeof(parser->header)) {
            parser->payload_length = get_u16(&parser->header[4]);
            if (parser->payload_length > OTA_PROTOCOL_MAX_PAYLOAD_SIZE) {
                parser_restart(parser, byte);
                return OTA_PARSE_BAD_LENGTH;
            }
            parser->packet.command = parser->header[1];
            parser->packet.sequence = get_u16(&parser->header[2]);
            parser->packet.length = parser->payload_length;
            parser->running_crc = ota_crc32_update(ota_crc32_init(), parser->header, sizeof(parser->header));
            parser->index = 0u;
            parser->state = (parser->payload_length == 0u) ? OTA_PARSER_CRC : OTA_PARSER_PAYLOAD;
        }
        break;
    case OTA_PARSER_PAYLOAD:
        parser->packet.payload[parser->index++] = byte;
        parser->running_crc = ota_crc32_update(parser->running_crc, &byte, 1u);
        if (parser->index == parser->payload_length) {
            parser->index = 0u;
            parser->state = OTA_PARSER_CRC;
        }
        break;
    case OTA_PARSER_CRC:
        parser->crc_bytes[parser->index++] = byte;
        if (parser->index == sizeof(parser->crc_bytes)) {
            uint32_t expected = ota_crc32_finish(parser->running_crc);
            uint32_t received = get_u32(parser->crc_bytes);
            ota_packet_t completed = parser->packet;
            parser_restart(parser, byte);
            if ((parser->header[0] != OTA_PROTOCOL_VERSION) || (expected != received)) {
                return OTA_PARSE_BAD_CRC;
            }
            *packet = completed;
            return OTA_PARSE_PACKET;
        }
        break;
    default:
        ota_parser_init(parser);
        timeout_result = OTA_PARSE_BAD_LENGTH;
        break;
    }
    return timeout_result;
}
