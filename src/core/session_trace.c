// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/session_trace.h"

#include "infiltratr/core.h"

#include <stdio.h>
#include <string.h>

static const uint8_t link_default_graph_pids[] = {
    UINT8_C(0x0c), UINT8_C(0x0d), UINT8_C(0x05), UINT8_C(0x23),
    UINT8_C(0x2f), UINT8_C(0x11), UINT8_C(0x46), UINT8_C(0x49)
};

const uint8_t *link_session_trace_default_graph_pids(size_t *count)
{
    if (count != NULL)
        *count = INFILTRATR_ARRAY_LENGTH(link_default_graph_pids);
    return link_default_graph_pids;
}

static LinkParameterKey standard_obd_key(uint8_t pid)
{
    LinkParameterKey key = {
        LINK_PARAMETER_PROTOCOL_OBD2,
        LINK_PARAMETER_MODULE_STANDARD_OBD2,
        (uint32_t)pid
    };
    return key;
}

bool link_session_trace_init(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count)
{
    if (trace == NULL || graph_count > LINK_SESSION_TRACE_MAX_GRAPHS ||
        (graph_count != 0U && graph_pids == NULL)) {
        return false;
    }
    memset(trace, 0, sizeof(*trace));
    return link_session_trace_configure_graph_pids(
        trace, graph_pids, graph_count);
}

bool link_session_trace_configure_graph_keys(
    LinkSessionTrace *trace,
    const LinkParameterKey *graph_keys,
    size_t graph_count)
{
    size_t left;
    size_t right;

    if (trace == NULL || graph_count > LINK_SESSION_TRACE_MAX_GRAPHS ||
        (graph_count != 0U && graph_keys == NULL)) {
        return false;
    }
    for (left = 0U; left < graph_count; ++left) {
        if (!link_parameter_key_is_valid(&graph_keys[left]))
            return false;
        for (right = left + 1U; right < graph_count; ++right) {
            if (link_parameter_key_equal(
                    &graph_keys[left], &graph_keys[right])) {
                return false;
            }
        }
    }

    memset(trace->graph_keys, 0, sizeof(trace->graph_keys));
    memset(trace->graph_pids, 0, sizeof(trace->graph_pids));
    for (left = 0U; left < graph_count; ++left) {
        trace->graph_keys[left] = graph_keys[left];
        if (graph_keys[left].protocol == LINK_PARAMETER_PROTOCOL_OBD2 &&
            graph_keys[left].module ==
                LINK_PARAMETER_MODULE_STANDARD_OBD2 &&
            graph_keys[left].identifier <= UINT8_MAX) {
            trace->graph_pids[left] = (uint8_t)graph_keys[left].identifier;
        }
    }
    trace->graph_count = graph_count;
    link_session_trace_reset_graph(trace);
    return true;
}

bool link_session_trace_configure_graph_pids(
    LinkSessionTrace *trace, const uint8_t *graph_pids, size_t graph_count)
{
    LinkParameterKey keys[LINK_SESSION_TRACE_MAX_GRAPHS];
    size_t index;
    if (trace == NULL || graph_count > LINK_SESSION_TRACE_MAX_GRAPHS ||
        (graph_count != 0U && graph_pids == NULL)) {
        return false;
    }
    for (index = 0U; index < graph_count; ++index)
        keys[index] = standard_obd_key(graph_pids[index]);
    return link_session_trace_configure_graph_keys(
        trace, keys, graph_count);
}

size_t link_session_trace_graph_key_index(
    const LinkSessionTrace *trace,
    const LinkParameterKey *key)
{
    size_t index;
    if (trace == NULL || !link_parameter_key_is_valid(key))
        return trace != NULL ? trace->graph_count : 0U;
    for (index = 0U; index < trace->graph_count; ++index) {
        if (link_parameter_key_equal(&trace->graph_keys[index], key))
            return index;
    }
    return trace->graph_count;
}

size_t link_session_trace_graph_index(
    const LinkSessionTrace *trace, uint8_t pid)
{
    const LinkParameterKey key = standard_obd_key(pid);
    return link_session_trace_graph_key_index(trace, &key);
}

void link_session_trace_reset_graph(LinkSessionTrace *trace)
{
    if (trace == NULL) return;
    memset(trace->graph_history, 0, sizeof(trace->graph_history));
    memset(trace->graph_history_count, 0, sizeof(trace->graph_history_count));
    memset(trace->graph_history_next, 0, sizeof(trace->graph_history_next));
}

void link_session_trace_record_parameter(
    LinkSessionTrace *trace,
    const LinkParameterKey *key,
    double value)
{
    size_t graph;
    uint8_t slot;
    if (trace == NULL || !link_parameter_key_is_valid(key))
        return;
    graph = link_session_trace_graph_key_index(trace, key);
    if (graph >= trace->graph_count) return;

    slot = trace->graph_history_next[graph];
    trace->graph_history[graph][slot] = value;
    trace->graph_history_next[graph] = (uint8_t)(
        (slot + 1U) % LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY);
    if (trace->graph_history_count[graph] <
        LINK_SESSION_TRACE_GRAPH_HISTORY_CAPACITY) {
        ++trace->graph_history_count[graph];
    }
}

void link_session_trace_record_graph(
    LinkSessionTrace *trace, uint8_t pid, double value)
{
    const LinkParameterKey key = standard_obd_key(pid);
    link_session_trace_record_parameter(trace, &key, value);
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
