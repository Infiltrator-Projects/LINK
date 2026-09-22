// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32C092_ISSUE27_MAIN_H
#define LINK_TEST_STM32C092_ISSUE27_MAIN_H
#include <stdint.h>
typedef struct { uint32_t OscillatorType; uint32_t HSEState; } RCC_OscInitTypeDef;
typedef struct {
    uint32_t ClockType;
    uint32_t SYSCLKSource;
    uint32_t SYSCLKDivider;
    uint32_t AHBCLKDivider;
    uint32_t APB1CLKDivider;
} RCC_ClkInitTypeDef;
#define FLASH_LATENCY_1 UINT32_C(1)
#define RCC_OSCILLATORTYPE_HSE UINT32_C(1)
#define RCC_HSE_ON UINT32_C(1)
#define RCC_CLOCKTYPE_HCLK UINT32_C(1)
#define RCC_CLOCKTYPE_SYSCLK UINT32_C(2)
#define RCC_CLOCKTYPE_PCLK1 UINT32_C(4)
#define RCC_SYSCLKSOURCE_HSE UINT32_C(1)
#define RCC_SYSCLK_DIV1 UINT32_C(1)
#define RCC_HCLK_DIV1 UINT32_C(1)
#define RCC_APB1_DIV1 UINT32_C(1)
#define __HAL_FLASH_SET_LATENCY(x) ((void)(x))
void HAL_Init(void);
int HAL_RCC_OscConfig(RCC_OscInitTypeDef *config);
int HAL_RCC_ClockConfig(RCC_ClkInitTypeDef *config, uint32_t latency);
void __disable_irq(void);
void Error_Handler(void);
#endif
