// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file elm327_can.h
 * @brief ELM327-managed ISO 15765 CAN channel contracts.
 *
 * This path is for adapters that perform ISO-TP framing/flow control internally.
 * Raw CAN providers use link/isotp.h instead. UDS remains above both paths and
 * consumes complete PDUs.
 */
#ifndef LINK_ELM327_CAN_H
#define LINK_ELM327_CAN_H

#include "link/elm327.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_ELM327_CAN_MAX_REQUEST_PDU 31U

typedef struct LinkElm327CanChannelConfig {
    uint32_t tx_can_id;
    uint32_t rx_can_id;
    bool extended_id;
} LinkElm327CanChannelConfig;

typedef enum LinkElm327CanStage {
    LINK_ELM327_CAN_STAGE_SET_HEADER = 0,
    LINK_ELM327_CAN_STAGE_SET_RECEIVE_ADDRESS,
    LINK_ELM327_CAN_STAGE_ENABLE_AUTO_FORMATTING,
    LINK_ELM327_CAN_STAGE_ENABLE_FLOW_CONTROL,
    LINK_ELM327_CAN_STAGE_COMPLETE,
    LINK_ELM327_CAN_STAGE_FAILED
} LinkElm327CanStage;

typedef enum LinkElm327CanResult {
    LINK_ELM327_CAN_RESULT_OK = 0,
    LINK_ELM327_CAN_RESULT_INVALID_ARGUMENT,
    LINK_ELM327_CAN_RESULT_BUFFER_TOO_SMALL,
    LINK_ELM327_CAN_RESULT_PDU_TOO_LARGE,
    LINK_ELM327_CAN_RESULT_ELM_ERROR,
    LINK_ELM327_CAN_RESULT_MALFORMED_RESPONSE,
    LINK_ELM327_CAN_RESULT_UNEXPECTED_RESPONSE,
    LINK_ELM327_CAN_RESULT_FAILED_STATE
} LinkElm327CanResult;

typedef struct LinkElm327CanChannelState {
    LinkElm327CanChannelConfig config;
    LinkElm327CanStage stage;
    LinkElm327CanResult failure;
    LinkElm327Result elm_failure;
} LinkElm327CanChannelState;

const char *link_elm327_can_result_name(LinkElm327CanResult result);
const char *link_elm327_can_stage_name(LinkElm327CanStage stage);
bool link_elm327_can_channel_config_is_valid(
    const LinkElm327CanChannelConfig *config);
LinkElm327CanResult link_elm327_can_channel_begin(
    LinkElm327CanChannelState *state,
    const LinkElm327CanChannelConfig *config);
LinkElm327CanResult link_elm327_can_channel_command(
    const LinkElm327CanChannelState *state,
    char *buffer,
    size_t buffer_size);
LinkElm327CanResult link_elm327_can_channel_accept(
    LinkElm327CanChannelState *state,
    const LinkElm327Response *response);

/** Render a complete PDU as the hex command consumed by ELM auto-formatting. */
LinkElm327CanResult link_elm327_can_build_pdu_command(
    const uint8_t *pdu,
    size_t pdu_length,
    char *buffer,
    size_t buffer_size,
    size_t *written);

/** Decode a complete PDU from single-line or indexed ELM output. */
LinkElm327CanResult link_elm327_can_decode_pdu(
    const LinkElm327Response *response,
    uint8_t *pdu,
    size_t pdu_size,
    size_t *pdu_length);

#ifdef __cplusplus
}
#endif

#endif
