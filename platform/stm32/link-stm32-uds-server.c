// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-uds-server.c @brief STM32 LINK UDS ECU/server transport. */
#include "link-stm32-uds-server.h"

#include <string.h>

static bool link_stm32_uds_server_config_valid(
    const LinkStm32UdsServerConfig *config)
{
    if (config == NULL ||
        !link_isotp_address_is_valid(&config->address) ||
        config->address.target_type != LINK_ISOTP_TARGET_PHYSICAL ||
        config->consecutive_timeout_us == 0U ||
        config->flow_control_timeout_us == 0U ||
        !link_isotp_can_data_length_is_valid(
            config->can_fd, config->data_length)) {
        return false;
    }
    if (!config->functional_address_enabled) return true;
    return link_isotp_address_is_valid(&config->functional_address) &&
           config->functional_address.target_type ==
               LINK_ISOTP_TARGET_FUNCTIONAL &&
           config->functional_address.tx_can_id ==
               config->address.tx_can_id &&
           config->functional_address.tx_extended_id ==
               config->address.tx_extended_id &&
           config->functional_address.addressing_mode ==
               config->address.addressing_mode;
}

static uint64_t link_stm32_uds_server_deadline(
    uint64_t now_us, uint64_t delta_us)
{
    return UINT64_MAX - now_us < delta_us ? UINT64_MAX : now_us + delta_us;
}

static LinkStm32UdsServerResult link_stm32_uds_server_fail(
    LinkStm32UdsServer *transport,
    LinkStm32UdsServerResult failure)
{
    if (transport != NULL) {
        transport->state = LINK_STM32_UDS_SERVER_FAILED;
        transport->failure = failure;
        transport->pending_tx_valid = false;
        transport->pending_tx_tracks_transmitter = false;
        transport->in_flight_tracks_transmitter = false;
        transport->response_active = false;
        transport->deferred_rx_head = 0U;
        transport->deferred_rx_tail = 0U;
        transport->tx_completion_deadline_us = 0U;
    }
    return failure;
}

static bool link_stm32_uds_server_is_flow_control(
    const LinkStm32UdsServer *transport,
    const LinkIsoTpCanFrame *frame)
{
    size_t offset;
    if (transport == NULL || frame == NULL || !transport->response_active ||
        transport->transmitter.state != LINK_ISOTP_TX_WAIT_FLOW_CONTROL ||
        frame->can_id != transport->config.address.rx_can_id ||
        frame->extended_id != transport->config.address.rx_extended_id) {
        return false;
    }
    offset = transport->config.address.addressing_mode ==
             LINK_ISOTP_ADDRESSING_NORMAL ? 0U : 1U;
    if (frame->length <= offset) return false;
    if (offset != 0U &&
        frame->data[0] != transport->config.address.rx_address_extension) {
        return false;
    }
    return (frame->data[offset] >> 4U) == 0x3U;
}

static LinkIsoTpResult link_stm32_uds_server_confirm_transmitter_sent(
    LinkStm32UdsServer *transport,
    uint64_t sent_us)
{
    LinkIsoTpTx *tx = &transport->transmitter;
    if (tx->state == LINK_ISOTP_TX_FAILED) return tx->failure;
    if (tx->state == LINK_ISOTP_TX_WAIT_FLOW_CONTROL) {
        tx->deadline_us = link_stm32_uds_server_deadline(
            sent_us, tx->config.flow_control_timeout_us);
        return LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL;
    }
    if (tx->state == LINK_ISOTP_TX_SENDING) {
        tx->next_send_us = link_stm32_uds_server_deadline(
            sent_us, tx->separation_time_us);
        return tx->separation_time_us == 0U
            ? LINK_ISOTP_RESULT_OK : LINK_ISOTP_RESULT_WAIT_SEPARATION;
    }
    if (tx->state == LINK_ISOTP_TX_COMPLETE) return LINK_ISOTP_RESULT_COMPLETE;
    return LINK_ISOTP_RESULT_UNEXPECTED_FRAME;
}

static LinkStm32UdsServerResult link_stm32_uds_server_send_pending(
    LinkStm32UdsServer *transport)
{
    bool tracks_transmitter;
    uint64_t now_us;

    if (!transport->pending_tx_valid) return LINK_STM32_UDS_SERVER_RESULT_OK;
    if (!link_stm32_can_tx_ready(transport->channel)) {
        return LINK_STM32_UDS_SERVER_RESULT_WAITING;
    }
    tracks_transmitter = transport->pending_tx_tracks_transmitter;
    if (!link_stm32_can_send(transport->channel, &transport->pending_tx)) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_CAN_IO);
    }
    transport->pending_tx_valid = false;
    transport->pending_tx_tracks_transmitter = false;
    transport->in_flight_tracks_transmitter = tracks_transmitter;
    now_us = link_stm32_can_now_us(transport->channel);
    transport->tx_completion_deadline_us = link_stm32_uds_server_deadline(
        now_us, transport->config.flow_control_timeout_us);
    return LINK_STM32_UDS_SERVER_RESULT_OK;
}

static LinkStm32UdsServerResult link_stm32_uds_server_finish_response(
    LinkStm32UdsServer *transport)
{
    transport->response_active = false;
    transport->tx_completion_deadline_us = 0U;
    link_isotp_tx_reset(&transport->transmitter);
    link_isotp_rx_reset(&transport->receiver);
    transport->completed_request_count++;
    return LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE;
}

static LinkStm32UdsServerResult link_stm32_uds_server_poll_tx_completion(
    LinkStm32UdsServer *transport)
{
    LinkStm32CanTxStatus status;
    uint64_t completion_us = 0U;
    uint64_t now_us;
    bool tracks_transmitter;

    if (!link_stm32_can_tx_in_flight(transport->channel)) {
        transport->in_flight_tracks_transmitter = false;
        transport->tx_completion_deadline_us = 0U;
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }
    status = link_stm32_can_poll_tx_status(
        transport->channel, &completion_us);
    if (status == LINK_STM32_CAN_TX_PENDING) {
        now_us = link_stm32_can_now_us(transport->channel);
        if (transport->tx_completion_deadline_us != 0U &&
            now_us >= transport->tx_completion_deadline_us) {
            return link_stm32_uds_server_fail(
                transport, LINK_STM32_UDS_SERVER_RESULT_CAN_IO);
        }
        return LINK_STM32_UDS_SERVER_RESULT_WAITING;
    }
    if (status == LINK_STM32_CAN_TX_FAILED) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_CAN_IO);
    }
    if (status != LINK_STM32_CAN_TX_COMPLETE) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }

    tracks_transmitter = transport->in_flight_tracks_transmitter;
    transport->in_flight_tracks_transmitter = false;
    transport->tx_completion_deadline_us = 0U;
    if (!tracks_transmitter) return LINK_STM32_UDS_SERVER_RESULT_OK;

    transport->isotp_result =
        link_stm32_uds_server_confirm_transmitter_sent(
            transport, completion_us);
    if (transport->isotp_result == LINK_ISOTP_RESULT_COMPLETE) {
        return link_stm32_uds_server_finish_response(transport);
    }
    if (transport->isotp_result == LINK_ISOTP_RESULT_OK ||
        transport->isotp_result == LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL ||
        transport->isotp_result == LINK_ISOTP_RESULT_WAIT_SEPARATION) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }
    return link_stm32_uds_server_fail(
        transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
}

static LinkStm32UdsServerResult link_stm32_uds_server_begin_response(
    LinkStm32UdsServer *transport,
    size_t response_length,
    uint64_t now_us)
{
    LinkIsoTpTxConfig tx_config;

    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = transport->config.address;
    tx_config.flow_control_timeout_us =
        transport->config.flow_control_timeout_us;
    tx_config.max_wait_frames = transport->config.max_wait_frames;
    tx_config.can_fd = transport->config.can_fd;
    tx_config.data_length = transport->config.data_length;
    tx_config.pad_short_frames = transport->config.pad_short_frames;
    tx_config.padding_byte = transport->config.padding_byte;

    transport->isotp_result = link_isotp_tx_init(
        &transport->transmitter, &tx_config,
        transport->tx_storage, response_length);
    if (transport->isotp_result != LINK_ISOTP_RESULT_OK) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    transport->isotp_result = link_isotp_tx_start(
        &transport->transmitter, now_us, &transport->pending_tx);
    if (transport->isotp_result != LINK_ISOTP_RESULT_COMPLETE &&
        transport->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    transport->pending_tx_valid = true;
    transport->pending_tx_tracks_transmitter = true;
    transport->response_active = true;
    return link_stm32_uds_server_send_pending(transport);
}

static bool link_stm32_uds_server_functional_nrc_suppressed(
    const uint8_t *response,
    size_t response_length)
{
    uint8_t nrc;
    if (response == NULL || response_length < 3U ||
        response[0] != UINT8_C(0x7f)) {
        return false;
    }
    nrc = response[2];
    return nrc == UINT8_C(0x11) ||
           nrc == UINT8_C(0x12) ||
           nrc == UINT8_C(0x31) ||
           nrc == UINT8_C(0x7e) ||
           nrc == UINT8_C(0x7f);
}

static LinkStm32UdsServerResult link_stm32_uds_server_dispatch_payload(
    LinkStm32UdsServer *transport,
    const uint8_t *request,
    size_t request_length,
    bool functional_request,
    uint64_t now_us)
{
    size_t response_length = 0U;
    if (request == NULL || request_length == 0U) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    transport->uds_result = link_uds_server_handle(
        transport->server, request, request_length,
        transport->tx_storage, transport->tx_capacity, &response_length);
    if (transport->uds_result == LINK_UDS_SERVER_RESULT_INVALID_ARGUMENT ||
        transport->uds_result == LINK_UDS_SERVER_RESULT_BUFFER_TOO_SMALL) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_UDS_ERROR);
    }
    if (functional_request &&
        link_stm32_uds_server_functional_nrc_suppressed(
            transport->tx_storage, response_length)) {
        response_length = 0U;
    }
    if (response_length == 0U) {
        transport->completed_request_count++;
        return LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE;
    }
    return link_stm32_uds_server_begin_response(
        transport, response_length, now_us);
}

static LinkStm32UdsServerResult link_stm32_uds_server_dispatch(
    LinkStm32UdsServer *transport,
    uint64_t now_us)
{
    const uint8_t *request;
    size_t request_length = 0U;
    LinkStm32UdsServerResult result;
    request = link_isotp_rx_payload(&transport->receiver, &request_length);
    result = link_stm32_uds_server_dispatch_payload(
        transport, request, request_length, false, now_us);
    link_isotp_rx_reset(&transport->receiver);
    return result;
}

static bool link_stm32_uds_server_frame_matches_rx(
    const LinkIsoTpAddress *address,
    const LinkIsoTpCanFrame *frame,
    size_t *offset)
{
    size_t local_offset;
    if (address == NULL || frame == NULL ||
        frame->can_id != address->rx_can_id ||
        frame->extended_id != address->rx_extended_id) {
        return false;
    }
    local_offset = address->addressing_mode == LINK_ISOTP_ADDRESSING_NORMAL
        ? 0U : 1U;
    if (frame->length <= local_offset) return false;
    if (local_offset != 0U &&
        frame->data[0] != address->rx_address_extension) {
        return false;
    }
    if (offset != NULL) *offset = local_offset;
    return true;
}

static LinkStm32UdsServerResult link_stm32_uds_server_accept_functional(
    LinkStm32UdsServer *transport,
    const LinkIsoTpCanFrame *frame,
    uint64_t arrival_us)
{
    LinkIsoTpRx receiver;
    LinkIsoTpRxConfig rx_config;
    LinkIsoTpCanFrame unused_flow_control;
    bool flow_control_ready = false;
    const uint8_t *request;
    size_t request_length = 0U;
    size_t offset = 0U;
    LinkIsoTpResult result;

    if (!transport->config.functional_address_enabled ||
        !link_stm32_uds_server_frame_matches_rx(
            &transport->config.functional_address, frame, &offset)) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }
    if ((frame->data[offset] >> 4U) != 0U) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }

    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = transport->config.functional_address;
    rx_config.consecutive_timeout_us =
        transport->config.consecutive_timeout_us;
    rx_config.can_fd = transport->config.can_fd;
    rx_config.data_length = transport->config.data_length;
    rx_config.pad_short_frames = transport->config.pad_short_frames;
    rx_config.padding_byte = transport->config.padding_byte;

    result = link_isotp_rx_init(
        &receiver, &rx_config,
        transport->rx_storage, transport->rx_capacity);
    if (result != LINK_ISOTP_RESULT_OK) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    memset(&unused_flow_control, 0, sizeof(unused_flow_control));
    result = link_isotp_rx_feed(
        &receiver, frame, arrival_us,
        &unused_flow_control, &flow_control_ready);
    if (result != LINK_ISOTP_RESULT_COMPLETE || flow_control_ready) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }
    request = link_isotp_rx_payload(&receiver, &request_length);
    return link_stm32_uds_server_dispatch_payload(
        transport, request, request_length, true, arrival_us);
}

static bool link_stm32_uds_server_defer_rx(
    LinkStm32UdsServer *transport,
    const LinkIsoTpCanFrame *frame,
    uint64_t arrival_us)
{
    uint8_t head;
    size_t index;

    if (transport == NULL || frame == NULL) return false;
    head = transport->deferred_rx_head;
    if ((uint8_t)(head - transport->deferred_rx_tail) >=
        LINK_STM32_CAN_RX_QUEUE_CAPACITY) {
        transport->deferred_rx_dropped++;
        return false;
    }

    index = (size_t)(head % LINK_STM32_CAN_RX_QUEUE_CAPACITY);
    transport->deferred_rx[index] = *frame;
    transport->deferred_rx_arrival_us[index] = arrival_us;
    transport->deferred_rx_head = (uint8_t)(head + 1U);
    return true;
}

static bool link_stm32_uds_server_pop_deferred_rx(
    LinkStm32UdsServer *transport,
    LinkIsoTpCanFrame *frame,
    uint64_t *arrival_us)
{
    uint8_t tail;
    size_t index;

    if (transport == NULL || frame == NULL || arrival_us == NULL) return false;
    tail = transport->deferred_rx_tail;
    if (tail == transport->deferred_rx_head) return false;

    index = (size_t)(tail % LINK_STM32_CAN_RX_QUEUE_CAPACITY);
    *frame = transport->deferred_rx[index];
    *arrival_us = transport->deferred_rx_arrival_us[index];
    transport->deferred_rx_tail = (uint8_t)(tail + 1U);
    return true;
}

static LinkStm32UdsServerResult link_stm32_uds_server_accept_rx(
    LinkStm32UdsServer *transport,
    const LinkIsoTpCanFrame *frame,
    uint64_t arrival_us)
{
    LinkIsoTpCanFrame flow_control;
    bool flow_control_ready = false;

    if (link_stm32_uds_server_is_flow_control(transport, frame)) {
        transport->isotp_result = link_isotp_tx_accept_flow_control(
            &transport->transmitter, frame, arrival_us);
        if (transport->isotp_result == LINK_ISOTP_RESULT_OK ||
            transport->isotp_result == LINK_ISOTP_RESULT_FLOW_CONTROL_WAIT) {
            return LINK_STM32_UDS_SERVER_RESULT_OK;
        }
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    if (transport->response_active) {
        /*
         * A tester should not start a new diagnostic request until the current
         * response is complete, but real PCAN tools can race or burst several
         * requests around the final CAN TX completion. Preserve those frames in
         * a bounded FIFO instead of silently consuming everything after the
         * first raced request. FlowControl remains processable immediately.
         */
        (void)link_stm32_uds_server_defer_rx(
            transport, frame, arrival_us);
        return LINK_STM32_UDS_SERVER_RESULT_WAITING;
    }

    if (transport->config.functional_address_enabled &&
        link_stm32_uds_server_frame_matches_rx(
            &transport->config.functional_address, frame, NULL)) {
        return link_stm32_uds_server_accept_functional(
            transport, frame, arrival_us);
    }

    transport->isotp_result = link_isotp_rx_feed(
        &transport->receiver, frame, arrival_us,
        &flow_control, &flow_control_ready);
    if (flow_control_ready) {
        if (transport->pending_tx_valid ||
            link_stm32_can_tx_in_flight(transport->channel)) {
            return link_stm32_uds_server_fail(
                transport, LINK_STM32_UDS_SERVER_RESULT_CAN_IO);
        }
        transport->pending_tx = flow_control;
        transport->pending_tx_valid = true;
        transport->pending_tx_tracks_transmitter = false;
    }
    if (transport->isotp_result == LINK_ISOTP_RESULT_UNEXPECTED_FRAME) {
        return LINK_STM32_UDS_SERVER_RESULT_OK;
    }
    if (transport->isotp_result != LINK_ISOTP_RESULT_OK &&
        transport->isotp_result != LINK_ISOTP_RESULT_COMPLETE) {
        return link_stm32_uds_server_fail(
            transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
    }
    if (flow_control_ready) {
        LinkStm32UdsServerResult send_result =
            link_stm32_uds_server_send_pending(transport);
        if (send_result == LINK_STM32_UDS_SERVER_RESULT_CAN_IO) {
            return send_result;
        }
    }
    if (transport->isotp_result == LINK_ISOTP_RESULT_COMPLETE) {
        return link_stm32_uds_server_dispatch(transport, arrival_us);
    }
    return LINK_STM32_UDS_SERVER_RESULT_OK;
}

bool link_stm32_uds_server_init(
    LinkStm32UdsServer *transport,
    LinkStm32Can *channel,
    LinkUdsServer *server,
    const LinkStm32UdsServerConfig *config,
    uint8_t *rx_storage,
    size_t rx_capacity,
    uint8_t *tx_storage,
    size_t tx_capacity)
{
    LinkIsoTpRxConfig rx_config;

    if (transport == NULL || channel == NULL || server == NULL ||
        !link_stm32_uds_server_config_valid(config) ||
        rx_storage == NULL || rx_capacity == 0U ||
        tx_storage == NULL || tx_capacity == 0U) {
        return false;
    }
    memset(transport, 0, sizeof(*transport));
    transport->channel = channel;
    transport->server = server;
    transport->config = *config;
    transport->rx_storage = rx_storage;
    transport->rx_capacity = rx_capacity;
    transport->tx_storage = tx_storage;
    transport->tx_capacity = tx_capacity;

    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = config->address;
    rx_config.block_size = config->rx_block_size;
    rx_config.stmin = config->rx_stmin;
    rx_config.consecutive_timeout_us = config->consecutive_timeout_us;
    rx_config.can_fd = config->can_fd;
    rx_config.data_length = config->data_length;
    rx_config.pad_short_frames = config->pad_short_frames;
    rx_config.padding_byte = config->padding_byte;
    transport->isotp_result = link_isotp_rx_init(
        &transport->receiver, &rx_config, rx_storage, rx_capacity);
    if (transport->isotp_result != LINK_ISOTP_RESULT_OK) return false;
    transport->state = LINK_STM32_UDS_SERVER_ACTIVE;
    return true;
}

void link_stm32_uds_server_reset(LinkStm32UdsServer *transport)
{
    if (transport == NULL) return;
    link_isotp_rx_reset(&transport->receiver);
    link_isotp_tx_reset(&transport->transmitter);
    transport->pending_tx_valid = false;
    transport->pending_tx_tracks_transmitter = false;
    transport->in_flight_tracks_transmitter = false;
    transport->response_active = false;
    transport->deferred_rx_head = 0U;
    transport->deferred_rx_tail = 0U;
    transport->deferred_rx_dropped = 0U;
    transport->tx_completion_deadline_us = 0U;
    transport->isotp_result = LINK_ISOTP_RESULT_OK;
    transport->uds_result = LINK_UDS_SERVER_RESULT_POSITIVE;
    transport->failure = LINK_STM32_UDS_SERVER_RESULT_OK;
    transport->state = LINK_STM32_UDS_SERVER_ACTIVE;
}

LinkStm32UdsServerResult link_stm32_uds_server_poll(
    LinkStm32UdsServer *transport)
{
    LinkStm32UdsServerResult result;
    LinkIsoTpCanFrame frame;
    uint64_t arrival_us = 0U;
    uint64_t now_us;

    if (transport == NULL) return LINK_STM32_UDS_SERVER_RESULT_INVALID_ARGUMENT;
    if (transport->state == LINK_STM32_UDS_SERVER_FAILED) {
        return transport->failure == LINK_STM32_UDS_SERVER_RESULT_OK
            ? LINK_STM32_UDS_SERVER_RESULT_FAILED_STATE : transport->failure;
    }
    if (transport->state != LINK_STM32_UDS_SERVER_ACTIVE) {
        return LINK_STM32_UDS_SERVER_RESULT_FAILED_STATE;
    }

    result = link_stm32_uds_server_poll_tx_completion(transport);
    if (transport->state == LINK_STM32_UDS_SERVER_FAILED ||
        result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE) {
        return result;
    }
    result = link_stm32_uds_server_send_pending(transport);
    if (transport->state == LINK_STM32_UDS_SERVER_FAILED) return result;

    if (!transport->response_active &&
        link_stm32_uds_server_pop_deferred_rx(
            transport, &frame, &arrival_us)) {
        result = link_stm32_uds_server_accept_rx(
            transport, &frame, arrival_us);
        if (transport->state == LINK_STM32_UDS_SERVER_FAILED ||
            result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE) {
            return result;
        }
    }

    while (link_stm32_can_pop_timed(
               transport->channel, &frame, &arrival_us)) {
        result = link_stm32_uds_server_accept_rx(
            transport, &frame, arrival_us);
        if (transport->state == LINK_STM32_UDS_SERVER_FAILED ||
            result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE) {
            return result;
        }
    }

    now_us = link_stm32_can_now_us(transport->channel);
    if (transport->response_active &&
        !transport->pending_tx_valid &&
        !link_stm32_can_tx_in_flight(transport->channel) &&
        transport->transmitter.state == LINK_ISOTP_TX_SENDING &&
        link_stm32_can_tx_ready(transport->channel)) {
        transport->isotp_result = link_isotp_tx_next(
            &transport->transmitter, now_us, &transport->pending_tx);
        if (transport->isotp_result == LINK_ISOTP_RESULT_OK ||
            transport->isotp_result == LINK_ISOTP_RESULT_COMPLETE) {
            transport->pending_tx_valid = true;
            transport->pending_tx_tracks_transmitter = true;
            result = link_stm32_uds_server_send_pending(transport);
            if (transport->state == LINK_STM32_UDS_SERVER_FAILED) return result;
        } else if (transport->isotp_result != LINK_ISOTP_RESULT_WAIT_SEPARATION &&
                   transport->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
            return link_stm32_uds_server_fail(
                transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
        }
    }

    if (transport->response_active &&
        !(transport->pending_tx_valid &&
          transport->pending_tx_tracks_transmitter) &&
        !transport->in_flight_tracks_transmitter) {
        transport->isotp_result = link_isotp_tx_tick(
            &transport->transmitter, now_us);
        if (transport->isotp_result != LINK_ISOTP_RESULT_OK &&
            transport->isotp_result != LINK_ISOTP_RESULT_COMPLETE &&
            transport->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL &&
            transport->isotp_result != LINK_ISOTP_RESULT_WAIT_SEPARATION) {
            return link_stm32_uds_server_fail(
                transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
        }
    } else if (!transport->response_active) {
        transport->isotp_result = link_isotp_rx_tick(
            &transport->receiver, now_us);
        if (transport->isotp_result != LINK_ISOTP_RESULT_OK &&
            transport->isotp_result != LINK_ISOTP_RESULT_COMPLETE) {
            return link_stm32_uds_server_fail(
                transport, LINK_STM32_UDS_SERVER_RESULT_ISOTP_ERROR);
        }
    }

    return LINK_STM32_UDS_SERVER_RESULT_WAITING;
}

LinkStm32UdsServerState link_stm32_uds_server_state(
    const LinkStm32UdsServer *transport)
{
    return transport == NULL ? LINK_STM32_UDS_SERVER_FAILED : transport->state;
}

uint32_t link_stm32_uds_server_completed_requests(
    const LinkStm32UdsServer *transport)
{
    return transport == NULL ? 0U : transport->completed_request_count;
}

uint32_t link_stm32_uds_server_deferred_rx_dropped(
    const LinkStm32UdsServer *transport)
{
    return transport == NULL ? 0U : transport->deferred_rx_dropped;
}
