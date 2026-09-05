// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/session_trace.h"

#include <stdio.h>
#include <string.h>

static const uint8_t link_default_graph_pids[] = {
    UINT8_C(0x0c), UINT8_C(0x0d), UINT8_C(0x05), UINT8_C(0x23),
    UINT8_C(0x2f), UINT8_C(0x11), UINT8_C(0x46), UINT8_C(0x49)
};

const uint8_t *link_session_trace_default_graph_pids(size_t *count)
{
    if (count != NULL)
        *count = sizeof(link_default_graph_pids) / sizeof(link_default_graph_pids[0]);
    return link_default_graph_pids;
}

bool link_session_trace_init(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count)
{
    if (trace == NULL || graph_count > LINK_SESSION_TRACE_MAX_GRAPHS ||
        (graph_count != 0U && graph_pids == NULL)) {
        return false;
    }
    memset(trace, 0, sizeof(*trace));
    if (graph_count != 0U)
        memcpy(trace->graph_pids, graph_pids, graph_count);
    trace->graph_count = graph_count;
    return true;
}

size_t link_session_trace_graph_index(
    const LinkSessionTrace *trace, uint8_t pid)
{
    size_t index;
    if (trace == NULL) return 0U;
    for (index = 0U; index < trace->graph_count; ++index) {
        if (trace->graph_pids[index] == pid) return index;
    }
    return trace->graph_count;
}

void link_session_trace_reset_graph(LinkSessionTrace *trace)
{
    if (trace == NULL) return;
    memset(trace->graph_history, 0, sizeof(trace->graph_history));
    memset(trace->graph_history_count, 0, sizeof(trace->graph_history_count));
    memset(trace->graph_history_next, 0, sizeof(trace->graph_history_next));
}

void link_session_trace_record_graph(
    LinkSessionTrace *trace, uint8_t pid, double value)
{
    const size_t graph = link_session_trace_graph_index(trace, pid);
    uint8_t slot;
    if (trace == NULL || graph >= trace->graph_count) return;

    slot = trace->graph_history_next[graph];
    trace->graph_history[graph][slot] = value;
    trace->graph_history_next[graph] = (uint8_t)(
        (slot + 1U) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY);
    if (trace->graph_history_count[graph] <
        LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY) {
        ++trace->graph_history_count[graph];
    }
}

static size_t bounded_length(const char *text, size_t maximum)
{
    size_t length = 0U;
    if (text == NULL) return 0U;
    while (length < maximum && text[length] != '\0') ++length;
    return length;
}

static void append_text(char *output, size_t output_size, const char *text)
{
    const size_t used =
        output != NULL ? bounded_length(output, output_size) : output_size;
    if (output == NULL || output_size == 0U || used >= output_size) return;
    (void)snprintf(output + used, output_size - used, "%s", text);
}

void link_session_trace_format_sparkline(
    const double *history, size_t count, size_t next,
    char *output, size_t output_size)
{
    static const char *const blocks[] = {
        "▁", "▂", "▃", "▄", "▅", "▆", "▇", "█"
    };
    double minimum;
    double maximum;
    size_t start;
    size_t index;

    if (output == NULL || output_size == 0U) return;
    output[0] = '\0';
    if (history == NULL || count == 0U) return;
    if (count > LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY)
        count = LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY;

    start = count < LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY ? 0U : next;
    minimum = history[start];
    maximum = history[start];
    for (index = 1U; index < count; ++index) {
        const double value = history[
            (start + index) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;
    }

    for (index = 0U; index < count; ++index) {
        const double value = history[
            (start + index) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY];
        unsigned int level = 3U;
        if (maximum > minimum) {
            const double scaled =
                ((value - minimum) / (maximum - minimum)) * 7.0;
            level = (unsigned int)(scaled + 0.5);
            if (level > 7U) level = 7U;
        }
        append_text(output, output_size, blocks[level]);
    }
}

void link_session_trace_clear_log(LinkSessionTrace *trace, uint64_t now_ms)
{
    if (trace == NULL) return;
    memset(trace->session_log, 0, sizeof(trace->session_log));
    memset(trace->session_log_time_ms, 0, sizeof(trace->session_log_time_ms));
    trace->session_log_count = 0U;
    trace->session_log_next = 0U;
    trace->session_log_started_ms = now_ms;
}

void link_session_trace_append_log(
    LinkSessionTrace *trace, uint64_t now_ms, const char *message)
{
    uint8_t slot;
    size_t length;
    if (trace == NULL || message == NULL || message[0] == '\0') return;

    slot = trace->session_log_next;
    if (trace->session_log_started_ms == 0U)
        trace->session_log_started_ms = now_ms;

    length = strlen(message);
    if (length >= LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY)
        length = LINK_SESSION_TRACE_LOG_MESSAGE_CAPACITY - 1U;
    memcpy(trace->session_log[slot], message, length);
    trace->session_log[slot][length] = '\0';
    trace->session_log_time_ms[slot] =
        now_ms >= trace->session_log_started_ms
            ? now_ms - trace->session_log_started_ms : 0U;
    trace->session_log_next = (uint8_t)(
        (slot + 1U) % LINK_SESSION_TRACE_LOG_CAPACITY);
    if (trace->session_log_count < LINK_SESSION_TRACE_LOG_CAPACITY)
        ++trace->session_log_count;
}

size_t link_session_trace_log_ordered_slot(
    const LinkSessionTrace *trace, size_t ordered_index)
{
    size_t start;
    if (trace == NULL || ordered_index >= trace->session_log_count)
        return LINK_SESSION_TRACE_LOG_CAPACITY;
    start = trace->session_log_count < LINK_SESSION_TRACE_LOG_CAPACITY
        ? 0U : trace->session_log_next;
    return (start + ordered_index) % LINK_SESSION_TRACE_LOG_CAPACITY;
}

const char *link_diagnostic_flow_event_text(LinkDiagnosticFlowEventKind kind)
{
    switch (kind) {
    case LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED:
        return "Adapter identified";
    case LINK_DIAGNOSTIC_FLOW_EVENT_PROTOCOL_IDENTIFIED:
        return "OBD protocol identified";
    case LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE:
        return "Standard PID discovery complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN:
        return "Standard VIN read complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST:
        return "Standard DTC inventory updated";
    case LINK_DIAGNOSTIC_FLOW_EVENT_READINESS:
        return "Readiness monitors captured";
    case LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE:
        return "Freeze-frame sample captured";
    case LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE:
        return "Diagnostic context complete";
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA:
        return "Live PID returned no data";
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED:
        return "Live PID reported unsupported";
    case LINK_DIAGNOSTIC_FLOW_EVENT_NONE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED:
        return NULL;
    }
    return NULL;
}
