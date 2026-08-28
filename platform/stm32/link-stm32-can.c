// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-can.c @brief STM32 CAN/FDCAN edge for LINK. */
#include "link-stm32-can.h"

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

bool link_stm32_can_init(LinkStm32Can *channel, const LinkStm32CanOps *ops)
{
    if (channel == NULL || ops == NULL || ops->receive == NULL ||
        ops->tx_ready == NULL || ops->send == NULL || ops->clock_ms == NULL) {
        return false;
    }

    memset(channel, 0, sizeof(*channel));
    channel->ops = *ops;
    return true;
}

void link_stm32_can_rx_isr(LinkStm32Can *channel)
{
    LinkIsoTpCanFrame frame;

    if (channel == NULL || channel->ops.receive == NULL) {
        return;
    }

    while (channel->ops.receive(channel->ops.context, &frame)) {
        uint8_t head;

        if (!link_stm32_can_frame_valid(&frame)) {
            continue;
        }

        head = channel->rx_head;
        if ((uint8_t)(head - channel->rx_tail) >=
            LINK_STM32_CAN_RX_QUEUE_CAPACITY) {
            channel->rx_dropped++;
            continue;
        }

        channel->rx_queue[head % LINK_STM32_CAN_RX_QUEUE_CAPACITY] = frame;
        channel->rx_head = (uint8_t)(head + 1U);
    }
}

bool link_stm32_can_pop(LinkStm32Can *channel, LinkIsoTpCanFrame *frame)
{
    uint8_t tail;

    if (channel == NULL || frame == NULL) {
        return false;
    }

    tail = channel->rx_tail;
    if (tail == channel->rx_head) {
        return false;
    }

    *frame = channel->rx_queue[tail % LINK_STM32_CAN_RX_QUEUE_CAPACITY];
    channel->rx_tail = (uint8_t)(tail + 1U);
    return true;
}

uint32_t link_stm32_can_rx_dropped(const LinkStm32Can *channel)
{
    return channel == NULL ? 0U : channel->rx_dropped;
}

bool link_stm32_can_tx_ready(const LinkStm32Can *channel)
{
    return channel != NULL && channel->ops.tx_ready != NULL &&
           channel->ops.tx_ready(channel->ops.context);
}

bool link_stm32_can_send(LinkStm32Can *channel, const LinkIsoTpCanFrame *frame)
{
    if (channel == NULL || channel->ops.send == NULL ||
        !link_stm32_can_frame_valid(frame)) {
        return false;
    }
    return channel->ops.send(channel->ops.context, frame);
}

uint64_t link_stm32_can_now_us(LinkStm32Can *channel)
{
    uint32_t current;

    if (channel == NULL || channel->ops.clock_ms == NULL) {
        return 0U;
    }

    current = channel->ops.clock_ms(channel->ops.context);
    if (!channel->clock_started) {
        channel->last_tick_ms = current;
        channel->clock_started = true;
        return 0U;
    }

    channel->elapsed_ms += (uint32_t)(current - channel->last_tick_ms);
    channel->last_tick_ms = current;

    if (channel->elapsed_ms > UINT64_MAX / UINT64_C(1000)) {
        return UINT64_MAX;
    }
    return channel->elapsed_ms * UINT64_C(1000);
}
