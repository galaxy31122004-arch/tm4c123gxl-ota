#ifndef ESP_AT_RPC_H
#define ESP_AT_RPC_H

#include <stddef.h>
#include <stdint.h>

#define ESP_AT_RPC_REQUEST_ID_SIZE 16U
#define ESP_AT_RPC_TOPIC_SIZE 96U

typedef enum {
    ESP_AT_RPC_OK = 0,
    ESP_AT_RPC_INVALID,
    ESP_AT_RPC_TOO_LARGE,
    ESP_AT_RPC_UNSUPPORTED
} esp_at_rpc_result_t;

typedef enum {
    ESP_AT_RPC_METHOD_GET_INFO = 0,
    ESP_AT_RPC_METHOD_START_OTA
} esp_at_rpc_method_t;

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
} esp_at_rpc_version_t;

typedef struct {
    char request_id[ESP_AT_RPC_REQUEST_ID_SIZE];
    char response_topic[ESP_AT_RPC_TOPIC_SIZE];
    esp_at_rpc_method_t method;
    esp_at_rpc_version_t version;
} esp_at_rpc_request_t;

esp_at_rpc_result_t esp_at_rpc_parse(const char *line,
                                     esp_at_rpc_request_t *request);
esp_at_rpc_result_t esp_at_rpc_build_response(
    const esp_at_rpc_request_t *request,
    char *output,
    size_t output_size);

#endif
