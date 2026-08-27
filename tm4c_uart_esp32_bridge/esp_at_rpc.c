#include "esp_at_rpc.h"

#include <stdio.h>
#include <string.h>

#define RECEIVE_PREFIX "+MQTTSUBRECV:"
#define REQUEST_TOPIC_PREFIX "v1/devices/me/rpc/request/"
#define START_OTA_PREFIX "{\"method\":\"START_OTA\",\"params\":{\"version\":\""

static int compact_json(const char *payload, size_t payload_length, char *output,
                        size_t output_size)
{
    size_t read_index;
    size_t write_index = 0U;
    int in_string = 0;
    int escaped = 0;

    if (payload_length + 1U > output_size) {
        return 0;
    }
    for (read_index = 0U; read_index < payload_length; ++read_index) {
        char byte = payload[read_index];
        if (!in_string && (byte == ' ' || byte == '\t' || byte == '\r' ||
                           byte == '\n')) {
            continue;
        }
        output[write_index++] = byte;
        if (in_string) {
            if (escaped) {
                escaped = 0;
            } else if (byte == '\\') {
                escaped = 1;
            } else if (byte == '"') {
                in_string = 0;
            }
        } else if (byte == '"') {
            in_string = 1;
        }
    }
    output[write_index] = '\0';
    return !in_string && !escaped;
}

static int payload_is_get_info(const char *payload, size_t payload_length)
{
    static const char native_method[] =
        "{\"method\":\"GET_INFO\",\"params\":{}}";
    static const char legacy_command[] = "{\"command\":\"GET_INFO\"}";
    char copy[96];

    if (payload_length >= sizeof(copy)) {
        return 0;
    }
    if (!compact_json(payload, payload_length, copy, sizeof(copy))) {
        return 0;
    }
    return strcmp(copy, native_method) == 0 || strcmp(copy, legacy_command) == 0;
}

static int parse_version_component(const char **cursor, uint16_t *component,
                                   char terminator)
{
    const char *text = *cursor;
    uint32_t value = 0U;

    if (*text < '0' || *text > '9') {
        return 0;
    }
    do {
        value = value * 10U + (uint32_t)(*text - '0');
        if (value > UINT16_MAX) {
            return 0;
        }
        ++text;
    } while (*text >= '0' && *text <= '9');
    if (*text != terminator) {
        return 0;
    }
    *component = (uint16_t)value;
    *cursor = text + 1;
    return 1;
}

static int payload_parse_start_ota(const char *payload, size_t payload_length,
                                   esp_at_rpc_version_t *version)
{
    char copy[128];
    const char *field;

    if (payload_length >= sizeof(copy)) {
        return 0;
    }
    if (!compact_json(payload, payload_length, copy, sizeof(copy)) ||
        strncmp(copy, START_OTA_PREFIX, sizeof(START_OTA_PREFIX) - 1U) != 0) {
        return 0;
    }
    field = copy + sizeof(START_OTA_PREFIX) - 1U;
    return parse_version_component(&field, &version->major, '.') &&
           parse_version_component(&field, &version->minor, '.') &&
           parse_version_component(&field, &version->patch, '"') &&
           strcmp(field, "}}") == 0;
}

static int payload_looks_like_start_ota(const char *payload,
                                        size_t payload_length)
{
    char copy[128];
    static const char prefix[] = "{\"method\":\"START_OTA\",";

    return compact_json(payload, payload_length, copy, sizeof(copy)) &&
           strncmp(copy, prefix, sizeof(prefix) - 1U) == 0;
}

esp_at_rpc_result_t esp_at_rpc_parse(const char *line,
                                     esp_at_rpc_request_t *request)
{
    const char *topic;
    const char *topic_end;
    const char *request_id;
    const char *length_text;
    const char *payload;
    size_t request_id_length;
    size_t declared_length = 0U;
    size_t actual_length;
    int written;

    if (line == NULL || request == NULL ||
        strncmp(line, RECEIVE_PREFIX, sizeof(RECEIVE_PREFIX) - 1U) != 0) {
        return ESP_AT_RPC_INVALID;
    }
    memset(request, 0, sizeof(*request));

    topic = strchr(line, '"');
    if (topic == NULL) {
        return ESP_AT_RPC_INVALID;
    }
    ++topic;
    topic_end = strchr(topic, '"');
    if (topic_end == NULL ||
        (size_t)(topic_end - topic) <= sizeof(REQUEST_TOPIC_PREFIX) - 1U ||
        strncmp(topic, REQUEST_TOPIC_PREFIX,
                sizeof(REQUEST_TOPIC_PREFIX) - 1U) != 0) {
        return ESP_AT_RPC_INVALID;
    }

    request_id = topic + sizeof(REQUEST_TOPIC_PREFIX) - 1U;
    request_id_length = (size_t)(topic_end - request_id);
    if (request_id_length == 0U) {
        return ESP_AT_RPC_INVALID;
    }
    if (request_id_length >= sizeof(request->request_id)) {
        return ESP_AT_RPC_TOO_LARGE;
    }

    length_text = topic_end + 1;
    if (*length_text != ',') {
        return ESP_AT_RPC_INVALID;
    }
    ++length_text;
    if (*length_text < '0' || *length_text > '9') {
        return ESP_AT_RPC_INVALID;
    }
    while (*length_text >= '0' && *length_text <= '9') {
        if (declared_length > 9999U) {
            return ESP_AT_RPC_TOO_LARGE;
        }
        declared_length = declared_length * 10U +
                          (size_t)(*length_text - '0');
        ++length_text;
    }
    if (*length_text != ',') {
        return ESP_AT_RPC_INVALID;
    }
    payload = length_text + 1;
    actual_length = strlen(payload);
    if (actual_length != declared_length) {
        return ESP_AT_RPC_INVALID;
    }
    if (payload_is_get_info(payload, actual_length)) {
        request->method = ESP_AT_RPC_METHOD_GET_INFO;
    } else if (payload_looks_like_start_ota(payload, actual_length)) {
        if (!payload_parse_start_ota(payload, actual_length,
                                     &request->version)) {
            return ESP_AT_RPC_INVALID;
        }
        request->method = ESP_AT_RPC_METHOD_START_OTA;
    } else {
        return ESP_AT_RPC_UNSUPPORTED;
    }

    memcpy(request->request_id, request_id, request_id_length);
    request->request_id[request_id_length] = '\0';
    written = snprintf(request->response_topic, sizeof(request->response_topic),
                       "v1/devices/me/rpc/response/%s", request->request_id);
    if (written < 0 || (size_t)written >= sizeof(request->response_topic)) {
        return ESP_AT_RPC_TOO_LARGE;
    }
    return ESP_AT_RPC_OK;
}

esp_at_rpc_result_t esp_at_rpc_build_response(
    const esp_at_rpc_request_t *request,
    char *output,
    size_t output_size)
{
    static const char info_body[] =
        "{\"app_version\":\"1.0.0\","
        "\"bootloader_version\":\"1.0.0\","
        "\"active_slot\":\"A\"}";
    static const char accepted_body[] = "{\"accepted\":true}";
    const char *body;
    size_t body_size;

    if (request == NULL || output == NULL || request->request_id[0] == '\0') {
        return ESP_AT_RPC_INVALID;
    }
    body = request->method == ESP_AT_RPC_METHOD_START_OTA ? accepted_body :
                                                           info_body;
    body_size = strlen(body) + 1U;
    if (body_size > output_size) {
        return ESP_AT_RPC_TOO_LARGE;
    }
    memcpy(output, body, body_size);
    return ESP_AT_RPC_OK;
}
