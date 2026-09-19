/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef LINK_RESEARCH_H
#define LINK_RESEARCH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkResearchPhase {
    LINK_RESEARCH_PHASE_IDLE = 0,
    LINK_RESEARCH_PHASE_PASSIVE_CAPTURE,
    LINK_RESEARCH_PHASE_STANDARD_INVENTORY,
    LINK_RESEARCH_PHASE_MANUFACTURER_SWEEP,
    LINK_RESEARCH_PHASE_PAUSED,
    LINK_RESEARCH_PHASE_COMPLETE
} LinkResearchPhase;

typedef enum LinkResearchDirection {
    LINK_RESEARCH_DIRECTION_TX = 0,
    LINK_RESEARCH_DIRECTION_RX = 1
} LinkResearchDirection;

typedef struct LinkResearchState {
    bool active;
    LinkResearchPhase phase;
    uint64_t started_ns;
    uint64_t phase_started_ns;
    size_t phase_transition_count;
    size_t frame_count;
    size_t tx_frame_count;
    size_t rx_frame_count;
    size_t event_count;
} LinkResearchState;

const char *link_research_phase_name(LinkResearchPhase phase);
void link_research_init(LinkResearchState *state);
bool link_research_begin(LinkResearchState *state, uint64_t timestamp_ns);
bool link_research_set_phase(LinkResearchState *state,
                             LinkResearchPhase phase,
                             uint64_t timestamp_ns);
bool link_research_record_frame(LinkResearchState *state,
                                LinkResearchDirection direction);
size_t link_research_mark_event(LinkResearchState *state);
bool link_research_finish(LinkResearchState *state, uint64_t timestamp_ns);

#ifdef __cplusplus
}
#endif

#endif
