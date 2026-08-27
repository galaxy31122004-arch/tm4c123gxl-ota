#ifndef ESP_AT_CONTROLLER_H
#define ESP_AT_CONTROLLER_H

#include <stddef.h>
#include <stdint.h>

#define ESP_AT_COMMAND_TIMEOUT_MS 5000U
#define ESP_AT_RETRY_DELAY_MS 2000U
#define ESP_AT_LINE_SIZE 384U

typedef enum {
    ESP_AT_STATE_SYNC = 0,
    ESP_AT_STATE_ECHO_OFF,
    ESP_AT_STATE_WIFI_MODE,
    ESP_AT_STATE_WIFI_JOIN,
    ESP_AT_STATE_MQTT_CONFIG,
    ESP_AT_STATE_MQTT_CONNECT,
    ESP_AT_STATE_MQTT_SUBSCRIBE,
    ESP_AT_STATE_MQTT_ANNOUNCE,
    ESP_AT_STATE_ONLINE,
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

typedef struct {
    esp_at_controller_config_t config;
    esp_at_tx_fn tx;
    esp_at_log_fn log;
    void *context;
    esp_at_state_t state;
    uint32_t state_started_ms;
    size_t line_length;
    int command_sent;
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
esp_at_state_t esp_at_controller_state(const esp_at_controller_t *controller);

#endif
