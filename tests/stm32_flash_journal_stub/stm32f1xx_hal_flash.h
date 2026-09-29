// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32F1XX_HAL_FLASH_H
#define LINK_TEST_STM32F1XX_HAL_FLASH_H

#include "main.h"
#include <stdint.h>

#define FLASH_TYPEPROGRAM_HALFWORD UINT32_C(1)

HAL_StatusTypeDef HAL_FLASH_Unlock(void);
HAL_StatusTypeDef HAL_FLASH_Lock(void);
HAL_StatusTypeDef HAL_FLASH_Program(
    uint32_t type_program, uint32_t address, uint64_t data);

#endif
