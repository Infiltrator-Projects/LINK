/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32C092 diagnostic tester port for MBLINK issue #27
  ******************************************************************************
  *
  * Cube-generated peripheral setup is retained from the submitted project.
  * UDS/ISO-TP application logic is provided by LINK.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "fdcan.h"
#include "usart.h"
#include "gpio.h"

/* USER CODE BEGIN Includes */
#include "link-stm32c092-example.h"
/* USER CODE END Includes */

void SystemClock_Config(void);

int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_USART2_UART_Init();

  /* USER CODE BEGIN 2 */
  /*
   * LINK owns the FDCAN filter, notifications and HAL_FDCAN_Start() call for
   * the diagnostic tester path.  Do not also initialise the old project-local
   * UDS server transport here.
   */
  if (!link_stm32c092_example_init(&hfdcan1))
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  while (1)
  {
    /* USER CODE BEGIN WHILE */
    link_stm32c092_example_process();
    /* USER CODE END WHILE */
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                               RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan,
                               uint32_t RxFifo0ITs)
{
  if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U)
  {
    /* LINK drains the FIFO and timestamps frames from the ISR edge. */
    link_stm32c092_example_rx_fifo0_irq(hfdcan);
  }
}

void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan,
                                   uint32_t TxEventFifoITs)
{
  /*
   * LINK uses the TX-event FIFO rather than assuming that queue insertion
   * means successful transmission.  This is required for correct ISO-TP
   * separation/flow-control timing and failed-TX detection.
   */
  link_stm32c092_example_tx_event_irq(hfdcan, TxEventFifoITs);
}
/* USER CODE END 4 */

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  (void)file;
  (void)line;
}
#endif /* USE_FULL_ASSERT */
