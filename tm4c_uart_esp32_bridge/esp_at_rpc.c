#include "esp_at_rpc.h"

#include <stdio.h>
#include <string.h>

#define RECEIVE_PREFIX "+MQTTSUBRECV:"
#define REQUEST_TOPIC_PREFIX "v1/devices/me/rpc/request/"

static int payload_is_get_info(const char *payload, size_t payload_length)
{
    static const char native_method[] = "\"method\":\"GET_INFO\"";
    static const char legacy_command[] = "\"command\":\"GET_INFO\"";
    char copy[96];

    if (payload_length >= sizeof(copy)) {
        return 0;
    }
    memcpy(copy, payload, payload_length);
    copy[payload_length] = '\0';
    return strstr(copy, native_method) != NULL ||
           strstr(copy, legacy_command) != NULL;
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
    if (!payload_is_get_info(payload, actual_length)) {
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
    static const char body[] =
        "{\"app_version\":\"1.0.0\","
        "\"bootloader_version\":\"1.0.0\","
        "\"active_slot\":\"A\"}";

    if (request == NULL || output == NULL || request->request_id[0] == '\0') {
        return ESP_AT_RPC_INVALID;
    }
    if (sizeof(body) > output_size) {
        return ESP_AT_RPC_TOO_LARGE;
    }
    memcpy(output, body, sizeof(body));
    return ESP_AT_RPC_OK;
}
