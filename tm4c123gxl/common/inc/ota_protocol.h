#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#include "ota_config.h"

typedef enum {
    OTA_CMD_GET_INFO = 0x01,
    OTA_CMD_START_UPDATE = 0x02,
    OTA_CMD_DATA = 0x03,
    OTA_CMD_END_UPDATE = 0x04,
    OTA_CMD_ABORT = 0x05,
    OTA_CMD_ACK = 0x06,
    OTA_CMD_NACK = 0x07,
    OTA_CMD_RESET = 0x08
} ota_command_t;

typedef struct {
    uint8_t command;
    uint16_t sequence;
    uint16_t length;
    uint8_t payload[OTA_PROTOCOL_MAX_PAYLOAD_SIZE];
} ota_packet_t;

typedef enum {
    OTA_PROTOCOL_OK = 0,
    OTA_PROTOCOL_INVALID_ARGUMENT,
    OTA_PROTOCOL_BAD_LENGTH,
    OTA_PROTOCOL_NO_SPACE
} ota_protocol_result_t;

typedef enum {
    OTA_PARSE_MORE = 0,
    OTA_PARSE_PACKET,
    OTA_PARSE_BAD_CRC,
    OTA_PARSE_BAD_LENGTH,
    OTA_PARSE_TIMEOUT
} ota_parse_result_t;

typedef enum {
    OTA_PARSER_SOF0 = 0,
    OTA_PARSER_SOF1,
    OTA_PARSER_HEADER,
    OTA_PARSER_PAYLOAD,
    OTA_PARSER_CRC
} ota_parser_state_t;

typedef struct {
    ota_parser_state_t state;
    uint8_t header[6];
    uint8_t crc_bytes[4];
    uint16_t index;
    uint16_t payload_length;
    uint32_t running_crc;
    uint32_t last_byte_ms;
    ota_packet_t packet;
} ota_parser_t;

ota_protocol_result_t ota_packet_encode(const ota_packet_t *packet,
                                        uint8_t *output,
                                        size_t capacity,
                                        size_t *written);
void ota_parser_init(ota_parser_t *parser);
ota_parse_result_t ota_parser_consume(ota_parser_t *parser,
                                      uint8_t byte,
                                      uint32_t now_ms,
                                      ota_packet_t *packet);

#endif
