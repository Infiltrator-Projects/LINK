// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Drop-in UDS ECU main for the STM32F103-V3 archive in the LINK #47 comments.
 * The earlier CANV2 project was only a periodic raw CAN sender; V3 contains
 * LINK but starts CAN before its flash journal is recovered. This entry point
 * replaces V3 Core/Src/main.c, and retains its Cube clock configuration.
 * Compile exactly one main.c. The V3 can.c has its old RX callback disabled;
 * do not re-enable it alongside the LINK callback below.
 * Reserve 0x0807F000..0x0807FFFF from the Keil IROM1 image first.
 */
#include "main.h"

void SystemClock_Config(void);

#define LINK_STM32F103_CAN_HANDLE hcan
#include "../issue-32/Src-main.c"

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

void Error_Handler(void)
{
    __disable_irq();
    for (;;) { }
}
