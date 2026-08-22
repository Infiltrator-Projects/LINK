// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file isotp.h
 * @brief Portable ISO-TP (ISO 15765-2) transport-layer foundation.
 *
 * Classical-CAN buffers are caller-owned and all protocol state is bounded.
 * Timing values use one caller-supplied monotonic microsecond clock.
 */
#ifndef LINK_ISOTP_H
#define LINK_ISOTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH 8U
#define LINK_ISOTP_MAX_PDU_LENGTH 4095U

typedef enum {
    LINK_ISOTP_ADDRESSING_NORMAL = 0,
    LINK_ISOTP_ADDRESSING_EXTENDED,
    LINK_ISOTP_ADDRESSING_MIXED
} LinkIsoTpAddressingMode;

typedef enum {
    LINK_ISOTP_TARGET_PHYSICAL = 0,
    LINK_ISOTP_TARGET_FUNCTIONAL
} LinkIsoTpTargetType;

typedef enum {
    LINK_ISOTP_FLOW_CONTINUE_TO_SEND = 0,
    LINK_ISOTP_FLOW_WAIT = 1,
    LINK_ISOTP_FLOW_OVERFLOW = 2
} LinkIsoTpFlowStatus;

typedef enum {
    LINK_ISOTP_RESULT_OK = 0,
    LINK_ISOTP_RESULT_COMPLETE,
    LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL,
    LINK_ISOTP_RESULT_WAIT_SEPARATION,
    LINK_ISOTP_RESULT_FLOW_CONTROL_WAIT,
    LINK_ISOTP_RESULT_INVALID_ARGUMENT,
    LINK_ISOTP_RESULT_INVALID_FRAME,
    LINK_ISOTP_RESULT_UNEXPECTED_FRAME,
    LINK_ISOTP_RESULT_WRONG_SEQUENCE,
    LINK_ISOTP_RESULT_BUFFER_TOO_SMALL,
    LINK_ISOTP_RESULT_PAYLOAD_TOO_LARGE,
    LINK_ISOTP_RESULT_TIMEOUT,
    LINK_ISOTP_RESULT_FLOW_CONTROL_OVERFLOW,
    LINK_ISOTP_RESULT_WAIT_FRAME_LIMIT,
    LINK_ISOTP_RESULT_UNSUPPORTED
} LinkIsoTpResult;

typedef enum {
    LINK_ISOTP_RX_IDLE = 0,
    LINK_ISOTP_RX_RECEIVING,
    LINK_ISOTP_RX_COMPLETE,
    LINK_ISOTP_RX_FAILED
} LinkIsoTpRxState;

typedef enum {
    LINK_ISOTP_TX_IDLE = 0,
    LINK_ISOTP_TX_WAIT_FLOW_CONTROL,
    LINK_ISOTP_TX_SENDING,
    LINK_ISOTP_TX_COMPLETE,
    LINK_ISOTP_TX_FAILED
} LinkIsoTpTxState;

typedef struct {
    uint32_t tx_can_id;
    uint32_t rx_can_id;
    bool tx_extended_id;
    bool rx_extended_id;
    LinkIsoTpAddressingMode addressing_mode;
    LinkIsoTpTargetType target_type;
    uint8_t tx_address_extension;
    uint8_t rx_address_extension;
} LinkIsoTpAddress;

typedef struct {
    uint32_t can_id;
    bool extended_id;
    uint8_t length;
    uint8_t data[LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH];
} LinkIsoTpCanFrame;

typedef struct {
    LinkIsoTpAddress address;
    uint8_t block_size;
    uint8_t stmin;
    uint64_t consecutive_timeout_us;
} LinkIsoTpRxConfig;

typedef struct {
    LinkIsoTpRxConfig config;
    uint8_t *buffer;
    size_t capacity;
    size_t expected_length;
    size_t received_length;
    uint8_t next_sequence;
    uint8_t block_counter;
    uint64_t deadline_us;
    LinkIsoTpRxState state;
    LinkIsoTpResult failure;
} LinkIsoTpRx;

typedef struct {
    LinkIsoTpAddress address;
    uint64_t flow_control_timeout_us;
    uint8_t max_wait_frames;
} LinkIsoTpTxConfig;

typedef struct {
    LinkIsoTpTxConfig config;
    const uint8_t *payload;
    size_t payload_length;
    size_t offset;
    uint8_t next_sequence;
    uint8_t block_size;
    uint8_t block_counter;
    uint8_t wait_frame_count;
    uint32_t separation_time_us;
    uint64_t next_send_us;
    uint64_t deadline_us;
    LinkIsoTpTxState state;
    LinkIsoTpResult failure;
} LinkIsoTpTx;

const char *link_isotp_result_name(LinkIsoTpResult result);
const char *link_isotp_rx_state_name(LinkIsoTpRxState state);
const char *link_isotp_tx_state_name(LinkIsoTpTxState state);

bool link_isotp_address_is_valid(const LinkIsoTpAddress *address);
bool link_isotp_stmin_to_us(uint8_t stmin, uint32_t *microseconds);

LinkIsoTpResult link_isotp_rx_init(
    LinkIsoTpRx *receiver,
    const LinkIsoTpRxConfig *config,
    uint8_t *buffer,
    size_t capacity);

void link_isotp_rx_reset(LinkIsoTpRx *receiver);

LinkIsoTpResult link_isotp_rx_feed(
    LinkIsoTpRx *receiver,
    const LinkIsoTpCanFrame *frame,
    uint64_t now_us,
    LinkIsoTpCanFrame *flow_control_frame,
    bool *flow_control_ready);

LinkIsoTpResult link_isotp_rx_tick(
    LinkIsoTpRx *receiver,
    uint64_t now_us);

const uint8_t *link_isotp_rx_payload(
    const LinkIsoTpRx *receiver,
    size_t *length);

LinkIsoTpResult link_isotp_tx_init(
    LinkIsoTpTx *transmitter,
    const LinkIsoTpTxConfig *config,
    const uint8_t *payload,
    size_t payload_length);

void link_isotp_tx_reset(LinkIsoTpTx *transmitter);

LinkIsoTpResult link_isotp_tx_start(
    LinkIsoTpTx *transmitter,
    uint64_t now_us,
    LinkIsoTpCanFrame *frame);

LinkIsoTpResult link_isotp_tx_accept_flow_control(
    LinkIsoTpTx *transmitter,
    const LinkIsoTpCanFrame *frame,
    uint64_t now_us);

LinkIsoTpResult link_isotp_tx_next(
    LinkIsoTpTx *transmitter,
    uint64_t now_us,
    LinkIsoTpCanFrame *frame);

LinkIsoTpResult link_isotp_tx_tick(
    LinkIsoTpTx *transmitter,
    uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif
