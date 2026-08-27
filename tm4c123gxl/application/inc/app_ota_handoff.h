#ifndef APP_OTA_HANDOFF_H
#define APP_OTA_HANDOFF_H

#include <stdint.h>

#include "esp_at_rpc.h"
#include "ota_types.h"

#define APP_OTA_RESET_DELAY_MS UINT32_C(500)

typedef struct {
    ota_version_t current_version;
    uint32_t accepted_at_ms;
    int pending;
} app_ota_handoff_t;

void app_ota_handoff_init(app_ota_handoff_t *handoff,
                          ota_version_t current_version);
int app_ota_handoff_accept(app_ota_handoff_t *handoff,
                           const esp_at_rpc_version_t *version,
                           uint32_t now_ms);
int app_ota_handoff_should_reset(const app_ota_handoff_t *handoff,
                                 uint32_t now_ms, int uart_busy);

#endif
