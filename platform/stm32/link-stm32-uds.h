// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32-uds.h
 * @brief Bare-metal STM32 orchestration for LINK ISO-TP + UDS client traffic.
 */
#ifndef LINK_STM32_UDS_H
#define LINK_STM32_UDS_H

#include "link-stm32-can.h"
#include "link/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_STM32_UDS_IDLE = 0,
    LINK_STM32_UDS_ACTIVE,
    LINK_STM32_UDS_COMPLETE,
    LINK_STM32_UDS_FAILED
} LinkStm32UdsState;

typedef enum {
    LINK_STM32_UDS_RESULT_OK = 0,
    LINK_STM32_UDS_RESULT_WAITING,
    LINK_STM32_UDS_RESULT_COMPLETE,
    LINK_STM32_UDS_RESULT_NEGATIVE_RESPONSE,
    LINK_STM32_UDS_RESULT_INVALID_ARGUMENT,
    LINK_STM32_UDS_RESULT_BUSY,
    LINK_STM32_UDS_RESULT_CAN_IO,
    LINK_STM32_UDS_RESULT_ISOTP_ERROR,
    LINK_STM32_UDS_RESULT_UDS_ERROR
} LinkStm32UdsResult;

typedef struct {
    LinkIsoTpAddress address;
    uint8_t rx_block_size;
    uint8_t rx_stmin;
    uint64_t consecutive_timeout_us;
    uint64_t flow_control_timeout_us;
    uint8_t max_wait_frames;
    uint64_t p2_timeout_us;
    uint64_t p2_star_timeout_us;
    bool can_fd;
    uint8_t data_length;
} LinkStm32UdsConfig;

typedef struct {
    LinkStm32Can *channel;
    LinkStm32UdsConfig config;
    LinkIsoTpRx receiver;
    LinkIsoTpTx transmitter;
    LinkUdsClient uds;
    uint8_t *rx_storage;
    size_t rx_capacity;
    uint8_t *tx_storage;
    size_t tx_capacity;
    size_t request_length;
    LinkIsoTpCanFrame pending_tx;
    bool pending_tx_valid;
    bool pending_tx_tracks_transmitter;
    bool in_flight_tracks_transmitter;
    uint64_t tx_completion_deadline_us;
    uint64_t last_transmitter_completion_us;
    bool last_transmitter_completion_valid;
    bool uds_started;
    LinkUdsResponse response;
    LinkIsoTpResult isotp_result;
    LinkUdsResult uds_result;
    LinkStm32UdsResult failure;
    LinkStm32UdsState state;
} LinkStm32UdsClient;

bool link_stm32_uds_init(
    LinkStm32UdsClient *client,
    LinkStm32Can *channel,
    const LinkStm32UdsConfig *config,
    uint8_t *rx_storage,
    size_t rx_capacity,
    uint8_t *tx_storage,
    size_t tx_capacity);

void link_stm32_uds_reset(LinkStm32UdsClient *client);

LinkStm32UdsResult link_stm32_uds_start(
    LinkStm32UdsClient *client,
    const uint8_t *request,
    size_t request_length);

LinkStm32UdsResult link_stm32_uds_poll(LinkStm32UdsClient *client);
const LinkUdsResponse *link_stm32_uds_response(const LinkStm32UdsClient *client);

#ifdef __cplusplus
}
#endif

#endif
