// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-can.c @brief STM32 CAN/FDCAN edge for LINK. */
#include "link-stm32-can.h"

#include "infiltratr/core.h"

#include <string.h>

static bool link_stm32_can_frame_valid(const LinkIsoTpCanFrame *frame)
{
    if (frame == NULL || frame->length == 0U ||
        frame->length > LINK_ISOTP_CAN_FD_MAX_DATA_LENGTH) {
        return false;
    }
    if (!frame->can_fd && frame->length > LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH) {
        return false;
    }
    return frame->extended_id ? frame->can_id <= 0x1fffffffU
                              : frame->can_id <= 0x7ffU;
}

static uint64_t link_stm32_can_extend_tick(
    LinkStm32Can *channel,
    uint32_t current)
{
    const uint32_t delta = (uint32_t)(current - channel->last_tick_ms);

    channel->elapsed_ms = infiltratr_u64_add_saturating(
        channel->elapsed_ms, (uint64_t)delta);
    channel->last_tick_ms = current;

    return infiltratr_u64_multiply_saturating(
        channel->elapsed_ms, UINT64_C(1000));
}

static uint64_t link_stm32_can_project_tick_us(
    LinkStm32Can *channel,
    uint32_t event_tick_ms)
{
    uint32_t current;
    uint32_t age_ms;
    uint64_t now_us;
    uint64_t age_us;

    current = channel->ops.clock_ms(channel->ops.context);
    now_us = link_stm32_can_extend_tick(channel, current);
    age_ms = (uint32_t)(current - event_tick_ms);
    age_us = (uint64_t)age_ms * UINT64_C(1000);
    return age_us > now_us ? 0U : now_us - age_us;
}

bool link_stm32_can_init(LinkStm32Can *channel, const LinkStm32CanOps *ops)
{
    if (channel == NULL || ops == NULL || ops->receive == NULL ||
        ops->tx_ready == NULL || ops->send == NULL ||
        ops->tx_status == NULL || ops->clock_ms == NULL) {
        return false;
    }

    memset(channel, 0, sizeof(*channel));
    channel->ops = *ops;
    channel->last_tick_ms = channel->ops.clock_ms(channel->ops.context);
    return true;
}

void link_stm32_can_rx_isr(LinkStm32Can *channel)
{
    LinkIsoTpCanFrame frame;

    if (channel == NULL || channel->ops.receive == NULL ||
        channel->ops.clock_ms == NULL) {
        return;
    }

    /*
     * Some Cube integrations retain both the FDCAN RX callback and a
     * main-loop fallback for boards where delivery of that callback has been
     * unreliable. On a single-core STM32 the fallback can be interrupted
     * while it is inside the HAL receive callback. Coalesce that nested entry
     * into the active drain so there remains exactly one RX producer and FIFO
     * order cannot be reversed. A following IRQ or fallback pass collects a
     * frame that arrives immediately after the active drain's final poll.
     */
    if (channel->rx_draining) {
        return;
    }
    channel->rx_draining = true;

    while (channel->ops.receive(channel->ops.context, &frame)) {
        uint8_t head;
        uint32_t arrival_tick_ms;

        arrival_tick_ms = channel->ops.clock_ms(channel->ops.context);
        if (!link_stm32_can_frame_valid(&frame)) {
            continue;
        }

        head = channel->rx_head;
        if ((uint8_t)(head - channel->rx_tail) >=
            LINK_STM32_CAN_RX_QUEUE_CAPACITY) {
            channel->rx_dropped++;
            continue;
        }

        channel->rx_queue[head % LINK_STM32_CAN_RX_QUEUE_CAPACITY].frame = frame;
        channel->rx_queue[head % LINK_STM32_CAN_RX_QUEUE_CAPACITY]
            .arrival_tick_ms = arrival_tick_ms;
        channel->rx_head = (uint8_t)(head + 1U);
    }

    channel->rx_draining = false;
}

bool link_stm32_can_pop_timed(
    LinkStm32Can *channel,
    LinkIsoTpCanFrame *frame,
    uint64_t *arrival_us)
{
    LinkStm32CanRxEntry entry;
    uint8_t tail;

    if (arrival_us != NULL) {
        *arrival_us = 0U;
    }
    if (channel == NULL || frame == NULL) {
        return false;
    }

    tail = channel->rx_tail;
    if (tail == channel->rx_head) {
        return false;
    }

    entry = channel->rx_queue[tail % LINK_STM32_CAN_RX_QUEUE_CAPACITY];
    channel->rx_tail = (uint8_t)(tail + 1U);
    *frame = entry.frame;
    if (arrival_us != NULL) {
        *arrival_us = link_stm32_can_project_tick_us(
            channel, entry.arrival_tick_ms);
    }
    return true;
}

bool link_stm32_can_pop(LinkStm32Can *channel, LinkIsoTpCanFrame *frame)
{
    return link_stm32_can_pop_timed(channel, frame, NULL);
}

uint32_t link_stm32_can_rx_dropped(const LinkStm32Can *channel)
{
    return channel == NULL ? 0U : channel->rx_dropped;
}

bool link_stm32_can_tx_ready(const LinkStm32Can *channel)
{
    return channel != NULL && !channel->tx_in_flight &&
           channel->ops.tx_ready != NULL &&
           channel->ops.tx_ready(channel->ops.context);
}

bool link_stm32_can_send(LinkStm32Can *channel, const LinkIsoTpCanFrame *frame)
{
    if (channel == NULL || channel->ops.send == NULL ||
        !link_stm32_can_frame_valid(frame) ||
        !link_stm32_can_tx_ready(channel)) {
        return false;
    }
    if (!channel->ops.send(channel->ops.context, frame)) {
        return false;
    }
    channel->tx_in_flight = true;
    return true;
}

bool link_stm32_can_tx_in_flight(const LinkStm32Can *channel)
{
    return channel != NULL && channel->tx_in_flight;
}

LinkStm32CanTxStatus link_stm32_can_poll_tx_status(
    LinkStm32Can *channel,
    uint64_t *completion_us)
{
    LinkStm32CanTxStatus status;
    uint32_t completion_tick_ms = 0U;

    if (completion_us != NULL) {
        *completion_us = 0U;
    }
    if (channel == NULL || channel->ops.tx_status == NULL) {
        return LINK_STM32_CAN_TX_FAILED;
    }
    if (!channel->tx_in_flight) {
        return LINK_STM32_CAN_TX_IDLE;
    }

    status = channel->ops.tx_status(
        channel->ops.context, &completion_tick_ms);
    if (status == LINK_STM32_CAN_TX_COMPLETE) {
        if (completion_us != NULL) {
            *completion_us = link_stm32_can_project_tick_us(
                channel, completion_tick_ms);
        }
        channel->tx_in_flight = false;
    } else if (status == LINK_STM32_CAN_TX_FAILED ||
               status == LINK_STM32_CAN_TX_IDLE) {
        channel->tx_in_flight = false;
        status = LINK_STM32_CAN_TX_FAILED;
    }
    return status;
}

uint64_t link_stm32_can_now_us(LinkStm32Can *channel)
{
    uint32_t current;

    if (channel == NULL || channel->ops.clock_ms == NULL) {
        return 0U;
    }

    current = channel->ops.clock_ms(channel->ops.context);
    return link_stm32_can_extend_tick(channel, current);
}
