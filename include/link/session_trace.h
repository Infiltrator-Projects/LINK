// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_SESSION_TRACE_H
#define LINK_SESSION_TRACE_H

#include "link/diagnostic_flow.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_SESSION_TRACE_MAX_GRAPHS 16U
#define LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY 48U
#define LINK_SESSION_TRACE_LOG_CAPACITY 24U
#define LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY 160U

typedef struct LinkSessionTrace {
    uint8_t graph_pids[LINK_SESSION_TRACE_MAX_GRAPHS];
    size_t graph_count;
    double graph_history[LINK_SESSION_TRACE_MAX_GRAPHS]
                        [LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
    uint8_t graph_history_count[LINK_SESSION_TRACE_MAX_GRAPHS];
    uint8_t graph_history_next[LINK_SESSION_TRACE_MAX_GRAPHS];
    char session_log[LINK_SESSION_TRACE_LOG_CAPACITY]
                    [LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY];
    uint64_t session_log_time_ms[LINK_SESSION_TRACE_LOG_CAPACITY];
    uint8_t session_log_count;
    uint8_t session_log_next;
    uint64_t session_log_started_ms;
} LinkSessionTrace;

bool link_session_trace_init(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count);
size_t link_session_trace_graph_index(
    const LinkSessionTrace *trace, uint8_t pid);
void link_session_trace_reset_graph(LinkSessionTrace *trace);
void link_session_trace_record_graph(
    LinkSessionTrace *trace, uint8_t pid, double value);
void link_session_trace_format_sparkline(
    const double *history, size_t count, size_t next,
    char *output, size_t output_size);

void link_session_trace_clear_log(LinkSessionTrace *trace, uint64_t now_ms);
void link_session_trace_append_log(
    LinkSessionTrace *trace, uint64_t now_ms, const char *message);
size_t link_session_trace_log_ordered_slot(
    const LinkSessionTrace *trace, size_t ordered_index);

/* Human-readable generic flow milestones; live samples deliberately return NULL. */
const char *link_diagnostic_flow_event_text(LinkDiagnosticFlowEventKind kind);

#ifdef __cplusplus
}
#endif
#endif
