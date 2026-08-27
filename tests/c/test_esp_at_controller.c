#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_at_controller.h"

typedef struct {
    char tx[2048];
    char log[1024];
} capture_t;

static void capture_tx(const char *data, size_t length, void *context)
{
    capture_t *capture = context;
    size_t used = strlen(capture->tx);
    assert(used + length < sizeof(capture->tx));
    memcpy(capture->tx + used, data, length);
    capture->tx[used + length] = '\0';
}

static void capture_log(const char *message, void *context)
{
    capture_t *capture = context;
    size_t used = strlen(capture->log);
    size_t length = strlen(message);
    assert(used + length + 1U < sizeof(capture->log));
    memcpy(capture->log + used, message, length);
    capture->log[used + length] = '\n';
    capture->log[used + length + 1U] = '\0';
}

static void feed_line(esp_at_controller_t *controller, const char *line,
                      uint32_t now_ms)
{
    while (*line != '\0') {
        esp_at_controller_receive(controller, (unsigned char)*line, now_ms);
        ++line;
    }
    esp_at_controller_receive(controller, '\r', now_ms);
    esp_at_controller_receive(controller, '\n', now_ms);
}

static esp_at_controller_config_t test_config(void)
{
    esp_at_controller_config_t config = {
        "LabWifi",
        "SecretPassword",
        "DeviceToken",
        "thingsboard.cloud",
        1883U,
        "TM4C123GXL",
    };
    return config;
}

static void test_connects_in_order_and_handles_get_info(void)
{
    static const char expected_setup[] =
        "AT\r\n"
        "ATE0\r\n"
        "AT+CWMODE=1\r\n"
        "AT+CWJAP=\"LabWifi\",\"SecretPassword\"\r\n"
        "AT+MQTTUSERCFG=0,1,\"TM4C123GXL\",\"DeviceToken\",\"\",0,0,\"\"\r\n"
        "AT+MQTTCONN=0,\"thingsboard.cloud\",1883,0\r\n"
        "AT+MQTTSUB=0,\"v1/devices/me/rpc/request/+\",0\r\n";
    capture_t capture = {{0}, {0}};
    esp_at_controller_t controller;
    esp_at_controller_config_t config = test_config();

    esp_at_controller_init(&controller, &config, capture_tx, capture_log,
                           &capture, 0U);
    esp_at_controller_tick(&controller, 0U);
    feed_line(&controller, "OK", 10U);
    feed_line(&controller, "OK", 20U);
    feed_line(&controller, "OK", 30U);
    feed_line(&controller, "WIFI CONNECTED", 40U);
    feed_line(&controller, "WIFI GOT IP", 50U);
    feed_line(&controller, "OK", 60U);
    feed_line(&controller, "OK", 70U);
    feed_line(&controller, "OK", 80U);
    feed_line(&controller, "OK", 90U);

    assert(strcmp(capture.tx, expected_setup) == 0);
    assert(esp_at_controller_state(&controller) == ESP_AT_STATE_ONLINE);

    feed_line(&controller,
              "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/42\",33,"
              "{\"method\":\"GET_INFO\",\"params\":{}}",
              100U);
    assert(strstr(capture.tx,
                  "AT+MQTTPUB=0,\"v1/devices/me/rpc/response/42\","
                  "\"{\\\"app_version\\\":\\\"1.0.0\\\","
                  "\\\"bootloader_version\\\":\\\"1.0.0\\\","
                  "\\\"active_slot\\\":\\\"A\\\"}\",0,0\r\n") != NULL);
    assert(strstr(capture.log, "SecretPassword") == NULL);
    assert(strstr(capture.log, "DeviceToken") == NULL);
}

static void test_timeout_retries_without_exposing_secrets(void)
{
    capture_t capture = {{0}, {0}};
    esp_at_controller_t controller;
    esp_at_controller_config_t config = test_config();

    esp_at_controller_init(&controller, &config, capture_tx, capture_log,
                           &capture, 0U);
    esp_at_controller_tick(&controller, 0U);
    esp_at_controller_tick(&controller, ESP_AT_COMMAND_TIMEOUT_MS + 1U);
    assert(esp_at_controller_state(&controller) == ESP_AT_STATE_RETRY);
    esp_at_controller_tick(&controller,
                           ESP_AT_COMMAND_TIMEOUT_MS + ESP_AT_RETRY_DELAY_MS +
                               2U);
    assert(esp_at_controller_state(&controller) == ESP_AT_STATE_SYNC);
    assert(strcmp(capture.tx, "AT\r\nAT\r\n") == 0);
    assert(strstr(capture.log, "SecretPassword") == NULL);
    assert(strstr(capture.log, "DeviceToken") == NULL);
}

int main(void)
{
    test_connects_in_order_and_handles_get_info();
    test_timeout_retries_without_exposing_secrets();
    puts("esp_at_controller tests passed");
    return 0;
}
