// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32F1XX_HAL_FLASH_EX_H
#define LINK_TEST_STM32F1XX_HAL_FLASH_EX_H

#include "main.h"
#include <stdint.h>

#define FLASH_TYPEERASE_PAGES UINT32_C(1)

typedef struct {
    uint32_t TypeErase;
    uint32_t Banks;
    uint32_t PageAddress;
    uint32_t NbPages;
} FLASH_EraseInitTypeDef;

HAL_StatusTypeDef HAL_FLASHEx_Erase(
    FLASH_EraseInitTypeDef *erase_init, uint32_t *page_error);

#endif
