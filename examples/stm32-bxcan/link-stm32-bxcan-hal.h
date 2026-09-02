// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32-bxcan-hal.h
 * @brief STM32Cube bxCAN binding for the portable LINK STM32 edge.
 *
 * Include the Cube-generated can.h before using this adapter. The same HAL CAN
 * API is provided by STM32CubeF1 and STM32CubeF7, covering STM32F103,
 * STM32F107 and STM32F767 without copying the transport implementation.
 */
#ifndef LINK_STM32_BXCAN_HAL_H
#define LINK_STM32_BXCAN_HAL_H

#include "can.h"
#include "link-stm32-can.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    CAN_HandleTypeDef *hcan;
    uint32_t filter_bank;
    uint32_t slave_start_filter_bank;
    volatile uint32_t pending_mailbox;
    volatile uint32_t completed_mailbox;
    volatile uint32_t failed_mailbox;
    volatile uint32_t completed_tick_ms;
} LinkStm32BxCanHal;

void link_stm32_bxcan_hal_init(
    LinkStm32BxCanHal *adapter,
    CAN_HandleTypeDef *hcan,
    uint32_t filter_bank,
    uint32_t slave_start_filter_bank);
LinkStm32CanOps link_stm32_bxcan_hal_ops(LinkStm32BxCanHal *adapter);

bool link_stm32_bxcan_hal_start_standard(
    LinkStm32BxCanHal *adapter,
    uint32_t receive_id);
bool link_stm32_bxcan_hal_start_standard_dual(
    LinkStm32BxCanHal *adapter,
    uint32_t receive_id,
    uint32_t functional_receive_id);

void link_stm32_bxcan_hal_tx_complete_irq(
    LinkStm32BxCanHal *adapter,
    uint32_t mailbox);
void link_stm32_bxcan_hal_tx_abort_irq(
    LinkStm32BxCanHal *adapter,
    uint32_t mailbox);
void link_stm32_bxcan_hal_error_irq(LinkStm32BxCanHal *adapter);

#ifdef __cplusplus
}
#endif

#endif
