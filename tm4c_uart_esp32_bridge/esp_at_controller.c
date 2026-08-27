#include "esp_at_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_at_rpc.h"

#define RPC_REQUEST_TOPIC "v1/devices/me/rpc/request/+"
#define TELEMETRY_TOPIC "v1/devices/me/telemetry"
#define ONLINE_TELEMETRY                                                     \
    "{\\\"ota_state\\\":\\\"IDLE\\\",\\\"ota_progress\\\":0,"       \
    "\\\"app_version\\\":\\\"1.0.0\\\",\\\"bootloader_version\\\":" \
    "\\\"1.0.0\\\",\\\"active_slot\\\":\\\"A\\\",\\\"ota_error\\\":0}"

static void log_message(esp_at_controller_t *controller, const char *message)
{
    if (controller->log != NULL) {
        controller->log(message, controller->context);
    }
}

static void send_text(esp_at_controller_t *controller, const char *text)
{
    controller->tx(text, strlen(text), controller->context);
}

static int format_and_send(esp_at_controller_t *controller,
                           const char *format,
                           const char *first,
                           const char *second)
{
    char command[ESP_AT_LINE_SIZE];
    int written = snprintf(command, sizeof(command), format, first, second);

    if (written < 0 || (size_t)written >= sizeof(command)) {
        log_message(controller, "AT_COMMAND_TOO_LONG");
        return 0;
    }
    send_text(controller, command);
    return 1;
}

static int send_state_command(esp_at_controller_t *controller)
{
    char command[ESP_AT_LINE_SIZE];
    int written;

    switch (controller->state) {
    case ESP_AT_STATE_SYNC:
        send_text(controller, "AT\r\n");
        return 1;
    case ESP_AT_STATE_ECHO_OFF:
        send_text(controller, "ATE0\r\n");
        return 1;
    case ESP_AT_STATE_WIFI_MODE:
        send_text(controller, "AT+CWMODE=1\r\n");
        return 1;
    case ESP_AT_STATE_WIFI_JOIN:
        return format_and_send(controller, "AT+CWJAP=\"%s\",\"%s\"\r\n",
                               controller->config.wifi_ssid,
                               controller->config.wifi_password);
    case ESP_AT_STATE_MQTT_CONFIG:
        written = snprintf(
            command, sizeof(command),
            "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"\",0,0,\"\"\r\n",
            controller->config.client_id,
            controller->config.thingsboard_token);
        break;
    case ESP_AT_STATE_MQTT_CONNECT:
        written = snprintf(command, sizeof(command),
                           "AT+MQTTCONN=0,\"%s\",%u,0\r\n",
                           controller->config.mqtt_host,
                           (unsigned int)controller->config.mqtt_port);
        break;
    case ESP_AT_STATE_MQTT_SUBSCRIBE:
        send_text(controller,
                  "AT+MQTTSUB=0,\"" RPC_REQUEST_TOPIC "\",0\r\n");
        return 1;
    case ESP_AT_STATE_MQTT_ANNOUNCE:
        send_text(controller,
                  "AT+MQTTPUB=0,\"" TELEMETRY_TOPIC "\",\""
                  ONLINE_TELEMETRY "\",0,0\r\n");
        return 1;
    case ESP_AT_STATE_ONLINE:
    case ESP_AT_STATE_RETRY:
    default:
        return 0;
    }

    if (written < 0 || (size_t)written >= sizeof(command)) {
        log_message(controller, "AT_COMMAND_TOO_LONG");
        return 0;
    }
    send_text(controller, command);
    return 1;
}

static void enter_retry(esp_at_controller_t *controller, uint32_t now_ms,
                        const char *reason)
{
    controller->state = ESP_AT_STATE_RETRY;
    controller->state_started_ms = now_ms;
    controller->command_sent = 0;
    controller->line_length = 0U;
    log_message(controller, reason);
}

static void advance_state(esp_at_controller_t *controller, uint32_t now_ms)
{
    if (controller->state < ESP_AT_STATE_MQTT_ANNOUNCE) {
        controller->state = (esp_at_state_t)(controller->state + 1);
        controller->command_sent = 0;
        controller->state_started_ms = now_ms;
        esp_at_controller_tick(controller, now_ms);
    } else if (controller->state == ESP_AT_STATE_MQTT_ANNOUNCE) {
        controller->state = ESP_AT_STATE_ONLINE;
        controller->command_sent = 0;
        controller->state_started_ms = now_ms;
        log_message(controller, "MQTT_RPC_READY");
    }
}

static size_t escape_at_parameter(const char *input, char *output,
                                  size_t output_size)
{
    size_t used = 0U;

    while (*input != '\0') {
        if (*input == '"' || *input == '\\') {
            if (used + 2U >= output_size) {
                return 0U;
            }
            output[used++] = '\\';
        } else if (used + 1U >= output_size) {
            return 0U;
        }
        output[used++] = *input++;
    }
    output[used] = '\0';
    return used;
}

static void handle_rpc(esp_at_controller_t *controller, const char *line)
{
    esp_at_rpc_request_t request;
    char response[128];
    char escaped[256];
    char command[ESP_AT_LINE_SIZE];
    int written;

    if (esp_at_rpc_parse(line, &request) != ESP_AT_RPC_OK ||
        esp_at_rpc_build_response(&request, response, sizeof(response)) !=
            ESP_AT_RPC_OK ||
        escape_at_parameter(response, escaped, sizeof(escaped)) == 0U) {
        log_message(controller, "RPC_IGNORED");
        return;
    }

    written = snprintf(command, sizeof(command),
                       "AT+MQTTPUB=0,\"%s\",\"%s\",0,0\r\n",
                       request.response_topic, escaped);
    if (written < 0 || (size_t)written >= sizeof(command)) {
        log_message(controller, "RPC_RESPONSE_TOO_LONG");
        return;
    }
    send_text(controller, command);
    log_message(controller, "RPC_GET_INFO_RESPONSE_SENT");
}

static void process_line(esp_at_controller_t *controller, const char *line,
                         uint32_t now_ms)
{
    if (strncmp(line, "+MQTTSUBRECV:", 13U) == 0) {
        if (controller->state == ESP_AT_STATE_ONLINE) {
            handle_rpc(controller, line);
        }
        return;
    }
    if (strcmp(line, "ERROR") == 0 || strstr(line, "DISCONNECTED") != NULL) {
        enter_retry(controller, now_ms, "AT_SESSION_RETRY");
        return;
    }
    if (strcmp(line, "OK") == 0 && controller->command_sent != 0) {
        advance_state(controller, now_ms);
    }
}

void esp_at_controller_init(esp_at_controller_t *controller,
                            const esp_at_controller_config_t *config,
                            esp_at_tx_fn tx,
                            esp_at_log_fn log,
                            void *context,
                            uint32_t now_ms)
{
    memset(controller, 0, sizeof(*controller));
    controller->config = *config;
    controller->tx = tx;
    controller->log = log;
    controller->context = context;
    controller->state = ESP_AT_STATE_SYNC;
    controller->state_started_ms = now_ms;
}

void esp_at_controller_tick(esp_at_controller_t *controller, uint32_t now_ms)
{
    if (controller->state == ESP_AT_STATE_RETRY) {
        if ((uint32_t)(now_ms - controller->state_started_ms) <
            ESP_AT_RETRY_DELAY_MS) {
            return;
        }
        controller->state = ESP_AT_STATE_SYNC;
        controller->command_sent = 0;
        controller->state_started_ms = now_ms;
    }

    if (controller->state != ESP_AT_STATE_ONLINE &&
        controller->command_sent != 0 &&
        (uint32_t)(now_ms - controller->state_started_ms) >
            ESP_AT_COMMAND_TIMEOUT_MS) {
        enter_retry(controller, now_ms, "AT_COMMAND_TIMEOUT");
        return;
    }

    if (controller->state != ESP_AT_STATE_ONLINE &&
        controller->command_sent == 0) {
        if (send_state_command(controller) != 0) {
            controller->command_sent = 1;
            controller->state_started_ms = now_ms;
        } else {
            enter_retry(controller, now_ms, "AT_COMMAND_FAILED");
        }
    }
}

void esp_at_controller_receive(esp_at_controller_t *controller,
                               unsigned char byte,
                               uint32_t now_ms)
{
    if (byte == '\r') {
        return;
    }
    if (byte == '\n') {
        if (controller->line_length != 0U) {
            controller->line[controller->line_length] = '\0';
            process_line(controller, controller->line, now_ms);
            controller->line_length = 0U;
        }
        return;
    }
    if (controller->line_length + 1U >= sizeof(controller->line)) {
        enter_retry(controller, now_ms, "AT_LINE_TOO_LONG");
        return;
    }
    controller->line[controller->line_length++] = (char)byte;
}

esp_at_state_t esp_at_controller_state(const esp_at_controller_t *controller)
{
    return controller->state;
}
