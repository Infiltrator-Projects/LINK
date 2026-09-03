// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32-can.h
 * @brief Allocation-free STM32 CAN/FDCAN edge for LINK ISO-TP.
 *
 * This layer is deliberately independent of STM32Cube HAL types. A concrete
 * STM32 family adapter supplies RX, TX, completion-status and millisecond-clock
 * callbacks, so the portable LINK core gains no STM32 HAL dependency.
 *
 * RX is single-producer/single-consumer: the interrupt producer snapshots both
 * the frame and its arrival tick; the main-loop consumer receives a projected
 * monotonic timestamp. The drain entry is re-entrancy guarded so a target may
 * safely use both its normal RX interrupt and a main-loop fallback without two
 * producers entering the HAL receive callback at once. TX allows one
 * outstanding hardware frame at a time and does not report completion until
 * the concrete controller confirms it.
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

typedef enum {
    LINK_STM32_CAN_TX_IDLE = 0,
    LINK_STM32_CAN_TX_PENDING,
    LINK_STM32_CAN_TX_COMPLETE,
    LINK_STM32_CAN_TX_FAILED
} LinkStm32CanTxStatus;

typedef bool (*LinkStm32CanReceiveFn)(void *context, LinkIsoTpCanFrame *frame);
typedef bool (*LinkStm32CanTxReadyFn)(void *context);
typedef bool (*LinkStm32CanSendFn)(void *context, const LinkIsoTpCanFrame *frame);
typedef LinkStm32CanTxStatus (*LinkStm32CanTxStatusFn)(
    void *context,
    uint32_t *completion_tick_ms);
typedef uint32_t (*LinkStm32CanClockMsFn)(void *context);

typedef struct {
    void *context;
    LinkStm32CanReceiveFn receive;
    LinkStm32CanTxReadyFn tx_ready;
    LinkStm32CanSendFn send;
    LinkStm32CanTxStatusFn tx_status;
    LinkStm32CanClockMsFn clock_ms;
} LinkStm32CanOps;

typedef struct {
    LinkIsoTpCanFrame frame;
    uint32_t arrival_tick_ms;
} LinkStm32CanRxEntry;

typedef struct {
    LinkStm32CanOps ops;
    LinkStm32CanRxEntry rx_queue[LINK_STM32_CAN_RX_QUEUE_CAPACITY];
    volatile uint8_t rx_head;
    volatile uint8_t rx_tail;
    volatile uint32_t rx_dropped;
    volatile bool rx_draining;
    bool tx_in_flight;
    uint32_t last_tick_ms;
    uint64_t elapsed_ms;
} LinkStm32Can;

bool link_stm32_can_init(LinkStm32Can *channel, const LinkStm32CanOps *ops);
void link_stm32_can_rx_isr(LinkStm32Can *channel);
bool link_stm32_can_pop(LinkStm32Can *channel, LinkIsoTpCanFrame *frame);
bool link_stm32_can_pop_timed(
    LinkStm32Can *channel,
    LinkIsoTpCanFrame *frame,
    uint64_t *arrival_us);
uint32_t link_stm32_can_rx_dropped(const LinkStm32Can *channel);
bool link_stm32_can_tx_ready(const LinkStm32Can *channel);
bool link_stm32_can_send(LinkStm32Can *channel, const LinkIsoTpCanFrame *frame);
bool link_stm32_can_tx_in_flight(const LinkStm32Can *channel);
LinkStm32CanTxStatus link_stm32_can_poll_tx_status(
    LinkStm32Can *channel,
    uint64_t *completion_us);
uint64_t link_stm32_can_now_us(LinkStm32Can *channel);

#ifdef __cplusplus
}
#endif

#endif
