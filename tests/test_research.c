/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "link/research.h"

#include <stdio.h>
#include <string.h>

static int require_true(int condition, const char *message)
{
    if (condition) return 0;
    (void)fprintf(stderr, "%s\n", message);
    return 1;
}

int main(void)
{
    LinkResearchState state;
    int failures = 0;

    link_research_init(&state);
    failures += require_true(!state.active, "initial state must be inactive");
    failures += require_true(
        state.phase == LINK_RESEARCH_PHASE_IDLE,
        "initial phase must be idle");
    failures += require_true(
        strcmp(link_research_phase_name(LINK_RESEARCH_PHASE_PASSIVE_CAPTURE),
               "passive-capture") == 0,
        "passive phase name mismatch");

    failures += require_true(
        link_research_begin(&state, UINT64_C(1000)),
        "research session must begin");
    failures += require_true(
        link_research_set_phase(
            &state, LINK_RESEARCH_PHASE_PASSIVE_CAPTURE, UINT64_C(1100)),
        "passive phase transition failed");
    failures += require_true(
        link_research_record_frame(&state, LINK_RESEARCH_DIRECTION_RX),
        "rx frame was not counted");
    failures += require_true(
        link_research_record_frame(&state, LINK_RESEARCH_DIRECTION_TX),
        "tx frame was not counted");
    failures += require_true(
        link_research_mark_event(&state) == 1U,
        "first event marker index must be one");
    failures += require_true(
        link_research_set_phase(
            &state, LINK_RESEARCH_PHASE_STANDARD_INVENTORY, UINT64_C(1200)),
        "inventory phase transition failed");
    failures += require_true(
        !link_research_set_phase(
            &state, LINK_RESEARCH_PHASE_PAUSED, UINT64_C(900)),
        "phase timestamp must not move backwards");
    failures += require_true(
        state.frame_count == 2U &&
        state.rx_frame_count == 1U &&
        state.tx_frame_count == 1U,
        "research frame counters mismatch");
    failures += require_true(
        state.phase_transition_count == 2U,
        "research phase transition count mismatch");
    failures += require_true(
        link_research_finish(&state, UINT64_C(1300)),
        "research session must finish");
    failures += require_true(
        !state.active && state.phase == LINK_RESEARCH_PHASE_COMPLETE,
        "finished research session state mismatch");
    failures += require_true(
        !link_research_record_frame(&state, LINK_RESEARCH_DIRECTION_RX),
        "finished session must reject new frames");

    return failures == 0 ? 0 : 1;
}
