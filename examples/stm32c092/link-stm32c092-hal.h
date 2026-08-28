// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_STM32C092_HAL_H
#define LINK_STM32C092_HAL_H

#include "fdcan.h"
#include "link-stm32-can.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    FDCAN_HandleTypeDef *hfdcan;
    bool fd_bit_rate_switch;
    volatile uint8_t pending_marker;
    volatile uint8_t completed_marker;
    volatile uint32_t completed_tick_ms;
    volatile bool tx_event_lost;
    uint8_t next_marker;
} LinkStm32C092Hal;

void link_stm32c092_hal_init(
    LinkStm32C092Hal *adapter,
    FDCAN_HandleTypeDef *hfdcan,
    bool fd_bit_rate_switch);
LinkStm32CanOps link_stm32c092_hal_ops(LinkStm32C092Hal *adapter);
void link_stm32c092_hal_tx_event_irq(
    LinkStm32C092Hal *adapter,
    uint32_t tx_event_fifo_its);
bool link_stm32c092_hal_start_standard(
    LinkStm32C092Hal *adapter,
    uint32_t response_id);

#ifdef __cplusplus
}
#endif

#endif
