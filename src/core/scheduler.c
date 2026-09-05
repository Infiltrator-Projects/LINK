// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/scheduler.h"

#include "infiltratr/core.h"
#include "infiltratr/timing.h"

#include <string.h>

static bool priority_valid(LinkSchedulerPriority priority)
{
    return priority >= LINK_SCHEDULER_PRIORITY_LOW && priority <= LINK_SCHEDULER_PRIORITY_CRITICAL;
}

static LinkParameterKey obd2_key(uint8_t pid)
{
    LinkParameterKey key = { LINK_PARAMETER_PROTOCOL_OBD2, LINK_PARAMETER_MODULE_STANDARD_OBD2, (uint32_t)pid };
    return key;
}

static bool key_to_pid(const LinkParameterKey *key, uint8_t *pid)
{
    if (!link_parameter_key_is_valid(key) || pid == NULL || key->protocol != LINK_PARAMETER_PROTOCOL_OBD2 || key->module != LINK_PARAMETER_MODULE_STANDARD_OBD2 || key->identifier > UINT8_MAX) return false;
    *pid = (uint8_t)key->identifier;
    return true;
}

const char *link_scheduler_result_name(LinkSchedulerResult result)
{
    switch (result) { case LINK_SCHEDULER_RESULT_OK: return "ok"; case LINK_SCHEDULER_RESULT_INVALID_ARGUMENT: return "invalid-argument"; case LINK_SCHEDULER_RESULT_FULL: return "full"; case LINK_SCHEDULER_RESULT_DUPLICATE: return "duplicate"; case LINK_SCHEDULER_RESULT_NOT_FOUND: return "not-found"; }
    return "unknown";
}

const char *link_scheduler_next_result_name(LinkSchedulerNextResult result)
{
    switch (result) { case LINK_SCHEDULER_NEXT_READY: return "ready"; case LINK_SCHEDULER_NEXT_WAITING: return "waiting"; case LINK_SCHEDULER_NEXT_PAUSED: return "paused"; case LINK_SCHEDULER_NEXT_EMPTY: return "empty"; case LINK_SCHEDULER_NEXT_INVALID_ARGUMENT: return "invalid-argument"; }
    return "unknown";
}

void link_scheduler_init(LinkScheduler *scheduler) { if (scheduler != NULL) memset(scheduler, 0, sizeof(*scheduler)); }

LinkSchedulerResult link_scheduler_add_parameter(LinkScheduler *scheduler, const LinkParameterKey *key, uint32_t interval_ms, LinkSchedulerPriority priority, uint64_t first_due_ms)
{
    size_t index;
    LinkSchedulerItem item;
    uint8_t pid = 0U;
    if (scheduler == NULL || !link_parameter_key_is_valid(key) || interval_ms == 0U || !priority_valid(priority)) return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    for (index = 0U; index < scheduler->count; ++index) {
        if (scheduler->items[index].kind == LINK_SCHEDULER_ITEM_PARAMETER &&
            link_parameter_key_equal(&scheduler->items[index].key, key)) return LINK_SCHEDULER_RESULT_DUPLICATE;
    }
    if (scheduler->count >= LINK_SCHEDULER_MAX_ITEMS) return LINK_SCHEDULER_RESULT_FULL;
    memset(&item, 0, sizeof(item));
    item.kind = LINK_SCHEDULER_ITEM_PARAMETER;
    item.key = *key; item.pid_valid = key_to_pid(key, &pid); item.pid = pid; item.interval_ms = interval_ms; item.next_due_ms = first_due_ms; item.priority = priority; item.enabled = true;
    scheduler->items[scheduler->count++] = item;
    return LINK_SCHEDULER_RESULT_OK;
}

LinkSchedulerResult link_scheduler_set_parameter_enabled(LinkScheduler *scheduler, const LinkParameterKey *key, bool enabled)
{
    size_t index;
    if (scheduler == NULL || !link_parameter_key_is_valid(key)) return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    for (index = 0U; index < scheduler->count; ++index) {
        if (scheduler->items[index].kind == LINK_SCHEDULER_ITEM_PARAMETER &&
            link_parameter_key_equal(&scheduler->items[index].key, key)) {
            scheduler->items[index].enabled = enabled;
            return LINK_SCHEDULER_RESULT_OK;
        }
    }
    return LINK_SCHEDULER_RESULT_NOT_FOUND;
}

LinkSchedulerResult link_scheduler_add(LinkScheduler *scheduler, uint8_t pid, uint32_t interval_ms, LinkSchedulerPriority priority, uint64_t first_due_ms)
{
    const LinkParameterKey key = obd2_key(pid);
    return link_scheduler_add_parameter(scheduler, &key, interval_ms, priority, first_due_ms);
}

LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled)
{
    const LinkParameterKey key = obd2_key(pid);
    return link_scheduler_set_parameter_enabled(scheduler, &key, enabled);
}

LinkSchedulerResult link_scheduler_add_external(
    LinkScheduler *scheduler,
    uint32_t token,
    uint32_t interval_ms,
    LinkSchedulerPriority priority,
    uint64_t first_due_ms)
{
    LinkSchedulerItem item;
    size_t index;
    if (scheduler == NULL || token == 0U || interval_ms == 0U ||
        !priority_valid(priority)) {
        return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    }
    for (index = 0U; index < scheduler->count; ++index) {
        if (scheduler->items[index].kind == LINK_SCHEDULER_ITEM_EXTERNAL &&
            scheduler->items[index].external_token == token) {
            return LINK_SCHEDULER_RESULT_DUPLICATE;
        }
    }
    if (scheduler->count >= LINK_SCHEDULER_MAX_ITEMS)
        return LINK_SCHEDULER_RESULT_FULL;
    memset(&item, 0, sizeof(item));
    item.kind = LINK_SCHEDULER_ITEM_EXTERNAL;
    item.external_token = token;
    item.interval_ms = interval_ms;
    item.next_due_ms = first_due_ms;
    item.priority = priority;
    item.enabled = true;
    scheduler->items[scheduler->count++] = item;
    return LINK_SCHEDULER_RESULT_OK;
}

LinkSchedulerResult link_scheduler_set_external_enabled(
    LinkScheduler *scheduler,
    uint32_t token,
    bool enabled)
{
    size_t index;
    if (scheduler == NULL || token == 0U)
        return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    for (index = 0U; index < scheduler->count; ++index) {
        if (scheduler->items[index].kind == LINK_SCHEDULER_ITEM_EXTERNAL &&
            scheduler->items[index].external_token == token) {
            scheduler->items[index].enabled = enabled;
            return LINK_SCHEDULER_RESULT_OK;
        }
    }
    return LINK_SCHEDULER_RESULT_NOT_FOUND;
}

typedef struct { uint8_t pid; uint32_t interval_ms; LinkSchedulerPriority priority; } StandardSchedule;
static bool bitset_contains(const uint8_t bits[LINK_OBD2_PID_SET_BYTES], uint8_t pid) { return bits != NULL && (bits[pid / 8U] & (uint8_t)(1U << (pid % 8U))) != 0U; }

LinkSchedulerResult link_scheduler_configure_standard_obd2_bits(LinkScheduler *scheduler, const uint8_t supported_bits[LINK_OBD2_PID_SET_BYTES], uint64_t first_due_ms)
{
    static const StandardSchedule plan[] = {
        { 0x0cU, 500U, LINK_SCHEDULER_PRIORITY_CRITICAL },
        { 0x0dU, 750U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x0bU, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x23U, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x2fU, 5000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x49U, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x4aU, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x4cU, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x11U, 1000U, LINK_SCHEDULER_PRIORITY_HIGH },
        { 0x04U, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x10U, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x24U, 2000U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x3eU, 3000U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x2cU, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x2dU, 2000U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x45U, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x47U, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x48U, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x4bU, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x5eU, 1500U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x78U, 2000U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x3cU, 3000U, LINK_SCHEDULER_PRIORITY_NORMAL },
        { 0x05U, 3000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x0fU, 3000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x33U, 3000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x42U, 3000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x46U, 5000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x5cU, 3000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x1fU, 5000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x21U, 10000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x30U, 15000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x31U, 10000U, LINK_SCHEDULER_PRIORITY_LOW },
        { 0x4dU, 15000U, LINK_SCHEDULER_PRIORITY_LOW }
    };
    LinkScheduler external_only;
    size_t index;
    if (scheduler == NULL || supported_bits == NULL) return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;

    /* Rebuilding the standard SAE plan must never discard product jobs. */
    link_scheduler_init(&external_only);
    for (index = 0U; index < scheduler->count; ++index) {
        if (scheduler->items[index].kind != LINK_SCHEDULER_ITEM_EXTERNAL)
            continue;
        if (external_only.count >= LINK_SCHEDULER_MAX_ITEMS)
            return LINK_SCHEDULER_RESULT_FULL;
        external_only.items[external_only.count++] = scheduler->items[index];
    }
    *scheduler = external_only;

    for (index = 0U; index < INFILTRATR_ARRAY_LENGTH(plan); ++index) {
        LinkSchedulerResult result;
        if (!bitset_contains(supported_bits, plan[index].pid)) continue;
        if (link_parameter_obd2_definition(plan[index].pid) == NULL) continue;
        result = link_scheduler_add(
            scheduler, plan[index].pid, plan[index].interval_ms,
            plan[index].priority, first_due_ms);
        if (result != LINK_SCHEDULER_RESULT_OK) {
            return result;
        }
    }

    /*
     * The priority plan above is only a cadence override, never the standards
     * catalogue. Every assigned Mode 01 definition that the vehicle advertises
     * must be schedulable, including structured/raw definitions added later.
     * Polling preferences are applied by the platform controller afterwards,
     * so adding the item does not create unwanted bus traffic.
     */
    for (index = 0U; index < link_obd2_pid_definition_count(); ++index) {
        const LinkObd2PidDefinition *definition = link_obd2_pid_definition_at(index);
        LinkSchedulerResult result;
        uint8_t pid;
        if (definition == NULL || definition->mode != UINT8_C(0x01)) continue;
        pid = definition->pid;
        if ((pid & UINT8_C(0x1f)) == 0U) continue;
        if (!bitset_contains(supported_bits, pid)) continue;
        result = link_scheduler_add(scheduler, pid, 3000U,
                                    LINK_SCHEDULER_PRIORITY_LOW, first_due_ms);
        if (result == LINK_SCHEDULER_RESULT_DUPLICATE) continue;
        if (result != LINK_SCHEDULER_RESULT_OK) {
            return result;
        }
    }
    return LINK_SCHEDULER_RESULT_OK;
}

LinkSchedulerResult link_scheduler_configure_standard_obd2(
    LinkScheduler *scheduler,
    const LinkObd2PidSet *supported,
    uint64_t first_due_ms)
{
    if (supported == NULL)
        return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    return link_scheduler_configure_standard_obd2_bits(
        scheduler, supported->bits, first_due_ms);
}

void link_scheduler_set_paused(LinkScheduler *scheduler, bool paused, uint64_t now_ms)
{
    size_t index;
    uint64_t pause_duration = 0U;
    if (scheduler == NULL || scheduler->paused == paused) return;
    if (paused) { scheduler->paused = true; scheduler->pause_started_ms = now_ms; return; }
    if (now_ms >= scheduler->pause_started_ms) pause_duration = now_ms - scheduler->pause_started_ms;
    for (index = 0U; index < scheduler->count; ++index) scheduler->items[index].next_due_ms = infiltratr_u64_add_saturating(scheduler->items[index].next_due_ms, pause_duration);
    scheduler->paused = false; scheduler->pause_started_ms = 0U;
}

LinkSchedulerNextResult link_scheduler_next(const LinkScheduler *scheduler, uint64_t now_ms, LinkSchedulerDispatch *dispatch)
{
    bool have_enabled = false, have_due = false;
    size_t selected = 0U, index;
    uint64_t earliest_due = UINT64_MAX;
    if (dispatch != NULL) memset(dispatch, 0, sizeof(*dispatch));
    if (scheduler == NULL || dispatch == NULL) return LINK_SCHEDULER_NEXT_INVALID_ARGUMENT;
    if (scheduler->paused) return LINK_SCHEDULER_NEXT_PAUSED;
    for (index = 0U; index < scheduler->count; ++index) {
        const LinkSchedulerItem *item = &scheduler->items[index];
        if (!item->enabled) continue;
        if (!have_enabled || item->next_due_ms < earliest_due) earliest_due = item->next_due_ms;
        have_enabled = true;
        if (item->next_due_ms > now_ms) continue;
        if (!have_due || item->next_due_ms < scheduler->items[selected].next_due_ms || (item->next_due_ms == scheduler->items[selected].next_due_ms && item->priority > scheduler->items[selected].priority) || (item->next_due_ms == scheduler->items[selected].next_due_ms && item->priority == scheduler->items[selected].priority && index < selected)) { selected = index; have_due = true; }
    }
    if (!have_enabled) return LINK_SCHEDULER_NEXT_EMPTY;
    if (!have_due) { dispatch->due_ms = earliest_due; dispatch->wait_ms = earliest_due > now_ms ? earliest_due - now_ms : 0U; return LINK_SCHEDULER_NEXT_WAITING; }
    dispatch->index = selected;
    dispatch->kind = scheduler->items[selected].kind;
    dispatch->key = scheduler->items[selected].key;
    dispatch->pid = scheduler->items[selected].pid;
    dispatch->pid_valid = scheduler->items[selected].pid_valid;
    dispatch->external_token = scheduler->items[selected].external_token;
    dispatch->due_ms = scheduler->items[selected].next_due_ms;
    dispatch->wait_ms = 0U;
    return LINK_SCHEDULER_NEXT_READY;
}

LinkSchedulerResult link_scheduler_mark_dispatched(LinkScheduler *scheduler, size_t index, uint64_t now_ms)
{
    LinkSchedulerItem *item;
    if (scheduler == NULL || index >= scheduler->count) return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    item = &scheduler->items[index];
    if (!infiltratr_periodic_deadline_advance(item->next_due_ms, now_ms, (uint64_t)item->interval_ms, &item->next_due_ms)) return LINK_SCHEDULER_RESULT_INVALID_ARGUMENT;
    return LINK_SCHEDULER_RESULT_OK;
}
