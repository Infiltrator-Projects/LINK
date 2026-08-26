// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/discover.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

static int target_at(size_t index, link_discover_sweep_target *target)
{
    if (target == NULL || index >= 2U) return 0;
    memset(target, 0, sizeof(*target));
    target->bitrate = 500000U;
    if (index == 0U) {
        target->tx_can_id = UINT32_C(0x700);
        target->rx_can_id = UINT32_C(0x708);
        target->extended_id = false;
    } else {
        target->tx_can_id = UINT32_C(0x18da10f1);
        target->rx_can_id = UINT32_C(0x18daf110);
        target->extended_id = true;
    }
    return 1;
}

static int decode_identity(
    const uint8_t *payload, size_t length, char *label, size_t capacity)
{
    (void)payload;
    (void)length;
    if (label == NULL || capacity < 4U) return 0;
    memcpy(label, "ECU", 4U);
    return 1;
}

int main(void)
{
    static const link_discover_sweep_probe probes[] = {
        {{0x3eU, 0x00U}, 2U, "read-only presence"}
    };
    static const link_discover_sweep_probe identity = {
        {0x22U, 0xf1U, 0x97U}, 3U, "read-only identity"
    };
    const link_discover_sweep_plan plan = {
        "test plan", 2U, target_at,
        probes, 1U, &identity, decode_identity, NULL
    };
    link_discover_sweep_target target;

    CHECK(link_discover_sweep_plan_is_valid(&plan));
    CHECK(link_discover_sweep_plan_target_at(&plan, 0U, &target));
    CHECK(!target.extended_id);
    CHECK(target.tx_can_id == UINT32_C(0x700));
    CHECK(target.rx_can_id == UINT32_C(0x708));
    CHECK(target.bitrate == 500000U);
    CHECK(link_discover_sweep_plan_target_at(&plan, 1U, &target));
    CHECK(target.extended_id);
    CHECK(target.tx_can_id == UINT32_C(0x18da10f1));
    CHECK(!link_discover_sweep_plan_target_at(&plan, 2U, &target));

    {
        link_discover_sweep_plan unsafe = plan;
        static const link_discover_sweep_probe bad[] = {
            {{0x11U, 0x01U}, 2U, "ECU reset must be rejected"}
        };
        unsafe.presence_probes = bad;
        CHECK(!link_discover_sweep_plan_is_valid(&unsafe));
    }

    puts("LINK discover sweep plan tests passed");
    return 0;
}
