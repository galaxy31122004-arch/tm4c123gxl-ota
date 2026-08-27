#include "app_ota_handoff.h"

#include "ota_request.h"

void app_ota_handoff_init(app_ota_handoff_t *handoff,
                          ota_version_t current_version)
{
    if (handoff == NULL) return;
    handoff->current_version = current_version;
    handoff->accepted_at_ms = 0U;
    handoff->pending = 0;
}

int app_ota_handoff_accept(app_ota_handoff_t *handoff,
                           const esp_at_rpc_version_t *version,
                           uint32_t now_ms)
{
    ota_version_t requested;
    if (handoff == NULL || version == NULL || handoff->pending != 0) return 0;
    requested = (ota_version_t){version->major, version->minor, version->patch};
    if ((requested.major == 0U && requested.minor == 0U &&
         requested.patch == 0U) ||
        (requested.major == handoff->current_version.major &&
         requested.minor == handoff->current_version.minor &&
         requested.patch == handoff->current_version.patch) ||
        !ota_request_store(requested)) {
        return 0;
    }
    handoff->accepted_at_ms = now_ms;
    handoff->pending = 1;
    return 1;
}

int app_ota_handoff_should_reset(const app_ota_handoff_t *handoff,
                                 uint32_t now_ms, int uart_busy)
{
    return handoff != NULL && handoff->pending != 0 && uart_busy == 0 &&
        (uint32_t)(now_ms - handoff->accepted_at_ms) >= APP_OTA_RESET_DELAY_MS;
}
