/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_DISCOVER_H
#define LINK_DISCOVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum link_safety_decision {
    LINK_SAFETY_BLOCK = 0,
    LINK_SAFETY_ALLOW_READ_ONLY = 1
} link_safety_decision;

typedef enum link_safety_reason {
    LINK_SAFETY_REASON_ALLOWED_OBD_READ = 0,
    LINK_SAFETY_REASON_ALLOWED_UDS_READ,
    LINK_SAFETY_REASON_ALLOWED_KWP_READ,
    LINK_SAFETY_REASON_EMPTY_REQUEST,
    LINK_SAFETY_REASON_WRITE_OR_CONTROL,
    LINK_SAFETY_REASON_ECU_RESET,
    LINK_SAFETY_REASON_SECURITY_ACCESS,
    LINK_SAFETY_REASON_ROUTINE_CONTROL,
    LINK_SAFETY_REASON_DTC_CLEAR,
    LINK_SAFETY_REASON_PROGRAMMING,
    LINK_SAFETY_REASON_DENY_BY_DEFAULT
} link_safety_reason;

typedef struct link_safety_result {
    link_safety_decision decision;
    link_safety_reason reason;
    uint8_t service;
} link_safety_result;

link_safety_result link_safety_classify(const uint8_t *payload, size_t length);
const char *link_safety_reason_string(link_safety_reason reason);

typedef struct link_evidence_writer link_evidence_writer;

link_evidence_writer *link_evidence_open(const char *path);
int link_evidence_write_frame(link_evidence_writer *writer,
                              uint64_t timestamp_ns,
                              const char *direction,
                              const char *protocol,
                              uint32_t can_id,
                              const uint8_t *data,
                              size_t length,
                              const char *annotation);
int link_evidence_write_annotation(link_evidence_writer *writer,
                                   uint64_t timestamp_ns,
                                   const char *text);
int link_evidence_flush(link_evidence_writer *writer);
void link_evidence_close(link_evidence_writer *writer);

/*
 * Product-neutral exhaustive discovery contract.
 *
 * LINK owns iteration, transport, safety enforcement, cancellation, evidence
 * recording and progress reporting. Product repositories own the actual target
 * map, probe choices and identity interpretation. This prevents one
 * manufacturer's CAN assumptions leaking into another product face.
 */
#define LINK_DISCOVER_SWEEP_MAX_PAYLOAD 8U

typedef struct link_discover_sweep_target {
    uint32_t tx_can_id;
    uint32_t rx_can_id;
    uint32_t bitrate;
    bool extended_id;
} link_discover_sweep_target;

typedef struct link_discover_sweep_probe {
    uint8_t payload[LINK_DISCOVER_SWEEP_MAX_PAYLOAD];
    size_t payload_length;
    const char *annotation;
} link_discover_sweep_probe;

typedef int (*link_discover_sweep_target_at_fn)(
    size_t index, link_discover_sweep_target *target);

typedef int (*link_discover_sweep_decode_identity_fn)(
    const uint8_t *payload, size_t payload_length,
    char *label, size_t label_capacity);

typedef const char *(*link_discover_sweep_fallback_label_fn)(
    const link_discover_sweep_target *target);

typedef struct link_discover_sweep_plan {
    const char *name;
    size_t target_count;
    link_discover_sweep_target_at_fn target_at;
    const link_discover_sweep_probe *presence_probes;
    size_t presence_probe_count;
    const link_discover_sweep_probe *identity_probe;
    link_discover_sweep_decode_identity_fn decode_identity;
    link_discover_sweep_fallback_label_fn fallback_label;
} link_discover_sweep_plan;

int link_discover_sweep_target_is_valid(
    const link_discover_sweep_target *target);
int link_discover_sweep_plan_is_valid(
    const link_discover_sweep_plan *plan);
int link_discover_sweep_plan_target_at(
    const link_discover_sweep_plan *plan,
    size_t index,
    link_discover_sweep_target *target);

#ifdef __cplusplus
}
#endif

#endif
