#ifndef ESP_AT_CONTROLLER_H
#define ESP_AT_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#include "esp_at_rpc.h"
#include "esp_at_http.h"
#include "cloud_ota.h"

#define ESP_AT_COMMAND_TIMEOUT_MS 5000U
#define ESP_AT_RETRY_DELAY_MS 2000U
#define ESP_AT_LINE_SIZE 384U

typedef enum {
    ESP_AT_STATE_SYNC = 0,
    ESP_AT_STATE_ECHO_OFF,
    ESP_AT_STATE_SYSLOG,
    ESP_AT_STATE_WIFI_MODE,
    ESP_AT_STATE_WIFI_JOIN,
    ESP_AT_STATE_MQTT_CLEAN,
    ESP_AT_STATE_MQTT_CONFIG,
    ESP_AT_STATE_MQTT_CONNECT,
    ESP_AT_STATE_MQTT_SUBSCRIBE,
    ESP_AT_STATE_MQTT_ANNOUNCE,
    ESP_AT_STATE_ONLINE,
    ESP_AT_STATE_OTA_CLEAR_HEADER,
    ESP_AT_STATE_OTA_SIZE,
    ESP_AT_STATE_OTA_RANGE_HEADER,
    ESP_AT_STATE_OTA_DOWNLOAD,
    ESP_AT_STATE_RETRY
} esp_at_state_t;

typedef struct {
    const char *wifi_ssid;
    const char *wifi_password;
    const char *thingsboard_token;
    const char *mqtt_host;
    uint16_t mqtt_port;
    const char *client_id;
} esp_at_controller_config_t;

typedef void (*esp_at_tx_fn)(const char *data, size_t length, void *context);
typedef void (*esp_at_log_fn)(const char *message, void *context);
typedef int (*esp_at_ota_start_fn)(const esp_at_rpc_version_t *version,
                                   void *context);

typedef struct {
    esp_at_controller_config_t config;
    esp_at_tx_fn tx;
    esp_at_log_fn log;
    esp_at_ota_start_fn ota_start;
    void *ota_context;
    void *context;
    esp_at_state_t state;
    uint32_t state_started_ms;
    size_t line_length;
    int command_sent;
    int http_binary;
    int ota_size_pending;
    int ota_body_pending;
    int range_header_stage;
    int ota_chunk_complete;
    int ota_telemetry_pending;
    int ota_requested;
    int boot_confirmed;
    unsigned ota_error_pending;
    unsigned retry_count;
    bl_update_t *update;
    cloud_ota_t cloud_ota;
    cloud_ota_result_t cloud_result;
    esp_at_http_stream_t http_stream;
    char firmware_url[256];
    size_t remote_size;
    size_t ota_offset;
    size_t ota_chunk_size;
    ota_version_t requested_version;
    char line[ESP_AT_LINE_SIZE];
} esp_at_controller_t;

void esp_at_controller_init(esp_at_controller_t *controller,
                            const esp_at_controller_config_t *config,
                            esp_at_tx_fn tx,
                            esp_at_log_fn log,
                            void *context,
                            uint32_t now_ms);
void esp_at_controller_tick(esp_at_controller_t *controller, uint32_t now_ms);
void esp_at_controller_receive(esp_at_controller_t *controller,
                               unsigned char byte,
                               uint32_t now_ms);
void esp_at_controller_set_ota_start(esp_at_controller_t *controller,
                                     esp_at_ota_start_fn ota_start,
                                     void *context);
void esp_at_controller_attach_update(esp_at_controller_t *controller,
                                     bl_update_t *update);
void esp_at_controller_set_boot_confirmed(esp_at_controller_t *controller,
                                          int confirmed);
esp_at_state_t esp_at_controller_state(const esp_at_controller_t *controller);
int esp_at_controller_ota_active(const esp_at_controller_t *controller);
int esp_at_controller_request_ota(esp_at_controller_t *controller,
                                  const ota_version_t *version);

#endif
