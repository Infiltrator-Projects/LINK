/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "fdcan.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "can_transport_fdcan.h"
#include "uds_app_fdcan.h"
#include "uds_platform_fdcan.h"
#include "uds_dtc.h"
#include <stddef.h>
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
static UdsC092FdcanTransport uds_transport;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define TX_ID          (UDS_C092_RESPONSE_ID)   /* TX CAN message identifier    */
#define RX_ID          (UDS_C092_REQUEST_ID)   /* RX CAN message identifier    */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
FDCAN_RxHeaderTypeDef rxHeader;
FDCAN_TxHeaderTypeDef txHeader;

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

typedef struct {
  uint32_t code;
  uint8_t status;
} DemoDtcRecord;

static const DemoDtcRecord demo_dtcs[] = {
  { 0x123456U, 0x09U },
  { 0xABCDEFU, 0x08U }
};

static const uint8_t demo_vin[17] = {
  'L','I','N','K','S','T','M','3','2','C','0','9','2','0','0','1','A'
};

static bool demo_put_dtc(uint8_t *response, uint16_t capacity, uint16_t *offset,
                         const DemoDtcRecord *record)
{
  if ((response == NULL) || (offset == NULL) || (record == NULL) ||
      ((uint16_t)(capacity - *offset) < 4U))
    return false;
  response[(*offset)++] = (uint8_t)(record->code >> 16U);
  response[(*offset)++] = (uint8_t)(record->code >> 8U);
  response[(*offset)++] = (uint8_t)record->code;
  response[(*offset)++] = record->status;
  return true;
}

static UdsCallbackResult demo_read_did(void *context, uint16_t did, uint8_t *data,
                                       uint16_t *length, uint16_t capacity)
{
  (void)context;
  if ((data == NULL) || (length == NULL))
    return UDS_RESULT_ERROR;
  if (did != 0xF190U)
    return UDS_RESULT_OUT_OF_RANGE;
  if (capacity < sizeof(demo_vin))
    return UDS_RESULT_RESPONSE_TOO_LONG;
  memcpy(data, demo_vin, sizeof(demo_vin));
  *length = (uint16_t)sizeof(demo_vin);
  return UDS_RESULT_OK;
}

static UdsCallbackResult demo_dtc_report(void *context, uint8_t subfunction,
                                         const uint8_t *request, uint16_t request_length,
                                         uint8_t *response, uint16_t *response_length,
                                         uint16_t capacity)
{
  uint16_t offset = 0U;
  uint8_t mask = 0xFFU;
  size_t index;
  (void)context;
  (void)request_length;

  if ((request == NULL) || (response == NULL) || (response_length == NULL) ||
      (capacity == 0U))
    return UDS_RESULT_ERROR;

  response[offset++] = subfunction;
  switch (subfunction) {
  case 0x01U:
  {
    uint16_t count = 0U;
    if (capacity < 5U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    mask = request[2];
    for (index = 0U; index < (sizeof(demo_dtcs) / sizeof(demo_dtcs[0])); ++index)
      if ((demo_dtcs[index].status & mask) != 0U)
        ++count;
    response[offset++] = 0xFFU;
    response[offset++] = 0x01U;
    response[offset++] = (uint8_t)(count >> 8U);
    response[offset++] = (uint8_t)count;
    break;
  }
  case 0x02U:
    mask = request[2];
    if (capacity < 2U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    response[offset++] = 0xFFU;
    for (index = 0U; index < (sizeof(demo_dtcs) / sizeof(demo_dtcs[0])); ++index) {
      if ((demo_dtcs[index].status & mask) == 0U)
        continue;
      if (!demo_put_dtc(response, capacity, &offset, &demo_dtcs[index]))
        return UDS_RESULT_RESPONSE_TOO_LONG;
    }
    break;
  case 0x0AU:
    if (capacity < 2U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    response[offset++] = 0xFFU;
    for (index = 0U; index < (sizeof(demo_dtcs) / sizeof(demo_dtcs[0])); ++index)
      if (!demo_put_dtc(response, capacity, &offset, &demo_dtcs[index]))
        return UDS_RESULT_RESPONSE_TOO_LONG;
    break;
  case 0x0BU:
  case 0x0DU:
    if (capacity < 6U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    response[offset++] = 0xFFU;
    if (!demo_put_dtc(response, capacity, &offset, &demo_dtcs[0]))
      return UDS_RESULT_RESPONSE_TOO_LONG;
    break;
  case 0x0CU:
    if (capacity < 6U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    response[offset++] = 0xFFU;
    if (!demo_put_dtc(response, capacity, &offset, &demo_dtcs[0]))
      return UDS_RESULT_RESPONSE_TOO_LONG;
    break;
  case 0x0EU:
    if (capacity < 6U)
      return UDS_RESULT_RESPONSE_TOO_LONG;
    response[offset++] = 0xFFU;
    if (!demo_put_dtc(response, capacity, &offset, &demo_dtcs[1]))
      return UDS_RESULT_RESPONSE_TOO_LONG;
    break;
  default:
    return UDS_RESULT_OUT_OF_RANGE;
  }

  *response_length = offset;
  return UDS_RESULT_OK;
}

static const UdsDtcBackend demo_dtc_backend = {
  0xFFFFFFFFUL,
  demo_dtc_report
};

static void drain_fdcan_rx_fifo(void)
{
  while (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) != 0U) {
    uint8_t data[64] = {0};
    uint8_t length;

    if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxHeader, data) != HAL_OK)
      return;
    length = uds_c092_fdcan_data_length_bytes(rxHeader.DataLength);
    if ((length == 0U) || (length > sizeof(data)))
      continue;
    uds_c092_app_rx_from_isr(rxHeader.Identifier, data, length,
                            rxHeader.FDFormat == FDCAN_FD_CAN,
                            rxHeader.BitRateSwitch == FDCAN_BRS_ON);
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FDCAN1_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  FDCAN_FilterTypeDef        sFilterConfig;
  sFilterConfig.IdType       = FDCAN_STANDARD_ID;
  sFilterConfig.FilterIndex  = 0U;
  sFilterConfig.FilterType   = FDCAN_FILTER_DUAL;
  sFilterConfig.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
  sFilterConfig.FilterID1    = UDS_C092_REQUEST_ID;
  sFilterConfig.FilterID2    = UDS_C092_FUNCTIONAL_REQUEST_ID;
  if (HAL_FDCAN_ConfigFilter(&hfdcan1, &sFilterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                   FDCAN_REJECT, FDCAN_REJECT,
                                   FDCAN_FILTER_REMOTE, FDCAN_FILTER_REMOTE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_FDCAN_ActivateNotification(&hfdcan1,
        FDCAN_IT_RX_FIFO0_NEW_MESSAGE |
        FDCAN_IT_TX_EVT_FIFO_NEW_DATA |
        FDCAN_IT_TX_EVT_FIFO_ELT_LOST, 0U) != HAL_OK)
  {
    Error_Handler();
  }

  txHeader.Identifier          = TX_ID;
  txHeader.IdType              = FDCAN_STANDARD_ID;
  txHeader.TxFrameType         = FDCAN_DATA_FRAME;
  txHeader.DataLength          = FDCAN_DLC_BYTES_8;
  txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
  txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
  txHeader.FDFormat            = FDCAN_CLASSIC_CAN;
  txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
  txHeader.MessageMarker       = 0U;

  if (HAL_FDCAN_Start(&hfdcan1) != HAL_OK)
  {
    Error_Handler();
  }

  {
    UdsCallbacks callbacks = {0};
    callbacks.read_did = demo_read_did;
    callbacks.dtc_backend = &demo_dtc_backend;
    uds_c092_fdcan_transport_init(&uds_transport, &hfdcan1,
                                  UDS_C092_REQUEST_ID, UDS_C092_RESPONSE_ID);
    uds_c092_app_init(&uds_transport, uds_c092_platform_now_ms(),
                      &callbacks, NULL, NULL, NULL);
  }

  printf("UDS test---\r\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    __disable_irq();
    drain_fdcan_rx_fifo();
    __enable_irq();
    uds_c092_app_process(uds_c092_platform_now_ms());

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
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
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
  if ((hfdcan == &hfdcan1) &&
      ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U))
    drain_fdcan_rx_fifo();
}

void HAL_FDCAN_TxEventFifoCallback(FDCAN_HandleTypeDef *hfdcan,
                                   uint32_t TxEventFifoITs)
{
  if (hfdcan == &hfdcan1)
    uds_c092_fdcan_on_tx_event(&uds_transport, TxEventFifoITs);
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
