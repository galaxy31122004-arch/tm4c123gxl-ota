#ifndef ESP_AT_HTTP_H
#define ESP_AT_HTTP_H

#include <stddef.h>

#define ESP_AT_HTTP_FRAME_SIZE 32U

typedef enum {
    ESP_AT_HTTP_OK = 0,
    ESP_AT_HTTP_MORE,
    ESP_AT_HTTP_DONE,
    ESP_AT_HTTP_INVALID,
    ESP_AT_HTTP_TOO_LARGE,
    ESP_AT_HTTP_SINK_ERROR
} esp_at_http_result_t;

typedef enum {
    ESP_AT_HTTP_GET_SIZE = 0,
    ESP_AT_HTTP_GET_BODY
} esp_at_http_command_t;

typedef int (*esp_at_http_data_fn)(const unsigned char *data, size_t length,
                                   void *context);

typedef struct {
    size_t expected_size;
    size_t remaining;
    size_t frame_length;
    esp_at_http_data_fn data;
    void *context;
    int body_mode;
    char frame[ESP_AT_HTTP_FRAME_SIZE];
} esp_at_http_stream_t;

esp_at_http_result_t esp_at_http_build_url(
    const char *host, const char *token, const char *title, const char *version,
    char *output, size_t output_size);
esp_at_http_result_t esp_at_http_build_command(esp_at_http_command_t command,
                                               const char *url, char *output,
                                               size_t output_size);
esp_at_http_result_t esp_at_http_parse_size(const char *line, size_t *size);
void esp_at_http_stream_init(esp_at_http_stream_t *stream, size_t expected_size,
                             esp_at_http_data_fn data, void *context);
esp_at_http_result_t esp_at_http_stream_feed(esp_at_http_stream_t *stream,
                                             const unsigned char *data,
                                             size_t length);

#endif
