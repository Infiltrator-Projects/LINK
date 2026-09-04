// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-uds.c @brief STM32 orchestration for LINK ISO-TP + UDS. */
#include "link-stm32-uds.h"

#include "infiltratr/core.h"

#include <string.h>

static bool link_stm32_uds_config_valid(const LinkStm32UdsConfig *config)
{
    return config != NULL && config->consecutive_timeout_us != 0U &&
           config->flow_control_timeout_us != 0U &&
           config->p2_timeout_us != 0U && config->p2_star_timeout_us != 0U;
}

static uint64_t link_stm32_uds_deadline(uint64_t now_us, uint64_t delta_us)
{
    return infiltratr_u64_add_saturating(now_us, delta_us);
}

static LinkStm32UdsResult link_stm32_uds_fail(
    LinkStm32UdsClient *client,
    LinkStm32UdsResult result)
{
    if (client != NULL) {
        client->state = LINK_STM32_UDS_FAILED;
        client->pending_tx_valid = false;
        client->pending_tx_tracks_transmitter = false;
        client->in_flight_tracks_transmitter = false;
        client->tx_completion_deadline_us = 0U;
        client->failure = result;
    }
    return result;
}

static bool link_stm32_uds_is_flow_control(
    const LinkStm32UdsClient *client,
    const LinkIsoTpCanFrame *frame)
{
    size_t offset;

    if (client == NULL || frame == NULL ||
        client->transmitter.state != LINK_ISOTP_TX_WAIT_FLOW_CONTROL ||
        frame->can_id != client->config.address.rx_can_id ||
        frame->extended_id != client->config.address.rx_extended_id) {
        return false;
    }

    offset = client->config.address.addressing_mode == LINK_ISOTP_ADDRESSING_NORMAL
        ? 0U : 1U;
    if (frame->length <= offset) {
        return false;
    }
    if (offset != 0U &&
        frame->data[0] != client->config.address.rx_address_extension) {
        return false;
    }
    return (frame->data[offset] >> 4U) == 0x3U;
}

static LinkIsoTpResult link_stm32_uds_confirm_transmitter_sent(
    LinkStm32UdsClient *client,
    uint64_t sent_us)
{
    LinkIsoTpTx *tx = &client->transmitter;

    if (tx->state == LINK_ISOTP_TX_FAILED) {
        return tx->failure;
    }
    if (tx->state == LINK_ISOTP_TX_WAIT_FLOW_CONTROL) {
        tx->deadline_us = link_stm32_uds_deadline(
            sent_us, tx->config.flow_control_timeout_us);
        return LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL;
    }
    if (tx->state == LINK_ISOTP_TX_SENDING) {
        tx->next_send_us = link_stm32_uds_deadline(
            sent_us, tx->separation_time_us);
        return tx->separation_time_us == 0U
            ? LINK_ISOTP_RESULT_OK
            : LINK_ISOTP_RESULT_WAIT_SEPARATION;
    }
    if (tx->state == LINK_ISOTP_TX_COMPLETE) {
        return LINK_ISOTP_RESULT_COMPLETE;
    }
    return LINK_ISOTP_RESULT_UNEXPECTED_FRAME;
}

static LinkStm32UdsResult link_stm32_uds_send_pending(
    LinkStm32UdsClient *client)
{
    bool tracks_transmitter;
    uint64_t now_us;

    if (!client->pending_tx_valid) {
        return LINK_STM32_UDS_RESULT_OK;
    }
    if (!link_stm32_can_tx_ready(client->channel)) {
        return LINK_STM32_UDS_RESULT_WAITING;
    }

    tracks_transmitter = client->pending_tx_tracks_transmitter;
    if (!link_stm32_can_send(client->channel, &client->pending_tx)) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_CAN_IO);
    }

    client->pending_tx_valid = false;
    client->pending_tx_tracks_transmitter = false;
    client->in_flight_tracks_transmitter = tracks_transmitter;
    now_us = link_stm32_can_now_us(client->channel);
    client->tx_completion_deadline_us = link_stm32_uds_deadline(
        now_us, client->config.flow_control_timeout_us);
    return LINK_STM32_UDS_RESULT_OK;
}

static LinkStm32UdsResult link_stm32_uds_poll_tx_completion(
    LinkStm32UdsClient *client)
{
    LinkStm32CanTxStatus status;
    uint64_t completion_us = 0U;
    uint64_t now_us;
    bool tracks_transmitter;

    if (!link_stm32_can_tx_in_flight(client->channel)) {
        client->in_flight_tracks_transmitter = false;
        client->tx_completion_deadline_us = 0U;
        return LINK_STM32_UDS_RESULT_OK;
    }

    status = link_stm32_can_poll_tx_status(
        client->channel, &completion_us);
    if (status == LINK_STM32_CAN_TX_PENDING) {
        now_us = link_stm32_can_now_us(client->channel);
        if (client->tx_completion_deadline_us != 0U &&
            now_us >= client->tx_completion_deadline_us) {
            return link_stm32_uds_fail(
                client, LINK_STM32_UDS_RESULT_CAN_IO);
        }
        return LINK_STM32_UDS_RESULT_OK;
    }
    if (status == LINK_STM32_CAN_TX_FAILED) {
        return link_stm32_uds_fail(
            client, LINK_STM32_UDS_RESULT_CAN_IO);
    }
    if (status != LINK_STM32_CAN_TX_COMPLETE) {
        return LINK_STM32_UDS_RESULT_OK;
    }

    tracks_transmitter = client->in_flight_tracks_transmitter;
    client->in_flight_tracks_transmitter = false;
    client->tx_completion_deadline_us = 0U;
    if (!tracks_transmitter) {
        return LINK_STM32_UDS_RESULT_OK;
    }

    client->last_transmitter_completion_us = completion_us;
    client->last_transmitter_completion_valid = true;
    client->isotp_result = link_stm32_uds_confirm_transmitter_sent(
        client, completion_us);
    if (client->isotp_result == LINK_ISOTP_RESULT_OK ||
        client->isotp_result == LINK_ISOTP_RESULT_COMPLETE ||
        client->isotp_result == LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL ||
        client->isotp_result == LINK_ISOTP_RESULT_WAIT_SEPARATION) {
        return LINK_STM32_UDS_RESULT_OK;
    }
    return link_stm32_uds_fail(
        client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
}

static LinkStm32UdsResult link_stm32_uds_begin_response_wait(
    LinkStm32UdsClient *client,
    uint64_t now_us)
{
    uint64_t response_start_us;

    if (client->uds_started || client->pending_tx_valid ||
        link_stm32_can_tx_in_flight(client->channel) ||
        client->transmitter.state != LINK_ISOTP_TX_COMPLETE) {
        return LINK_STM32_UDS_RESULT_OK;
    }

    response_start_us = client->last_transmitter_completion_valid
        ? client->last_transmitter_completion_us : now_us;
    client->uds_result = link_uds_client_begin(
        &client->uds, client->tx_storage, client->request_length,
        response_start_us);
    if (client->uds_result != LINK_UDS_RESULT_OK) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_UDS_ERROR);
    }
    client->last_transmitter_completion_valid = false;
    client->uds_started = true;
    return LINK_STM32_UDS_RESULT_OK;
}

bool link_stm32_uds_init(
    LinkStm32UdsClient *client,
    LinkStm32Can *channel,
    const LinkStm32UdsConfig *config,
    uint8_t *rx_storage,
    size_t rx_capacity,
    uint8_t *tx_storage,
    size_t tx_capacity)
{
    LinkIsoTpRxConfig rx_config;
    LinkUdsClientConfig uds_config;

    if (client == NULL || channel == NULL || !link_stm32_uds_config_valid(config) ||
        rx_storage == NULL || rx_capacity == 0U ||
        tx_storage == NULL || tx_capacity == 0U) {
        return false;
    }

    memset(client, 0, sizeof(*client));
    client->channel = channel;
    client->config = *config;
    client->rx_storage = rx_storage;
    client->rx_capacity = rx_capacity;
    client->tx_storage = tx_storage;
    client->tx_capacity = tx_capacity;

    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = config->address;
    rx_config.block_size = config->rx_block_size;
    rx_config.stmin = config->rx_stmin;
    rx_config.consecutive_timeout_us = config->consecutive_timeout_us;
    rx_config.can_fd = config->can_fd;
    rx_config.data_length = config->data_length;
    client->isotp_result = link_isotp_rx_init(
        &client->receiver, &rx_config, rx_storage, rx_capacity);
    if (client->isotp_result != LINK_ISOTP_RESULT_OK) {
        return false;
    }

    uds_config.p2_timeout_us = config->p2_timeout_us;
    uds_config.p2_star_timeout_us = config->p2_star_timeout_us;
    client->uds_result = link_uds_client_init(&client->uds, &uds_config);
    if (client->uds_result != LINK_UDS_RESULT_OK) {
        return false;
    }

    client->state = LINK_STM32_UDS_IDLE;
    return true;
}

void link_stm32_uds_reset(LinkStm32UdsClient *client)
{
    if (client == NULL) {
        return;
    }
    link_isotp_rx_reset(&client->receiver);
    link_isotp_tx_reset(&client->transmitter);
    link_uds_client_reset(&client->uds);
    client->request_length = 0U;
    client->pending_tx_valid = false;
    client->pending_tx_tracks_transmitter = false;
    client->in_flight_tracks_transmitter = false;
    client->tx_completion_deadline_us = 0U;
    client->last_transmitter_completion_us = 0U;
    client->last_transmitter_completion_valid = false;
    client->uds_started = false;
    memset(&client->response, 0, sizeof(client->response));
    client->isotp_result = LINK_ISOTP_RESULT_OK;
    client->uds_result = LINK_UDS_RESULT_OK;
    client->failure = LINK_STM32_UDS_RESULT_OK;
    client->state = LINK_STM32_UDS_IDLE;
}

LinkStm32UdsResult link_stm32_uds_start(
    LinkStm32UdsClient *client,
    const uint8_t *request,
    size_t request_length)
{
    LinkIsoTpTxConfig tx_config;
    uint64_t now_us;

    if (client == NULL || request == NULL || request_length == 0U ||
        request_length > client->tx_capacity) {
        return LINK_STM32_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (client->state == LINK_STM32_UDS_ACTIVE) {
        return LINK_STM32_UDS_RESULT_BUSY;
    }

    link_stm32_uds_reset(client);
    memcpy(client->tx_storage, request, request_length);
    client->request_length = request_length;

    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = client->config.address;
    tx_config.flow_control_timeout_us = client->config.flow_control_timeout_us;
    tx_config.max_wait_frames = client->config.max_wait_frames;
    tx_config.can_fd = client->config.can_fd;
    tx_config.data_length = client->config.data_length;
    client->isotp_result = link_isotp_tx_init(
        &client->transmitter, &tx_config,
        client->tx_storage, client->request_length);
    if (client->isotp_result != LINK_ISOTP_RESULT_OK) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }

    now_us = link_stm32_can_now_us(client->channel);
    client->isotp_result = link_isotp_tx_start(
        &client->transmitter, now_us, &client->pending_tx);
    if (client->isotp_result != LINK_ISOTP_RESULT_COMPLETE &&
        client->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }

    client->pending_tx_valid = true;
    client->pending_tx_tracks_transmitter = true;
    client->state = LINK_STM32_UDS_ACTIVE;
    return LINK_STM32_UDS_RESULT_OK;
}

static LinkStm32UdsResult link_stm32_uds_accept_rx(
    LinkStm32UdsClient *client,
    const LinkIsoTpCanFrame *frame,
    uint64_t now_us)
{
    LinkIsoTpCanFrame flow_control;
    bool flow_control_ready = false;
    const uint8_t *payload;
    size_t payload_length = 0U;

    if (link_stm32_uds_is_flow_control(client, frame)) {
        client->isotp_result = link_isotp_tx_accept_flow_control(
            &client->transmitter, frame, now_us);
        if (client->isotp_result == LINK_ISOTP_RESULT_OK ||
            client->isotp_result == LINK_ISOTP_RESULT_FLOW_CONTROL_WAIT) {
            return LINK_STM32_UDS_RESULT_OK;
        }
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }

    if (!client->uds_started) {
        return LINK_STM32_UDS_RESULT_OK;
    }

    client->isotp_result = link_isotp_rx_feed(
        &client->receiver, frame, now_us, &flow_control, &flow_control_ready);
    if (flow_control_ready) {
        if (client->pending_tx_valid) {
            return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_CAN_IO);
        }
        client->pending_tx = flow_control;
        client->pending_tx_valid = true;
        client->pending_tx_tracks_transmitter = false;
    }

    if (client->isotp_result == LINK_ISOTP_RESULT_UNEXPECTED_FRAME) {
        return LINK_STM32_UDS_RESULT_OK;
    }
    if (client->isotp_result != LINK_ISOTP_RESULT_OK &&
        client->isotp_result != LINK_ISOTP_RESULT_COMPLETE) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }
    if (client->isotp_result != LINK_ISOTP_RESULT_COMPLETE) {
        return LINK_STM32_UDS_RESULT_OK;
    }

    payload = link_isotp_rx_payload(&client->receiver, &payload_length);
    if (payload == NULL || payload_length == 0U) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }

    client->uds_result = link_uds_client_accept(
        &client->uds, payload, payload_length, now_us, &client->response);
    if (client->uds_result == LINK_UDS_RESULT_RESPONSE_PENDING) {
        link_isotp_rx_reset(&client->receiver);
        return LINK_STM32_UDS_RESULT_WAITING;
    }
    if (client->uds_result == LINK_UDS_RESULT_NEGATIVE_RESPONSE) {
        client->state = LINK_STM32_UDS_COMPLETE;
        return LINK_STM32_UDS_RESULT_NEGATIVE_RESPONSE;
    }
    if (client->uds_result != LINK_UDS_RESULT_COMPLETE) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_UDS_ERROR);
    }

    client->state = LINK_STM32_UDS_COMPLETE;
    return LINK_STM32_UDS_RESULT_COMPLETE;
}

LinkStm32UdsResult link_stm32_uds_poll(LinkStm32UdsClient *client)
{
    LinkStm32UdsResult result;
    LinkIsoTpCanFrame frame;
    uint64_t now_us;
    uint64_t arrival_us = 0U;

    if (client == NULL) {
        return LINK_STM32_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (client->state == LINK_STM32_UDS_IDLE) {
        return LINK_STM32_UDS_RESULT_OK;
    }
    if (client->state == LINK_STM32_UDS_COMPLETE) {
        return client->response.kind == LINK_UDS_RESPONSE_NEGATIVE
            ? LINK_STM32_UDS_RESULT_NEGATIVE_RESPONSE
            : LINK_STM32_UDS_RESULT_COMPLETE;
    }
    if (client->state == LINK_STM32_UDS_FAILED) {
        return client->failure;
    }

    result = link_stm32_uds_poll_tx_completion(client);
    if (client->state == LINK_STM32_UDS_FAILED) {
        return result;
    }

    now_us = link_stm32_can_now_us(client->channel);
    result = link_stm32_uds_send_pending(client);
    if (result == LINK_STM32_UDS_RESULT_CAN_IO) {
        return result;
    }
    result = link_stm32_uds_begin_response_wait(client, now_us);
    if (result != LINK_STM32_UDS_RESULT_OK) {
        return result;
    }

    while (link_stm32_can_pop_timed(
               client->channel, &frame, &arrival_us)) {
        result = link_stm32_uds_accept_rx(
            client, &frame, arrival_us);
        if (result == LINK_STM32_UDS_RESULT_COMPLETE ||
            result == LINK_STM32_UDS_RESULT_NEGATIVE_RESPONSE ||
            client->state == LINK_STM32_UDS_FAILED) {
            return result;
        }
        result = link_stm32_uds_send_pending(client);
        if (result == LINK_STM32_UDS_RESULT_CAN_IO) {
            return result;
        }
    }

    now_us = link_stm32_can_now_us(client->channel);
    if (!client->pending_tx_valid &&
        !link_stm32_can_tx_in_flight(client->channel) &&
        client->transmitter.state == LINK_ISOTP_TX_SENDING &&
        link_stm32_can_tx_ready(client->channel)) {
        client->isotp_result = link_isotp_tx_next(
            &client->transmitter, now_us, &client->pending_tx);
        if (client->isotp_result == LINK_ISOTP_RESULT_OK ||
            client->isotp_result == LINK_ISOTP_RESULT_COMPLETE) {
            client->pending_tx_valid = true;
            client->pending_tx_tracks_transmitter = true;
            result = link_stm32_uds_send_pending(client);
            if (result == LINK_STM32_UDS_RESULT_CAN_IO) {
                return result;
            }
        } else if (client->isotp_result != LINK_ISOTP_RESULT_WAIT_SEPARATION &&
                   client->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
            return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
        }
    }

    result = link_stm32_uds_begin_response_wait(client, now_us);
    if (result != LINK_STM32_UDS_RESULT_OK) {
        return result;
    }

    if (!(client->pending_tx_valid &&
          client->pending_tx_tracks_transmitter) &&
        !client->in_flight_tracks_transmitter) {
        client->isotp_result = link_isotp_tx_tick(
            &client->transmitter, now_us);
        if (client->isotp_result != LINK_ISOTP_RESULT_OK &&
            client->isotp_result != LINK_ISOTP_RESULT_COMPLETE &&
            client->isotp_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL &&
            client->isotp_result != LINK_ISOTP_RESULT_WAIT_SEPARATION) {
            return link_stm32_uds_fail(
                client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
        }
    }

    client->isotp_result = link_isotp_rx_tick(&client->receiver, now_us);
    if (client->isotp_result != LINK_ISOTP_RESULT_OK &&
        client->isotp_result != LINK_ISOTP_RESULT_COMPLETE) {
        return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_ISOTP_ERROR);
    }

    if (client->uds_started) {
        client->uds_result = link_uds_client_tick(&client->uds, now_us);
        if (client->uds_result != LINK_UDS_RESULT_WAITING &&
            client->uds_result != LINK_UDS_RESULT_RESPONSE_PENDING &&
            client->uds_result != LINK_UDS_RESULT_COMPLETE) {
            return link_stm32_uds_fail(client, LINK_STM32_UDS_RESULT_UDS_ERROR);
        }
    }

    return LINK_STM32_UDS_RESULT_WAITING;
}

const LinkUdsResponse *link_stm32_uds_response(const LinkStm32UdsClient *client)
{
    return client != NULL && client->state == LINK_STM32_UDS_COMPLETE
        ? &client->response : NULL;
}
