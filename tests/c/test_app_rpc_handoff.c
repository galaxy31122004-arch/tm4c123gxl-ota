#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "app_ota_handoff.h"
#include "esp_at_controller.h"
#include "ota_request.h"

typedef struct {
    char tx[1024];
    app_ota_handoff_t handoff;
    uint32_t now_ms;
} fixture_t;

static void tx_capture(const char *data, size_t length, void *context)
{
    fixture_t *fixture = context;
    size_t used = strlen(fixture->tx);
    assert(used + length < sizeof(fixture->tx));
    memcpy(fixture->tx + used, data, length);
    fixture->tx[used + length] = '\0';
}

static void ignore_log(const char *message, void *context)
{
    (void)message;
    (void)context;
}

static int accept_ota(const esp_at_rpc_version_t *version, void *context)
{
    fixture_t *fixture = context;
    return app_ota_handoff_accept(&fixture->handoff, version, fixture->now_ms);
}

static void feed_line(esp_at_controller_t *controller, const char *line,
                      uint32_t now_ms)
{
    while (*line != '\0') {
        esp_at_controller_receive(controller, (unsigned char)*line++, now_ms);
    }
    esp_at_controller_receive(controller, '\r', now_ms);
    esp_at_controller_receive(controller, '\n', now_ms);
}

int main(void)
{
    fixture_t fixture = {{0}, {{0U, 0U, 0U}, 0U, 0}, 100U};
    esp_at_controller_t controller;
    esp_at_controller_config_t config = {
        "ssid", "password", "token", "thingsboard.cloud", 1883U, "device"
    };
    ota_version_t requested;

    ota_request_clear();
    app_ota_handoff_init(&fixture.handoff, (ota_version_t){1U, 0U, 1U});
    esp_at_controller_init(&controller, &config, tx_capture, ignore_log,
                           &fixture, 0U);
    esp_at_controller_set_ota_start(&controller, accept_ota, &fixture);
    controller.state = ESP_AT_STATE_ONLINE;

    feed_line(&controller,
        "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/1\",51,"
        "{\"method\":\"START_OTA\",\"params\":{\"version\":\"1.0.2\"}}",
        fixture.now_ms);
    assert(strstr(fixture.tx, "response/1") != NULL);
    assert(strstr(fixture.tx, "accepted\\\":true") != NULL);
    assert(!app_ota_handoff_should_reset(&fixture.handoff, 599U, 0));

    fixture.now_ms = 200U;
    feed_line(&controller,
        "+MQTTSUBRECV:0,\"v1/devices/me/rpc/request/2\",51,"
        "{\"method\":\"START_OTA\",\"params\":{\"version\":\"1.0.3\"}}",
        fixture.now_ms);
    assert(strstr(fixture.tx, "response/2") == NULL);
    assert(ota_request_consume(&requested));
    assert(requested.patch == 2U);
    assert(app_ota_handoff_should_reset(&fixture.handoff, 600U, 0));

    puts("app RPC handoff tests passed");
    return 0;
}
