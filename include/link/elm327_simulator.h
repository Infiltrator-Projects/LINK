// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file elm327_simulator.h
 * @brief Deterministic in-process ELM327 transport for end-to-end diagnostics testing.
 *
 * The simulator implements the same LinkTransport byte-stream ABI as a real
 * adapter provider. Callers therefore exercise normal ELM command framing,
 * response parsing, session state, OBD/UDS decoding, scheduling, telemetry and
 * evidence paths while deliberately bypassing only the physical transport.
 */
#ifndef LINK_ELM327_SIMULATOR_H
#define LINK_ELM327_SIMULATOR_H

#include "link/elm327.h"
#include "link/transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef bool (*LinkElm327SimulatorCustomResponderFn)(
    void *context,
    const char *canonical_command,
    char *response_body,
    size_t response_body_size);

typedef struct LinkElm327SimulatorConfig {
    const char *adapter_identifier;
    const char *vin;
    LinkElm327SimulatorCustomResponderFn custom_responder;
    void *custom_context;
} LinkElm327SimulatorConfig;

#define LINK_ELM327_SIMULATOR_CONFIG_INIT \
    { .adapter_identifier = NULL, .vin = NULL, \
      .custom_responder = NULL, .custom_context = NULL }

typedef struct LinkElm327Simulator {
    LinkElm327SimulatorConfig config;
    LinkTransportReceiveFn receiver;
    void *receiver_context;
    bool connected;
    bool echo;
    char command[LINK_ELM327_MAX_COMMAND];
    size_t command_length;
    uint64_t sample_counter;
} LinkElm327Simulator;

/** Initialise a deterministic ELM327 simulator instance. */
void link_elm327_simulator_init(
    LinkElm327Simulator *simulator,
    const LinkElm327SimulatorConfig *config);

/** Return a LinkTransport descriptor backed by `simulator`. */
LinkTransport link_elm327_simulator_transport(LinkElm327Simulator *simulator);

#ifdef __cplusplus
}
#endif

#endif
