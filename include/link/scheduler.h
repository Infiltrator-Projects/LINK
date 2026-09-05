// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_SCHEDULER_H
#define LINK_SCHEDULER_H

#include "link/parameter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_SCHEDULER_MAX_ITEMS 256U
#define LINK_OBD2_PID_SET_BYTES 32U

typedef enum {
    LINK_SCHEDULER_PRIORITY_LOW = 0,
    LINK_SCHEDULER_PRIORITY_NORMAL = 1,
    LINK_SCHEDULER_PRIORITY_HIGH = 2,
    LINK_SCHEDULER_PRIORITY_CRITICAL = 3
} LinkSchedulerPriority;

typedef enum {
    LINK_SCHEDULER_RESULT_OK = 0,
    LINK_SCHEDULER_RESULT_INVALID_ARGUMENT,
    LINK_SCHEDULER_RESULT_FULL,
    LINK_SCHEDULER_RESULT_DUPLICATE,
    LINK_SCHEDULER_RESULT_NOT_FOUND
} LinkSchedulerResult;

typedef enum {
    LINK_SCHEDULER_NEXT_READY = 0,
    LINK_SCHEDULER_NEXT_WAITING,
    LINK_SCHEDULER_NEXT_PAUSED,
    LINK_SCHEDULER_NEXT_EMPTY,
    LINK_SCHEDULER_NEXT_INVALID_ARGUMENT
} LinkSchedulerNextResult;

/**
 * Kind of work owned by the one LINK live scheduler.
 *
 * Standard OBD-II work is decoded by LINK. External work is an opaque product
 * transaction (Mercedes/Jaguar/etc.) whose semantics remain in the product
 * layer; LINK owns only its cadence, fairness and adapter-wire slot.
 */
typedef enum {
    LINK_SCHEDULER_ITEM_PARAMETER = 0,
    LINK_SCHEDULER_ITEM_EXTERNAL
} LinkSchedulerItemKind;

typedef struct {
    LinkSchedulerItemKind kind;
    LinkParameterKey key;
    uint8_t pid;
    bool pid_valid;
    uint32_t external_token;
    uint32_t interval_ms;
    uint64_t next_due_ms;
    LinkSchedulerPriority priority;
    bool enabled;
} LinkSchedulerItem;

typedef struct {
    LinkSchedulerItem items[LINK_SCHEDULER_MAX_ITEMS];
    size_t count;
    bool paused;
    uint64_t pause_started_ms;
} LinkScheduler;

typedef struct {
    size_t index;
    LinkSchedulerItemKind kind;
    LinkParameterKey key;
    uint8_t pid;
    bool pid_valid;
    uint32_t external_token;
    uint64_t due_ms;
    uint64_t wait_ms;
} LinkSchedulerDispatch;

const char *link_scheduler_result_name(LinkSchedulerResult result);
const char *link_scheduler_next_result_name(LinkSchedulerNextResult result);
void link_scheduler_init(LinkScheduler *scheduler);
LinkSchedulerResult link_scheduler_add_parameter(LinkScheduler *scheduler, const LinkParameterKey *key, uint32_t interval_ms, LinkSchedulerPriority priority, uint64_t first_due_ms);
LinkSchedulerResult link_scheduler_set_parameter_enabled(LinkScheduler *scheduler, const LinkParameterKey *key, bool enabled);
LinkSchedulerResult link_scheduler_add(LinkScheduler *scheduler, uint8_t pid, uint32_t interval_ms, LinkSchedulerPriority priority, uint64_t first_due_ms);
LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled);

/**
 * Register one opaque manufacturer/product live transaction with LINK's single
 * scheduler. token must be non-zero and unique inside the scheduler.
 */
LinkSchedulerResult link_scheduler_add_external(
    LinkScheduler *scheduler,
    uint32_t token,
    uint32_t interval_ms,
    LinkSchedulerPriority priority,
    uint64_t first_due_ms);
LinkSchedulerResult link_scheduler_set_external_enabled(
    LinkScheduler *scheduler,
    uint32_t token,
    bool enabled);

LinkSchedulerResult link_scheduler_configure_standard_obd2_bits(LinkScheduler *scheduler, const uint8_t supported_bits[LINK_OBD2_PID_SET_BYTES], uint64_t first_due_ms);
/** Configure the shared standard OBD-II cadence directly from LINK's PID set. */
LinkSchedulerResult link_scheduler_configure_standard_obd2(
    LinkScheduler *scheduler,
    const LinkObd2PidSet *supported,
    uint64_t first_due_ms);
void link_scheduler_set_paused(LinkScheduler *scheduler, bool paused, uint64_t now_ms);
LinkSchedulerNextResult link_scheduler_next(const LinkScheduler *scheduler, uint64_t now_ms, LinkSchedulerDispatch *dispatch);
LinkSchedulerResult link_scheduler_mark_dispatched(LinkScheduler *scheduler, size_t index, uint64_t now_ms);

#ifdef __cplusplus
}
#endif
#endif
