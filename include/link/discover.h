/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_DISCOVER_H
#define LINK_DISCOVER_H

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

#ifdef __cplusplus
}
#endif

#endif
