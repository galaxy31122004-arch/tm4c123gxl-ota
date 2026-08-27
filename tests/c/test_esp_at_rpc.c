#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_at_rpc.h"

static void test_native_get_info(void)
{
    const char *line =
        "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/42\",33,"
        "{\"method\":\"GET_INFO\",\"params\":{}}";
    esp_at_rpc_request_t request;
    char response[128];

    assert(esp_at_rpc_parse(line, &request) == ESP_AT_RPC_OK);
    assert(strcmp(request.request_id, "42") == 0);
    assert(strcmp(request.response_topic,
                  "v1/devices/me/rpc/response/42") == 0);
    assert(esp_at_rpc_build_response(&request, response, sizeof(response)) ==
           ESP_AT_RPC_OK);
    assert(strcmp(response,
                  "{\"app_version\":\"1.0.0\","
                  "\"bootloader_version\":\"1.0.0\","
                  "\"active_slot\":\"A\"}") == 0);
}

static void test_legacy_get_info(void)
{
    const char *line =
        "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/7\",22,"
        "{\"command\":\"GET_INFO\"}";
    esp_at_rpc_request_t request;

    assert(esp_at_rpc_parse(line, &request) == ESP_AT_RPC_OK);
    assert(strcmp(request.request_id, "7") == 0);
}

static void test_rejects_invalid_records(void)
{
    esp_at_rpc_request_t request;

    assert(esp_at_rpc_parse(
               "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/1\",99,{}",
               &request) == ESP_AT_RPC_INVALID);
    assert(esp_at_rpc_parse(
               "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/1234567890123456\","
               "22,{\"command\":\"GET_INFO\"}",
               &request) == ESP_AT_RPC_TOO_LARGE);
    assert(esp_at_rpc_parse(
               "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/2\",19,"
               "{\"method\":\"REBOOT\"}",
               &request) == ESP_AT_RPC_UNSUPPORTED);
}

int main(void)
{
    test_native_get_info();
    test_legacy_get_info();
    test_rejects_invalid_records();
    puts("esp_at_rpc tests passed");
    return 0;
}
