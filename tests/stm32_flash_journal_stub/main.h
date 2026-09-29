// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32_FLASH_MAIN_H
#define LINK_TEST_STM32_FLASH_MAIN_H

#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

void HAL_Init(void);
void SystemClock_Config(void);

#endif
