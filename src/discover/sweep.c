// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/discover.h"

#include <string.h>

static int sweep_probe_is_valid(const link_discover_sweep_probe *probe)
{
    link_safety_result safety;
    if (probe == NULL || probe->payload_length == 0U ||
        probe->payload_length > LINK_DISCOVER_SWEEP_MAX_PAYLOAD ||
        probe->annotation == NULL || probe->annotation[0] == '\0') {
        return 0;
    }
    safety = link_safety_classify(probe->payload, probe->payload_length);
    return safety.decision == LINK_SAFETY_ALLOW_READ_ONLY;
}

int link_discover_sweep_target_is_valid(
    const link_discover_sweep_target *target)
{
    if (target == NULL || target->bitrate == 0U) return 0;
    if (target->extended_id) {
        return target->tx_can_id <= UINT32_C(0x1fffffff) &&
               target->rx_can_id <= UINT32_C(0x1fffffff);
    }
    return target->tx_can_id <= UINT32_C(0x7ff) &&
           target->rx_can_id <= UINT32_C(0x7ff);
}

int link_discover_sweep_plan_is_valid(
    const link_discover_sweep_plan *plan)
{
    size_t index;
    link_discover_sweep_target first;
    link_discover_sweep_target last;

    if (plan == NULL || plan->name == NULL || plan->name[0] == '\0' ||
        plan->target_count == 0U || plan->target_at == NULL ||
        plan->presence_probes == NULL || plan->presence_probe_count == 0U) {
        return 0;
    }
    for (index = 0U; index < plan->presence_probe_count; ++index) {
        if (!sweep_probe_is_valid(&plan->presence_probes[index])) return 0;
    }
    if ((plan->identity_probe == NULL) != (plan->decode_identity == NULL)) {
        return 0;
    }
    if (plan->identity_probe != NULL &&
        !sweep_probe_is_valid(plan->identity_probe)) {
        return 0;
    }
    if (!plan->target_at(0U, &first) ||
        !link_discover_sweep_target_is_valid(&first)) {
        return 0;
    }
    if (plan->target_count > 1U &&
        (!plan->target_at(plan->target_count - 1U, &last) ||
         !link_discover_sweep_target_is_valid(&last))) {
        return 0;
    }
    return 1;
}

int link_discover_sweep_plan_target_at(
    const link_discover_sweep_plan *plan,
    size_t index,
    link_discover_sweep_target *target)
{
    link_discover_sweep_target value;
    if (target == NULL || !link_discover_sweep_plan_is_valid(plan) ||
        index >= plan->target_count) {
        return 0;
    }
    memset(&value, 0, sizeof(value));
    if (!plan->target_at(index, &value) ||
        !link_discover_sweep_target_is_valid(&value)) {
        return 0;
    }
    *target = value;
    return 1;
}


int link_discover_sweep_plan_probes_for_target(
    const link_discover_sweep_plan *plan,
    const link_discover_sweep_target *target,
    const link_discover_sweep_probe **presence_probes,
    size_t *presence_probe_count,
    const link_discover_sweep_probe **identity_probe,
    link_discover_sweep_decode_identity_fn *decode_identity)
{
    const link_discover_sweep_probe *presence;
    size_t count;
    const link_discover_sweep_probe *identity;
    link_discover_sweep_decode_identity_fn decoder;
    size_t index;

    if (!link_discover_sweep_plan_is_valid(plan) ||
        !link_discover_sweep_target_is_valid(target) ||
        presence_probes == NULL || presence_probe_count == NULL ||
        identity_probe == NULL || decode_identity == NULL) {
        return 0;
    }

    presence = plan->presence_probes;
    count = plan->presence_probe_count;
    identity = plan->identity_probe;
    decoder = plan->decode_identity;

    if (plan->target_probes != NULL &&
        !plan->target_probes(target, &presence, &count, &identity, &decoder)) {
        return 0;
    }

    if (presence == NULL || count == 0U) return 0;
    for (index = 0U; index < count; ++index) {
        if (!sweep_probe_is_valid(&presence[index])) return 0;
    }
    if ((identity == NULL) != (decoder == NULL)) return 0;
    if (identity != NULL && !sweep_probe_is_valid(identity)) return 0;

    *presence_probes = presence;
    *presence_probe_count = count;
    *identity_probe = identity;
    *decode_identity = decoder;
    return 1;
}
