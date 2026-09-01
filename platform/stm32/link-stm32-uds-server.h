// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32-uds-server.h
 * @brief Allocation-free STM32 ISO-TP + UDS ECU/server orchestration.
 */
#ifndef LINK_STM32_UDS_SERVER_H
#define LINK_STM32_UDS_SERVER_H

#include "link-stm32-can.h"
#include "link/uds_server.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    LinkIsoTpAddress address;
    uint8_t rx_block_size;
    uint8_t rx_stmin;
    uint64_t consecutive_timeout_us;
    uint64_t flow_control_timeout_us;
    uint8_t max_wait_frames;
    bool can_fd;
    uint8_t data_length;
    bool pad_short_frames;
    uint8_t padding_byte;
} LinkStm32UdsServerConfig;

typedef enum {
    LINK_STM32_UDS_SERVER_IDLE = 0,
    LINK_STM32_UDS_SERVER_ACTIVE,
    LINK_STM32_UDS_SERVER_FAILED
} LinkStm32UdsServerState;

typedef enum {
    LINK_STM32_UDS_SERVER_RESULT_OK = 0,
    LINK_STM32_UDS_SERVER_RESULT_WAITING,
    LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE,
    LINK_STM32_UDS_SERVER_RESULT_INVALID_ARGUMENT,
    LINK_STM32_UDS_SERVER_RESULT_CAN_IO,
    LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR,
    LINK_STM32_UDS_SERVER_RESULT_UDS_ERROR,
    LINK_STM32_UDS_SERVER_RESULT_FAILED_STATE
} LinkStm32UdsServerResult;

typedef struct {
    LinkStm32Can *channel;
    LinkUdsServer *server;
    LinkStm32UdsServerConfig config;
    LinkIsoTpRx receiver;
    LinkIsoTpTx transmitter;
    uint8_t *rx_storage;
    size_t rx_capacity;
    uint8_t *tx_storage;
    size_t tx_capacity;
    LinkIsoTpCanFrame pending_tx;
    bool pending_tx_valid;
    bool pending_tx_tracks_transmitter;
    bool in_flight_tracks_transmitter;
    bool response_active;
    LinkIsoTpCanFrame deferred_rx;
    uint64_t deferred_rx_arrival_us;
    bool deferred_rx_valid;
    uint32_t deferred_rx_dropped;
    uint64_t tx_completion_deadline_us;
    LinkIsoTpResult isotp_result;
    LinkUdsServerResult uds_result;
    LinkStm32UdsServerState state;
    LinkStm32UdsServerResult failure;
    uint32_t completed_request_count;
} LinkStm32UdsServer;

bool link_stm32_uds_server_init(
    LinkStm32UdsServer *transport,
    LinkStm32Can *channel,
    LinkUdsServer *server,
    const LinkStm32UdsServerConfig *config,
    uint8_t *rx_storage,
    size_t rx_capacity,
    uint8_t *tx_storage,
    size_t tx_capacity);
void link_stm32_uds_server_reset(LinkStm32UdsServer *transport);
LinkStm32UdsServerResult link_stm32_uds_server_poll(
    LinkStm32UdsServer *transport);
LinkStm32UdsServerState link_stm32_uds_server_state(
    const LinkStm32UdsServer *transport);
uint32_t link_stm32_uds_server_completed_requests(
    const LinkStm32UdsServer *transport);
uint32_t link_stm32_uds_server_deferred_rx_dropped(
    const LinkStm32UdsServer *transport);

#ifdef __cplusplus
}
#endif

#endif
