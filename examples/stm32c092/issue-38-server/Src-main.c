// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Replacement application integration for the STM32C092 KEIL/Cube project
 * attached to MBLINK issue #38. Keep the project's generated clock/GPIO/
 * FDCAN/UART initialization functions and use this LINK server-role wiring.
 */
#include "main.h"
#include "fdcan.h"
#include "gpio.h"
#include "usart.h"

#include "link-stm32c092-server-example.h"

extern FDCAN_HandleTypeDef hfdcan1;

int main(void)
{
    LinkStm32C092ServerConfig server = LINK_STM32C092_SERVER_CONFIG_INIT;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_FDCAN1_Init();
    MX_USART1_UART_Init();

    server.request_can_id = 0x7e0U;   /* Tester -> STM32 ECU/server. */
    server.response_can_id = 0x7e8U;  /* STM32 ECU/server -> tester. */
    server.can_fd = false;
    server.data_length = 8U;

    if (!link_stm32c092_server_example_init(&hfdcan1, &server)) {
        Error_Handler();
    }

    for (;;) {
        link_stm32c092_server_example_process();
    }
}

void HAL_FDCAN_RxFifo0Callback(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U) {
        link_stm32c092_server_example_rx_fifo0_irq(hfdcan);
    }
}

void HAL_FDCAN_TxEventFifoCallback(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t TxEventFifoITs)
{
    link_stm32c092_server_example_tx_event_irq(hfdcan, TxEventFifoITs);
}
