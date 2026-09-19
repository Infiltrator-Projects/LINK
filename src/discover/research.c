// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/research.h"

#include <string.h>

static bool link_research_phase_is_valid(LinkResearchPhase phase)
{
    return phase >= LINK_RESEARCH_PHASE_IDLE &&
           phase <= LINK_RESEARCH_PHASE_COMPLETE;
}

const char *link_research_phase_name(LinkResearchPhase phase)
{
    switch (phase) {
    case LINK_RESEARCH_PHASE_IDLE: return "idle";
    case LINK_RESEARCH_PHASE_PASSIVE_CAPTURE: return "passive-capture";
    case LINK_RESEARCH_PHASE_STANDARD_INVENTORY: return "standard-inventory";
    case LINK_RESEARCH_PHASE_MANUFACTURER_SWEEP: return "manufacturer-sweep";
    case LINK_RESEARCH_PHASE_PAUSED: return "paused";
    case LINK_RESEARCH_PHASE_COMPLETE: return "complete";
    }
    return "unknown";
}

void link_research_init(LinkResearchState *state)
{
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->phase = LINK_RESEARCH_PHASE_IDLE;
}

bool link_research_begin(LinkResearchState *state, uint64_t timestamp_ns)
{
    if (state == NULL) return false;
    link_research_init(state);
    state->active = true;
    state->started_ns = timestamp_ns;
    state->phase_started_ns = timestamp_ns;
    return true;
}

bool link_research_set_phase(LinkResearchState *state,
                             LinkResearchPhase phase,
                             uint64_t timestamp_ns)
{
    if (state == NULL || !state->active ||
        !link_research_phase_is_valid(phase) ||
        timestamp_ns < state->started_ns ||
        timestamp_ns < state->phase_started_ns) {
        return false;
    }
    if (state->phase == phase) return true;
    if (state->phase_transition_count == SIZE_MAX) return false;
    state->phase = phase;
    state->phase_started_ns = timestamp_ns;
    ++state->phase_transition_count;
    return true;
}

bool link_research_record_frame(LinkResearchState *state,
                                LinkResearchDirection direction)
{
    if (state == NULL || !state->active ||
        (direction != LINK_RESEARCH_DIRECTION_TX &&
         direction != LINK_RESEARCH_DIRECTION_RX) ||
        state->frame_count == SIZE_MAX) {
        return false;
    }
    if (direction == LINK_RESEARCH_DIRECTION_TX) {
        if (state->tx_frame_count == SIZE_MAX) return false;
        ++state->tx_frame_count;
    } else {
        if (state->rx_frame_count == SIZE_MAX) return false;
        ++state->rx_frame_count;
    }
    ++state->frame_count;
    return true;
}

size_t link_research_mark_event(LinkResearchState *state)
{
    if (state == NULL || !state->active || state->event_count == SIZE_MAX)
        return 0U;
    ++state->event_count;
    return state->event_count;
}

bool link_research_finish(LinkResearchState *state, uint64_t timestamp_ns)
{
    if (!link_research_set_phase(
            state, LINK_RESEARCH_PHASE_COMPLETE, timestamp_ns)) {
        return false;
    }
    state->active = false;
    return true;
}
