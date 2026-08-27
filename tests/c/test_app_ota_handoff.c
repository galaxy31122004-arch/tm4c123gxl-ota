#include <assert.h>
#include <stdio.h>

#include "app_ota_handoff.h"
#include "ota_request.h"

int main(void)
{
    app_ota_handoff_t handoff;
    ota_version_t version;

    app_ota_handoff_init(&handoff, (ota_version_t){1U, 0U, 1U});
    assert(!app_ota_handoff_accept(&handoff,
                                   &(esp_at_rpc_version_t){1U, 0U, 1U}, 10U));
    assert(app_ota_handoff_accept(&handoff,
                                  &(esp_at_rpc_version_t){1U, 0U, 2U}, 20U));
    assert(!app_ota_handoff_accept(&handoff,
                                   &(esp_at_rpc_version_t){1U, 0U, 3U}, 30U));
    assert(ota_request_consume(&version));
    assert(version.major == 1U && version.minor == 0U && version.patch == 2U);
    assert(!app_ota_handoff_should_reset(&handoff, 519U, 0));
    assert(!app_ota_handoff_should_reset(&handoff, 520U, 1));
    assert(app_ota_handoff_should_reset(&handoff, 520U, 0));
    puts("app_ota_handoff tests passed");
    return 0;
}
