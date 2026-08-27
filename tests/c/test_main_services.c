#include <stdio.h>
#include <string.h>

#include "bl_main.h"

void bl_hal_init(void) {}

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "CHECK failed: %s:%d\n", __FILE__, __LINE__); return 1; } } while (0)

int main(void)
{
    bl_services_t services;

    (void)memset(&services, 0, sizeof(services));
    bl_services_init(&services);

    CHECK(services.reset != NULL);
    return 0;
}
