#include <stdio.h>
#include <string.h>

#include "ota_request.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "failed: %s\n", #x); return 1; } } while (0)

int main(void)
{
    ota_request_mailbox_t mailbox;
    ota_version_t version;

    memset(&mailbox, 0, sizeof(mailbox));
    CHECK(!ota_request_validate(&mailbox, &version));

    ota_request_clear();
    CHECK(ota_request_store((ota_version_t){1U, 2U, 3U}));
    CHECK(ota_request_consume(&version));
    CHECK(version.major == 1U && version.minor == 2U && version.patch == 3U);
    CHECK(!ota_request_consume(&version));

    CHECK(!ota_request_store((ota_version_t){0U, 0U, 0U}));
    puts("ota_request tests passed");
    return 0;
}
