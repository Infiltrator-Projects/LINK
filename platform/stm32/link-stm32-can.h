// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32-can.h
 * @brief Allocation-free STM32 CAN/FDCAN edge for LINK ISO-TP.
 *
 * This layer is deliberately independent of STM32Cube HAL types. A concrete
 * STM32 family adapter supplies four tiny callbacks for RX, TX, TX readiness
 * and the millisecond HAL clock. The LINK protocol stack therefore remains
 * portable and the normal desktop/mobile builds gain no STM32 dependency.
 *
 * The receive queue is single-producer/single-consumer: the producer is the
 * CAN RX interrupt and the consumer is the main loop. Frames are copied into
 * bounded caller-owned state; overflow is counted and the newest frame is
 * dropped rather than overwriting unread evidence.
 */
#ifndef LINK_STM32_CAN_H
#define LINK_STM32_CAN_H

#include "link/isotp.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_STM32_CAN_RX_QUEUE_CAPACITY 8U

typedef bool (*LinkStm32CanReceiveFn)(void *context, LinkIsoTpCanFrame *frame);
typedef bool (*LinkStm32CanTxReadyFn)(void *context);
typedef bool (*LinkStm32CanSendFn)(void *context, const LinkIsoTpCanFrame *frame);
typedef uint32_t (*LinkStm32CanClockMsFn)(void *context);

typedef struct {
    void *context;
    LinkStm32CanReceiveFn receive;
    LinkStm32CanTxReadyFn tx_ready;
    LinkStm32CanSendFn send;
    LinkStm32CanClockMsFn clock_ms;
} LinkStm32CanOps;

typedef struct {
    LinkStm32CanOps ops;
    LinkIsoTpCanFrame rx_queue[LINK_STM32_CAN_RX_QUEUE_CAPACITY];
    volatile uint8_t rx_head;
    volatile uint8_t rx_tail;
    volatile uint32_t rx_dropped;
    uint32_t last_tick_ms;
    uint64_t elapsed_ms;
    bool clock_started;
} LinkStm32Can;

/** Validate callbacks, clear the queue and initialise the wrap-safe clock. */
bool link_stm32_can_init(LinkStm32Can *channel, const LinkStm32CanOps *ops);

/**
 * Drain the concrete FDCAN RX FIFO into the bounded LINK queue.
 * Call this from the STM32 HAL RX FIFO callback for the configured controller.
 */
void link_stm32_can_rx_isr(LinkStm32Can *channel);

/** Pop one queued frame in main-loop context. */
bool link_stm32_can_pop(LinkStm32Can *channel, LinkIsoTpCanFrame *frame);

/** Return the number of frames dropped because the bounded RX queue was full. */
uint32_t link_stm32_can_rx_dropped(const LinkStm32Can *channel);

/** Return true only when the concrete controller can accept another TX frame. */
bool link_stm32_can_tx_ready(const LinkStm32Can *channel);

/** Submit one LINK ISO-TP CAN frame to the concrete STM32 controller. */
bool link_stm32_can_send(LinkStm32Can *channel, const LinkIsoTpCanFrame *frame);

/**
 * Convert the caller's wrapping 32-bit millisecond HAL clock to monotonic
 * microseconds. Natural uint32_t subtraction extends HAL_GetTick across wrap.
 * The function is intended for main-loop use; the RX ISR never mutates time.
 */
uint64_t link_stm32_can_now_us(LinkStm32Can *channel);

#ifdef __cplusplus
}
#endif

#endif
