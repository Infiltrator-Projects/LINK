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

static int target_probes(
    const link_discover_sweep_target *target,
    const link_discover_sweep_probe **presence_probes,
    size_t *presence_probe_count,
    const link_discover_sweep_probe **identity_probe,
    link_discover_sweep_decode_identity_fn *identity_decoder)
{
    static const link_discover_sweep_probe kwp_probes[] = {
        {{0x3eU, 0x01U}, 2U, "KWP2000 presence"},
        {{0x18U, 0x02U, 0xffU, 0x00U}, 4U, "KWP2000 DTC read"}
    };
    if (target == NULL || presence_probes == NULL ||
        presence_probe_count == NULL || identity_probe == NULL ||
        identity_decoder == NULL) {
        return 0;
    }
    if (!target->extended_id && target->tx_can_id == UINT32_C(0x700)) {
        *presence_probes = kwp_probes;
        *presence_probe_count = sizeof(kwp_probes) / sizeof(kwp_probes[0]);
        *identity_probe = NULL;
        *identity_decoder = NULL;
    }
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
        probes, 1U, &identity, decode_identity, NULL, target_probes
    };
    link_discover_sweep_target target;

    CHECK(link_discover_sweep_plan_is_valid(&plan));
    CHECK(link_discover_sweep_plan_target_at(&plan, 0U, &target));
    CHECK(!target.extended_id);
    CHECK(target.tx_can_id == UINT32_C(0x700));
    CHECK(target.rx_can_id == UINT32_C(0x708));
    CHECK(target.bitrate == 500000U);
    {
        const link_discover_sweep_probe *selected = NULL;
        size_t selected_count = 0U;
        const link_discover_sweep_probe *selected_identity = &identity;
        link_discover_sweep_decode_identity_fn selected_decoder =
            decode_identity;
        CHECK(link_discover_sweep_plan_probes_for_target(
                  &plan, &target, &selected, &selected_count,
                  &selected_identity, &selected_decoder));
        CHECK(selected_count == 2U);
        CHECK(selected[0].payload[0] == 0x3eU);
        CHECK(selected[0].payload[1] == 0x01U);
        CHECK(selected[1].payload[0] == 0x18U);
        CHECK(selected_identity == NULL);
        CHECK(selected_decoder == NULL);
    }
    CHECK(link_discover_sweep_plan_target_at(&plan, 1U, &target));
    CHECK(target.extended_id);
    CHECK(target.tx_can_id == UINT32_C(0x18da10f1));
    {
        const link_discover_sweep_probe *selected = NULL;
        size_t selected_count = 0U;
        const link_discover_sweep_probe *selected_identity = NULL;
        link_discover_sweep_decode_identity_fn selected_decoder = NULL;
        CHECK(link_discover_sweep_plan_probes_for_target(
                  &plan, &target, &selected, &selected_count,
                  &selected_identity, &selected_decoder));
        CHECK(selected == probes);
        CHECK(selected_count == 1U);
        CHECK(selected_identity == &identity);
        CHECK(selected_decoder == decode_identity);
    }
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
