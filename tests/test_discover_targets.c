// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/discover.h"
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    link_discover_sweep_target target;

    CHECK(link_discover_standard_obd11_target_at(0U, 500000U, &target));
    CHECK(target.tx_can_id == 0x7e0U && target.rx_can_id == 0x7e8U);
    CHECK(!target.extended_id && target.bitrate == 500000U);
    CHECK(link_discover_standard_obd11_target_at(7U, 500000U, &target));
    CHECK(target.tx_can_id == 0x7e7U && target.rx_can_id == 0x7efU);
    CHECK(!link_discover_standard_obd11_target_at(8U, 500000U, &target));

    CHECK(link_discover_standard_uds29_target_at(0U, 500000U, &target));
    CHECK(target.tx_can_id == 0x18da00f1U && target.rx_can_id == 0x18daf100U);
    CHECK(target.extended_id);
    CHECK(link_discover_standard_uds29_target_at(0xf1U, 500000U, &target));
    CHECK(target.tx_can_id == 0x18daf2f1U && target.rx_can_id == 0x18daf1f2U);
    CHECK(!link_discover_standard_uds29_target(0xf1U, 500000U, &target));
    CHECK(!link_discover_standard_uds29_target_at(
        LINK_DISCOVER_STANDARD_UDS29_TARGET_COUNT, 500000U, &target));

    puts("LINK standards-defined discovery targets passed");
    return 0;
}
