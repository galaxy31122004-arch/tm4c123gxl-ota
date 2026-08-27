#include "esp_at_controller.h"

#include <stdio.h>
#include <string.h>

#include "esp_at_rpc.h"

#define RPC_REQUEST_TOPIC "v1/devices/me/rpc/request/+"
#define WIFI_JOIN_TIMEOUT_MS 30000U
#define TELEMETRY_TOPIC "v1/devices/me/telemetry"
#define ONLINE_TELEMETRY                                                     \
    "{\\\"ota_state\\\":\\\"IDLE\\\"\\,\\\"ota_progress\\\":0\\,"       \
    "\\\"app_version\\\":\\\"1.0.0\\\"\\,\\\"bootloader_version\\\":" \
    "\\\"1.0.0\\\"\\,\\\"active_slot\\\":\\\"A\\\"\\,\\\"ota_error\\\":0}"
#define OTA_DOWNLOADING_TELEMETRY                                            \
    "AT+MQTTPUB=0,\"" TELEMETRY_TOPIC "\","                           \
    "\"{\\\"ota_state\\\":\\\"DOWNLOADING\\\"\\,"                    \
    "\\\"ota_progress\\\":0\\,\\\"ota_error\\\":0}\",0,0\r\n"
#define OTA_REBOOTING_TELEMETRY                                              \
    "AT+MQTTPUB=0,\"" TELEMETRY_TOPIC "\","                           \
    "\"{\\\"ota_state\\\":\\\"REBOOTING\\\"\\,"                      \
    "\\\"ota_progress\\\":100\\,\\\"ota_error\\\":0}\",0,0\r\n"

static int cloud_data(const unsigned char *data, size_t length, void *context)
{
    esp_at_controller_t *controller = (esp_at_controller_t *)context;
    return cloud_ota_write(&controller->cloud_ota, data, length) ==
           CLOUD_OTA_OK;
}

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
    case ESP_AT_STATE_MQTT_CLEAN:
        send_text(controller, "AT+MQTTCLEAN=0\r\n");
        return 1;
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
        if (controller->update != NULL) {
            const ota_metadata_record_t *metadata =
                &controller->update->services.metadata;
            const ota_slot_record_t *active =
                metadata->active_slot == OTA_SLOT_B ? &metadata->slot_b :
                                                       &metadata->slot_a;
            written = snprintf(
                command, sizeof(command),
                "AT+MQTTPUB=0,\"" TELEMETRY_TOPIC "\","
                "\"{\\\"ota_state\\\":\\\"%s\\\"\\,"
                "\\\"ota_progress\\\":%u\\,"
                "\\\"app_version\\\":\\\"%u.%u.%u\\\"\\,"
                "\\\"bootloader_version\\\":\\\"1.0.0\\\"\\,"
                "\\\"active_slot\\\":\\\"%c\\\"\\,"
                "\\\"ota_error\\\":%u}\",0,0\r\n",
                controller->ota_error_pending != 0U ? "ERROR" :
                (controller->boot_confirmed ? "SUCCESS" : "IDLE"),
                controller->ota_error_pending != 0U ?
                    cloud_ota_progress(&controller->cloud_ota) :
                    (controller->boot_confirmed ? 100U : 0U),
                (unsigned)active->version.major,
                (unsigned)active->version.minor,
                (unsigned)active->version.patch,
                metadata->active_slot == OTA_SLOT_B ? 'B' : 'A',
                controller->ota_error_pending);
            break;
        }
        send_text(controller, "AT+MQTTPUB=0,\"" TELEMETRY_TOPIC "\",\""
                              ONLINE_TELEMETRY "\",0,0\r\n");
        return 1;
    case ESP_AT_STATE_ONLINE:
    case ESP_AT_STATE_OTA_SIZE:
    case ESP_AT_STATE_OTA_DOWNLOAD:
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
    if (controller->ota_requested != 0) {
        controller->ota_error_pending = 1U;
    }
    cloud_ota_abort(&controller->cloud_ota);
    controller->state = ESP_AT_STATE_RETRY;
    controller->state_started_ms = now_ms;
    controller->command_sent = 0;
    controller->line_length = 0U;
    if (controller->retry_count < OTA_MAX_RETRIES) {
        ++controller->retry_count;
    }
    log_message(controller, reason);
    if (controller->retry_count >= OTA_MAX_RETRIES) {
        log_message(controller, "AT_RETRY_EXHAUSTED");
    }
}

static const char *timeout_message(esp_at_state_t state)
{
    switch (state) {
    case ESP_AT_STATE_SYNC:
        return "AT_TIMEOUT_SYNC";
    case ESP_AT_STATE_ECHO_OFF:
        return "AT_TIMEOUT_ECHO_OFF";
    case ESP_AT_STATE_WIFI_MODE:
        return "AT_TIMEOUT_WIFI_MODE";
    case ESP_AT_STATE_WIFI_JOIN:
        return "AT_TIMEOUT_WIFI_JOIN";
    case ESP_AT_STATE_MQTT_CLEAN:
        return "AT_TIMEOUT_MQTT_CLEAN";
    case ESP_AT_STATE_MQTT_CONFIG:
        return "AT_TIMEOUT_MQTT_CONFIG";
    case ESP_AT_STATE_MQTT_CONNECT:
        return "AT_TIMEOUT_MQTT_CONNECT";
    case ESP_AT_STATE_MQTT_SUBSCRIBE:
        return "AT_TIMEOUT_MQTT_SUBSCRIBE";
    case ESP_AT_STATE_MQTT_ANNOUNCE:
        return "AT_TIMEOUT_MQTT_ANNOUNCE";
    case ESP_AT_STATE_ONLINE:
        return "AT_TIMEOUT_UNKNOWN";
    case ESP_AT_STATE_OTA_SIZE:
        return "AT_TIMEOUT_OTA_SIZE";
    case ESP_AT_STATE_OTA_DOWNLOAD:
        return "AT_TIMEOUT_OTA_DOWNLOAD";
    case ESP_AT_STATE_RETRY:
    default:
        return "AT_TIMEOUT_UNKNOWN";
    }
}

static uint32_t command_timeout_ms(esp_at_state_t state)
{
    if (state == ESP_AT_STATE_WIFI_JOIN ||
        state == ESP_AT_STATE_OTA_DOWNLOAD) {
        return WIFI_JOIN_TIMEOUT_MS;
    }
    return ESP_AT_COMMAND_TIMEOUT_MS;
}

static const char *error_message(esp_at_state_t state)
{
    switch (state) {
    case ESP_AT_STATE_SYNC:
        return "AT_ERROR_SYNC";
    case ESP_AT_STATE_ECHO_OFF:
        return "AT_ERROR_ECHO_OFF";
    case ESP_AT_STATE_WIFI_MODE:
        return "AT_ERROR_WIFI_MODE";
    case ESP_AT_STATE_WIFI_JOIN:
        return "AT_ERROR_WIFI_JOIN";
    case ESP_AT_STATE_MQTT_CLEAN:
        return "AT_ERROR_MQTT_CLEAN";
    case ESP_AT_STATE_MQTT_CONFIG:
        return "AT_ERROR_MQTT_CONFIG";
    case ESP_AT_STATE_MQTT_CONNECT:
        return "AT_ERROR_MQTT_CONNECT";
    case ESP_AT_STATE_MQTT_SUBSCRIBE:
        return "AT_ERROR_MQTT_SUBSCRIBE";
    case ESP_AT_STATE_MQTT_ANNOUNCE:
        return "AT_ERROR_MQTT_ANNOUNCE";
    case ESP_AT_STATE_ONLINE:
        return "AT_ERROR_ONLINE";
    case ESP_AT_STATE_OTA_SIZE:
        return "AT_ERROR_OTA_SIZE";
    case ESP_AT_STATE_OTA_DOWNLOAD:
        return "AT_ERROR_OTA_DOWNLOAD";
    case ESP_AT_STATE_RETRY:
    default:
        return "AT_ERROR_UNKNOWN";
    }
}

static void advance_state(esp_at_controller_t *controller, uint32_t now_ms)
{
    if (controller->state < ESP_AT_STATE_MQTT_ANNOUNCE) {
        controller->state = (esp_at_state_t)(controller->state + 1);
        controller->command_sent = 0;
        controller->state_started_ms = now_ms;
        esp_at_controller_tick(controller, now_ms);
    } else if (controller->state == ESP_AT_STATE_MQTT_ANNOUNCE) {
        if (controller->ota_requested != 0) {
            char command[ESP_AT_LINE_SIZE];
            if (esp_at_http_build_command(ESP_AT_HTTP_GET_SIZE,
                                          controller->firmware_url, command,
                                          sizeof(command)) != ESP_AT_HTTP_OK) {
                enter_retry(controller, now_ms, "OTA_RETRY_COMMAND_FAILED");
                return;
            }
            controller->state = ESP_AT_STATE_OTA_SIZE;
            controller->command_sent = 1;
            controller->state_started_ms = now_ms;
            controller->ota_size_pending = 0;
            controller->ota_body_pending = 0;
            controller->ota_error_pending = 0U;
            send_text(controller, command);
            log_message(controller, "OTA_RETRY_FROM_ZERO");
            return;
        }
        controller->state = ESP_AT_STATE_ONLINE;
        controller->command_sent = 0;
        controller->state_started_ms = now_ms;
        controller->retry_count = 0U;
        log_message(controller, "MQTT_RPC_READY");
    }
}

static size_t escape_at_parameter(const char *input, char *output,
                                  size_t output_size)
{
    size_t used = 0U;

    while (*input != '\0') {
        if (*input == '"' || *input == '\\' || *input == ',') {
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

static void handle_rpc(esp_at_controller_t *controller, const char *line,
                       uint32_t now_ms)
{
    esp_at_rpc_request_t request;
    char response[128];
    char escaped[256];
    char command[ESP_AT_LINE_SIZE];
    int written;

    if (esp_at_rpc_parse(line, &request) != ESP_AT_RPC_OK) {
        log_message(controller, "RPC_IGNORED");
        return;
    }
    if (request.method == ESP_AT_RPC_METHOD_START_OTA &&
        controller->update == NULL && controller->ota_start == NULL) {
        log_message(controller, "RPC_START_OTA_REJECTED");
        return;
    }
    if (request.method == ESP_AT_RPC_METHOD_START_OTA &&
        controller->update != NULL) {
        const ota_slot_record_t *active =
            controller->update->services.metadata.active_slot == OTA_SLOT_A ?
                &controller->update->services.metadata.slot_a :
                &controller->update->services.metadata.slot_b;
        if (active->version.major == request.version.major &&
            active->version.minor == request.version.minor &&
            active->version.patch == request.version.patch) {
            log_message(controller, "RPC_START_OTA_CURRENT_VERSION");
            return;
        }
    }
    if (request.method == ESP_AT_RPC_METHOD_START_OTA &&
        controller->ota_start != NULL &&
        controller->ota_start(&request.version, controller->ota_context) == 0) {
        log_message(controller, "RPC_START_OTA_REJECTED");
        return;
    }
    if (request.method == ESP_AT_RPC_METHOD_START_OTA &&
        controller->update != NULL) {
        char version[24];
        written = snprintf(version, sizeof(version), "%u.%u.%u",
                           (unsigned)request.version.major,
                           (unsigned)request.version.minor,
                           (unsigned)request.version.patch);
        if (written < 0 || (size_t)written >= sizeof(version) ||
            esp_at_http_build_url(controller->config.mqtt_host,
                                  controller->config.thingsboard_token,
                                  controller->config.client_id, version,
                                  controller->firmware_url,
                                  sizeof(controller->firmware_url)) !=
                ESP_AT_HTTP_OK ||
            esp_at_http_build_command(ESP_AT_HTTP_GET_SIZE,
                                      controller->firmware_url, command,
                                      sizeof(command)) != ESP_AT_HTTP_OK) {
            log_message(controller, "RPC_START_OTA_REJECTED");
            return;
        }
        controller->state = ESP_AT_STATE_OTA_SIZE;
        controller->state_started_ms = now_ms;
        controller->command_sent = 1;
        controller->ota_size_pending = 1;
        controller->ota_telemetry_pending = 1;
        controller->ota_requested = 1;
        controller->requested_version = (ota_version_t){
            request.version.major, request.version.minor, request.version.patch
        };
    }
    if (esp_at_rpc_build_response(&request, response, sizeof(response)) !=
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
    log_message(controller,
                request.method == ESP_AT_RPC_METHOD_START_OTA ?
                    "RPC_START_OTA_ACCEPTED" : "RPC_GET_INFO_RESPONSE_SENT");
}

static void process_line(esp_at_controller_t *controller, const char *line,
                         uint32_t now_ms)
{
    if (controller->state == ESP_AT_STATE_OTA_SIZE &&
        strncmp(line, "+HTTPGETSIZE:", 13U) == 0) {
        size_t remote_size;
        if (esp_at_http_parse_size(line, &remote_size) != ESP_AT_HTTP_OK ||
            cloud_ota_begin(&controller->cloud_ota, controller->update,
                            remote_size, &controller->requested_version) !=
                CLOUD_OTA_OK) {
            enter_retry(controller, now_ms, "OTA_SIZE_REJECTED");
            return;
        }
        esp_at_http_stream_init(&controller->http_stream, remote_size,
                                cloud_data, controller);
        controller->remote_size = remote_size;
        controller->ota_body_pending = 1;
        return;
    }
    if (strncmp(line, "+MQTTSUBRECV:", 13U) == 0) {
        if (controller->state == ESP_AT_STATE_ONLINE) {
            handle_rpc(controller, line, now_ms);
        }
        return;
    }
    if (strcmp(line, "ERROR") == 0) {
        if (controller->state == ESP_AT_STATE_MQTT_CLEAN &&
            controller->command_sent != 0) {
            advance_state(controller, now_ms);
            return;
        }
        enter_retry(controller, now_ms, error_message(controller->state));
        return;
    }
    if (controller->state == ESP_AT_STATE_ONLINE &&
        strstr(line, "DISCONNECTED") != NULL) {
        enter_retry(controller, now_ms, "AT_DISCONNECTED_ONLINE");
        return;
    }
    if (strcmp(line, "OK") == 0 && controller->command_sent != 0) {
        if (controller->state == ESP_AT_STATE_OTA_SIZE &&
            controller->ota_telemetry_pending != 0) {
            controller->ota_telemetry_pending = 0;
            send_text(controller, OTA_DOWNLOADING_TELEMETRY);
            return;
        }
        if (controller->state == ESP_AT_STATE_OTA_SIZE &&
            controller->ota_size_pending != 0) {
            char command[ESP_AT_LINE_SIZE];
            if (esp_at_http_build_command(ESP_AT_HTTP_GET_SIZE,
                                          controller->firmware_url, command,
                                          sizeof(command)) != ESP_AT_HTTP_OK) {
                enter_retry(controller, now_ms, "OTA_SIZE_COMMAND_FAILED");
                return;
            }
            controller->ota_size_pending = 0;
            controller->state_started_ms = now_ms;
            send_text(controller, command);
            return;
        }
        if (controller->state == ESP_AT_STATE_OTA_SIZE &&
            controller->ota_body_pending != 0) {
            char command[ESP_AT_LINE_SIZE];
            if (esp_at_http_build_command(ESP_AT_HTTP_GET_BODY,
                                          controller->firmware_url, command,
                                          sizeof(command)) != ESP_AT_HTTP_OK) {
                enter_retry(controller, now_ms, "OTA_BODY_COMMAND_FAILED");
                return;
            }
            controller->ota_body_pending = 0;
            controller->state = ESP_AT_STATE_OTA_DOWNLOAD;
            controller->state_started_ms = now_ms;
            send_text(controller, command);
            return;
        }
        if (controller->state == ESP_AT_STATE_OTA_SIZE ||
            controller->state == ESP_AT_STATE_OTA_DOWNLOAD) {
            return;
        }
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
        if (controller->retry_count >= OTA_MAX_RETRIES) {
            return;
        }
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
            command_timeout_ms(controller->state)) {
        enter_retry(controller, now_ms, timeout_message(controller->state));
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
    if (controller->http_binary != 0) {
        esp_at_http_result_t result = esp_at_http_stream_feed(
            &controller->http_stream, &byte, 1U);
        if (result == ESP_AT_HTTP_DONE) {
            controller->http_binary = 0;
            if (cloud_ota_finish(&controller->cloud_ota) ==
                CLOUD_OTA_READY_TO_REBOOT) {
                controller->ota_requested = 0;
                controller->state = ESP_AT_STATE_ONLINE;
                send_text(controller, OTA_REBOOTING_TELEMETRY);
                log_message(controller, "OTA_REBOOTING");
            } else {
                enter_retry(controller, now_ms, "OTA_VERIFY_FAILED");
            }
        } else if (result != ESP_AT_HTTP_MORE) {
            controller->http_binary = 0;
            enter_retry(controller, now_ms, "OTA_STREAM_FAILED");
        }
        return;
    }
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
    if (controller->state == ESP_AT_STATE_OTA_DOWNLOAD &&
        controller->line_length == sizeof("+HTTPCGET:") - 1U &&
        memcmp(controller->line, "+HTTPCGET:",
               sizeof("+HTTPCGET:") - 1U) == 0) {
        esp_at_http_result_t result = esp_at_http_stream_feed(
            &controller->http_stream,
            (const unsigned char *)controller->line,
            controller->line_length);
        controller->line_length = 0U;
        if (result == ESP_AT_HTTP_MORE) {
            controller->http_binary = 1;
        } else {
            enter_retry(controller, now_ms, "OTA_STREAM_FAILED");
        }
    }
}

esp_at_state_t esp_at_controller_state(const esp_at_controller_t *controller)
{
    return controller->state;
}

void esp_at_controller_set_ota_start(esp_at_controller_t *controller,
                                     esp_at_ota_start_fn ota_start,
                                     void *context)
{
    if (controller != NULL) {
        controller->ota_start = ota_start;
        controller->ota_context = context;
    }
}

void esp_at_controller_attach_update(esp_at_controller_t *controller,
                                     bl_update_t *update)
{
    if (controller != NULL) {
        controller->update = update;
    }
}

void esp_at_controller_set_boot_confirmed(esp_at_controller_t *controller,
                                          int confirmed)
{
    if (controller != NULL) {
        controller->boot_confirmed = confirmed != 0;
    }
}
