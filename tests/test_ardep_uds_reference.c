// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * Cross-implementation UDS coverage check against Mercedes-Benz ARDEP.
 *
 * ARDEP publishes an Apache-2.0 UDS server implementation. This test does not
 * copy ARDEP code; it uses the independently published service set as a
 * conformance/coverage reference for LINK's generic ISO 14229 catalogue.
 */
#include "link/uds_services.h"

#include <stdio.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

int main(void)
{
    static const uint8_t ardep_services[] = {
        0x10U, 0x11U, 0x14U, 0x19U, 0x22U, 0x23U,
        0x27U, 0x28U, 0x29U, 0x2cU, 0x2eU, 0x2fU,
        0x31U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U,
        0x3dU, 0x3eU, 0x85U, 0x87U
    };
    size_t index;

    CHECK(LINK_UDS_STANDARD_SERVICE_COUNT >=
          sizeof(ardep_services) / sizeof(ardep_services[0]));

    for (index = 0U;
         index < sizeof(ardep_services) / sizeof(ardep_services[0]);
         ++index) {
        CHECK(link_uds_standard_service_find(ardep_services[index]) != NULL);
    }

    /* ARDEP's implementation/tests and Kconfig identify LinkControl as 0x87. */
    CHECK(LINK_UDS_SERVICE_LINK_CONTROL == 0x87U);
    CHECK(LINK_UDS_SERVICE_RESPONSE_ON_EVENT == 0x86U);

    puts("Mercedes-Benz ARDEP UDS reference coverage passed");
    return 0;
}
