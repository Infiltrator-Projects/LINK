// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32c092-hal.h
 * @brief STM32C092 STM32Cube HAL binding for LINK's bare-metal CAN edge.
 */
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
} LinkStm32C092Hal;

void link_stm32c092_hal_init(
    LinkStm32C092Hal *adapter,
    FDCAN_HandleTypeDef *hfdcan,
    bool fd_bit_rate_switch);

LinkStm32CanOps link_stm32c092_hal_ops(LinkStm32C092Hal *adapter);

/**
 * Configure one exact standard-ID response filter, reject unmatched/remote
 * frames, enable FIFO0 notification and start the FDCAN controller.
 *
 * The supplied STM32C092 project already declares one standard filter and is
 * therefore directly compatible with this helper.
 */
bool link_stm32c092_hal_start_standard(
    LinkStm32C092Hal *adapter,
    uint32_t response_id);

#ifdef __cplusplus
}
#endif

#endif
