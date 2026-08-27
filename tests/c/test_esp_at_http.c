#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_at_http.h"

typedef struct {
    unsigned char data[32];
    size_t length;
} sink_t;

static int collect(const unsigned char *data, size_t length, void *context)
{
    sink_t *sink = (sink_t *)context;

    assert(sink->length + length <= sizeof(sink->data));
    memcpy(sink->data + sink->length, data, length);
    sink->length += length;
    return 1;
}

static int reject_data(const unsigned char *data, size_t length, void *context)
{
    (void)data;
    (void)length;
    (void)context;
    return 0;
}

static void test_commands_and_size(void)
{
    char output[256];
    size_t size = 0U;

    assert(esp_at_http_build_url("thingsboard.cloud", "dummy-token",
                                 "TM4C123GXL", "1.2.3", output,
                                 sizeof(output)) == ESP_AT_HTTP_OK);
    assert(strcmp(output,
                  "https://thingsboard.cloud/api/v1/dummy-token/firmware?"
                  "title=TM4C123GXL&version=1.2.3") == 0);
    assert(esp_at_http_build_command(ESP_AT_HTTP_GET_SIZE, output, output,
                                     sizeof(output)) == ESP_AT_HTTP_OK);
    assert(strncmp(output, "AT+HTTPGETSIZE=\"https://",
                   sizeof("AT+HTTPGETSIZE=\"https://") - 1U) == 0);
    assert(esp_at_http_parse_size("+HTTPGETSIZE:4097", &size) ==
           ESP_AT_HTTP_OK);
    assert(size == 4097U);
    assert(esp_at_http_parse_size("+HTTPGETSIZE:0", &size) ==
           ESP_AT_HTTP_INVALID);
}

static void test_binary_body_across_boundaries(void)
{
    static const unsigned char body[] = {0x00U, '\r', '\n', 'O', 'K', 0xffU};
    static const unsigned char first[] = "+HTTPCG";
    static const unsigned char second[] = "ET:6,";
    esp_at_http_stream_t stream;
    sink_t sink = {{0}, 0U};

    esp_at_http_stream_init(&stream, sizeof(body), collect, &sink);
    assert(esp_at_http_stream_feed(&stream, first, sizeof(first) - 1U) ==
           ESP_AT_HTTP_MORE);
    assert(esp_at_http_stream_feed(&stream, second, sizeof(second) - 1U) ==
           ESP_AT_HTTP_MORE);
    assert(esp_at_http_stream_feed(&stream, body, 2U) == ESP_AT_HTTP_MORE);
    assert(esp_at_http_stream_feed(&stream, body + 2U, sizeof(body) - 2U) ==
           ESP_AT_HTTP_DONE);
    assert(sink.length == sizeof(body));
    assert(memcmp(sink.data, body, sizeof(body)) == 0);
}

static void test_rejects_bad_framing_and_sink_failure(void)
{
    static const unsigned char wrong[] = "+HTTPCGET:4,abc";
    esp_at_http_stream_t stream;
    sink_t sink = {{0}, 0U};

    esp_at_http_stream_init(&stream, 3U, collect, &sink);
    assert(esp_at_http_stream_feed(&stream, wrong, sizeof(wrong) - 1U) ==
           ESP_AT_HTTP_INVALID);

    esp_at_http_stream_init(&stream, 3U, reject_data, &sink);
    assert(esp_at_http_stream_feed(
               &stream, (const unsigned char *)"+HTTPCGET:3,abc", 16U) ==
           ESP_AT_HTTP_SINK_ERROR);
}

int main(void)
{
    test_commands_and_size();
    test_binary_body_across_boundaries();
    test_rejects_bad_framing_and_sink_failure();
    puts("esp_at_http tests passed");
    return 0;
}
