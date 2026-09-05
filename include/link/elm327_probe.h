// SPDX-License-Identifier: GPL-3.0-or-later
/** @file elm327_probe.h @brief Deterministic ELM adapter/protocol probing. */
#ifndef LINK_ELM327_PROBE_H
#define LINK_ELM327_PROBE_H

#include "link/elm327.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_ELM327_MAX_DEVICE_DESCRIPTION 96U
#define LINK_ELM327_MAX_PROTOCOL_DESCRIPTION 128U

typedef enum LinkElm327ProbeStage {
    LINK_ELM327_PROBE_DEVICE_DESCRIPTION = 0,
    LINK_ELM327_PROBE_PROTOCOL_DESCRIPTION,
    LINK_ELM327_PROBE_PROTOCOL_NUMBER,
    LINK_ELM327_PROBE_COMPLETE,
    LINK_ELM327_PROBE_FAILED
} LinkElm327ProbeStage;

typedef struct LinkElm327ProbeState {
    LinkElm327ProbeStage stage;
    LinkElm327Result failure;
    bool device_description_supported;
    bool protocol_was_automatic;
    uint8_t protocol_number;
    char device_description[LINK_ELM327_MAX_DEVICE_DESCRIPTION];
    char protocol_description[LINK_ELM327_MAX_PROTOCOL_DESCRIPTION];
} LinkElm327ProbeState;

void link_elm327_probe_begin(LinkElm327ProbeState *state);
/** Begin only the active-protocol query (ATDP -> ATDPN), skipping AT@1. */
void link_elm327_probe_begin_protocol(LinkElm327ProbeState *state);
const char *link_elm327_probe_command(const LinkElm327ProbeState *state);

/**
 * Accept AT@1 -> ATDP -> ATDPN responses. AT@1 is optional; ATDP/ATDPN are
 * required because the active OBD protocol must be established explicitly.
 */
LinkElm327Result link_elm327_probe_accept(
    LinkElm327ProbeState *state,
    const LinkElm327Response *response);

#ifdef __cplusplus
}
#endif

#endif
