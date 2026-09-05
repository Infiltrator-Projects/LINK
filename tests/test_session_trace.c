// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/session_trace.h"
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL: %s\n", #x); return 1; } } while (0)

int main(void)
{
    static const uint8_t pids[] = {0x0cU, 0x0dU};
    LinkSessionTrace trace;
    char spark[128];

    CHECK(link_session_trace_init(&trace, pids, 2U));
    CHECK(link_session_trace_graph_index(&trace, 0x0cU) == 0U);
    CHECK(link_session_trace_graph_index(&trace, 0x0dU) == 1U);
    CHECK(link_session_trace_graph_index(&trace, 0x05U) == 2U);

    link_session_trace_record_graph(&trace, 0x0cU, 10.0);
    link_session_trace_record_graph(&trace, 0x0cU, 20.0);
    CHECK(trace.graph_history_count[0] == 2U);
    link_session_trace_format_sparkline(
        trace.graph_history[0], trace.graph_history_count[0],
        trace.graph_history_next[0], spark, sizeof(spark));
    CHECK(spark[0] != '\0');

    link_session_trace_clear_log(&trace, 1000U);
    link_session_trace_append_log(&trace, 1200U, "first");
    link_session_trace_append_log(&trace, 1400U, "second");
    CHECK(trace.session_log_count == 2U);
    CHECK(link_session_trace_log_ordered_slot(&trace, 0U) == 0U);
    CHECK(strcmp(trace.session_log[0], "first") == 0);
    CHECK(trace.session_log_time_ms[1] == 400U);

    CHECK(strcmp(
        link_diagnostic_flow_event_text(
            LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE),
        "Standard PID discovery complete") == 0);
    CHECK(link_diagnostic_flow_event_text(
        LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE) == NULL);

    puts("LINK shared session trace passed");
    return 0;
}
