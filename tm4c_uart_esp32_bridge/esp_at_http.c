#include "esp_at_http.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define HTTP_SIZE_PREFIX "+HTTPGETSIZE:"
#define HTTP_BODY_PREFIX "+HTTPCGET:"

static int component_is_safe(const char *text)
{
    if (text == NULL || *text == '\0') {
        return 0;
    }
    while (*text != '\0') {
        unsigned char byte = (unsigned char)*text++;
        if (byte <= 0x20U || byte == '"' || byte == '\\' || byte == '?' ||
            byte == '&' || byte == '#') {
            return 0;
        }
    }
    return 1;
}

esp_at_http_result_t esp_at_http_build_url(
    const char *host, const char *token, const char *title, const char *version,
    char *output, size_t output_size)
{
    int written;

    if (output == NULL || output_size == 0U || !component_is_safe(host) ||
        !component_is_safe(token) || !component_is_safe(title) ||
        !component_is_safe(version)) {
        return ESP_AT_HTTP_INVALID;
    }
    written = snprintf(output, output_size,
                       "https://%s/api/v1/%s/firmware?title=%s&version=%s",
                       host, token, title, version);
    if (written < 0 || (size_t)written >= output_size) {
        return ESP_AT_HTTP_TOO_LARGE;
    }
    return ESP_AT_HTTP_OK;
}

esp_at_http_result_t esp_at_http_build_command(esp_at_http_command_t command,
                                               const char *url, char *output,
                                               size_t output_size)
{
    char url_copy[256];
    const char *name;
    int written;

    if (url == NULL || output == NULL || output_size == 0U ||
        strlen(url) >= sizeof(url_copy)) {
        return ESP_AT_HTTP_INVALID;
    }
    memcpy(url_copy, url, strlen(url) + 1U);
    if (command == ESP_AT_HTTP_GET_SIZE) {
        name = "HTTPGETSIZE";
    } else if (command == ESP_AT_HTTP_GET_BODY) {
        name = "HTTPCGET";
    } else {
        return ESP_AT_HTTP_INVALID;
    }
    written = snprintf(output, output_size, "AT+%s=\"%s\"\r\n", name,
                       url_copy);
    if (written < 0 || (size_t)written >= output_size) {
        return ESP_AT_HTTP_TOO_LARGE;
    }
    return ESP_AT_HTTP_OK;
}

static int parse_positive_size(const char *text, char terminator, size_t *value)
{
    size_t parsed = 0U;

    if (*text < '0' || *text > '9') {
        return 0;
    }
    while (*text >= '0' && *text <= '9') {
        size_t digit = (size_t)(*text - '0');
        if (parsed > (SIZE_MAX - digit) / 10U) {
            return 0;
        }
        parsed = parsed * 10U + digit;
        ++text;
    }
    if (*text != terminator || parsed == 0U) {
        return 0;
    }
    *value = parsed;
    return 1;
}

esp_at_http_result_t esp_at_http_parse_size(const char *line, size_t *size)
{
    if (line == NULL || size == NULL ||
        strncmp(line, HTTP_SIZE_PREFIX, sizeof(HTTP_SIZE_PREFIX) - 1U) != 0 ||
        !parse_positive_size(line + sizeof(HTTP_SIZE_PREFIX) - 1U, '\0', size)) {
        return ESP_AT_HTTP_INVALID;
    }
    return ESP_AT_HTTP_OK;
}

void esp_at_http_stream_init(esp_at_http_stream_t *stream, size_t expected_size,
                             esp_at_http_data_fn data, void *context)
{
    memset(stream, 0, sizeof(*stream));
    stream->expected_size = expected_size;
    stream->data = data;
    stream->context = context;
}

static esp_at_http_result_t accept_frame(esp_at_http_stream_t *stream)
{
    size_t declared;

    stream->frame[stream->frame_length] = '\0';
    if (strncmp(stream->frame, HTTP_BODY_PREFIX,
                sizeof(HTTP_BODY_PREFIX) - 1U) != 0 ||
        !parse_positive_size(stream->frame + sizeof(HTTP_BODY_PREFIX) - 1U,
                             ',', &declared) ||
        declared != stream->expected_size) {
        return ESP_AT_HTTP_INVALID;
    }
    stream->remaining = declared;
    stream->body_mode = 1;
    return ESP_AT_HTTP_MORE;
}

esp_at_http_result_t esp_at_http_stream_feed(esp_at_http_stream_t *stream,
                                             const unsigned char *data,
                                             size_t length)
{
    size_t offset = 0U;

    if (stream == NULL || (data == NULL && length != 0U) ||
        stream->data == NULL || stream->expected_size == 0U) {
        return ESP_AT_HTTP_INVALID;
    }
    while (offset < length && stream->body_mode == 0) {
        if (stream->frame_length + 1U >= sizeof(stream->frame)) {
            return ESP_AT_HTTP_TOO_LARGE;
        }
        stream->frame[stream->frame_length++] = (char)data[offset++];
        if (stream->frame[stream->frame_length - 1U] == ',') {
            esp_at_http_result_t result = accept_frame(stream);
            if (result != ESP_AT_HTTP_MORE) {
                return result;
            }
        }
    }
    if (stream->body_mode != 0 && offset < length) {
        size_t available = length - offset;
        size_t accepted = available < stream->remaining ? available :
                                                          stream->remaining;
        if (accepted != 0U &&
            stream->data(data + offset, accepted, stream->context) == 0) {
            return ESP_AT_HTTP_SINK_ERROR;
        }
        stream->remaining -= accepted;
    }
    return stream->body_mode != 0 && stream->remaining == 0U ?
               ESP_AT_HTTP_DONE : ESP_AT_HTTP_MORE;
}
