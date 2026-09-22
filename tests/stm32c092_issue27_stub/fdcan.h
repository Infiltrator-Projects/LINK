// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32C092_ISSUE27_FDCAN_H
#define LINK_TEST_STM32C092_ISSUE27_FDCAN_H
#include "../stm32c092_hal_stub/fdcan.h"
extern FDCAN_HandleTypeDef hfdcan1;
void MX_FDCAN1_Init(void);
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan, uint32_t TxEventFifoITs);
#endif
