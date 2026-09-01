// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32C092_FDCAN_H
#define LINK_TEST_STM32C092_FDCAN_H

#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

typedef struct {
    uint32_t opaque;
} FDCAN_HandleTypeDef;

typedef struct {
    uint32_t Identifier;
    uint32_t IdType;
    uint32_t RxFrameType;
    uint32_t DataLength;
    uint32_t ErrorStateIndicator;
    uint32_t BitRateSwitch;
    uint32_t FDFormat;
    uint32_t RxTimestamp;
    uint32_t FilterIndex;
    uint32_t IsFilterMatchingFrame;
} FDCAN_RxHeaderTypeDef;

typedef struct {
    uint32_t Identifier;
    uint32_t IdType;
    uint32_t TxFrameType;
    uint32_t DataLength;
    uint32_t ErrorStateIndicator;
    uint32_t BitRateSwitch;
    uint32_t FDFormat;
    uint32_t TxEventFifoControl;
    uint32_t MessageMarker;
} FDCAN_TxHeaderTypeDef;

typedef struct {
    uint32_t Identifier;
    uint32_t IdType;
    uint32_t TxFrameType;
    uint32_t DataLength;
    uint32_t ErrorStateIndicator;
    uint32_t BitRateSwitch;
    uint32_t FDFormat;
    uint32_t TxTimestamp;
    uint32_t MessageMarker;
    uint32_t EventType;
} FDCAN_TxEventFifoTypeDef;

typedef struct {
    uint32_t IdType;
    uint32_t FilterIndex;
    uint32_t FilterType;
    uint32_t FilterConfig;
    uint32_t FilterID1;
    uint32_t FilterID2;
} FDCAN_FilterTypeDef;

#define FDCAN_DLC_BYTES_0  UINT32_C(0x00000000)
#define FDCAN_DLC_BYTES_1  UINT32_C(0x00010000)
#define FDCAN_DLC_BYTES_2  UINT32_C(0x00020000)
#define FDCAN_DLC_BYTES_3  UINT32_C(0x00030000)
#define FDCAN_DLC_BYTES_4  UINT32_C(0x00040000)
#define FDCAN_DLC_BYTES_5  UINT32_C(0x00050000)
#define FDCAN_DLC_BYTES_6  UINT32_C(0x00060000)
#define FDCAN_DLC_BYTES_7  UINT32_C(0x00070000)
#define FDCAN_DLC_BYTES_8  UINT32_C(0x00080000)
#define FDCAN_DLC_BYTES_12 UINT32_C(0x00090000)
#define FDCAN_DLC_BYTES_16 UINT32_C(0x000A0000)
#define FDCAN_DLC_BYTES_20 UINT32_C(0x000B0000)
#define FDCAN_DLC_BYTES_24 UINT32_C(0x000C0000)
#define FDCAN_DLC_BYTES_32 UINT32_C(0x000D0000)
#define FDCAN_DLC_BYTES_48 UINT32_C(0x000E0000)
#define FDCAN_DLC_BYTES_64 UINT32_C(0x000F0000)

#define FDCAN_STANDARD_ID UINT32_C(0)
#define FDCAN_EXTENDED_ID UINT32_C(1)
#define FDCAN_DATA_FRAME UINT32_C(0)
#define FDCAN_ESI_ACTIVE UINT32_C(0)
#define FDCAN_BRS_OFF UINT32_C(0)
#define FDCAN_BRS_ON UINT32_C(1)
#define FDCAN_CLASSIC_CAN UINT32_C(0)
#define FDCAN_FD_CAN UINT32_C(1)
#define FDCAN_STORE_TX_EVENTS UINT32_C(1)
#define FDCAN_TX_EVENT UINT32_C(1)
#define FDCAN_TX_IN_SPITE_OF_ABORT UINT32_C(2)
#define FDCAN_RX_FIFO0 UINT32_C(0)
#define FDCAN_FILTER_MASK UINT32_C(2)
#define FDCAN_FILTER_TO_RXFIFO0 UINT32_C(1)
#define FDCAN_REJECT UINT32_C(3)
#define FDCAN_FILTER_REMOTE UINT32_C(0)
#define FDCAN_IT_RX_FIFO0_NEW_MESSAGE UINT32_C(0x00000001)
#define FDCAN_IT_TX_EVT_FIFO_NEW_DATA UINT32_C(0x00000002)
#define FDCAN_IT_TX_EVT_FIFO_ELT_LOST UINT32_C(0x00000004)

uint32_t HAL_GetTick(void);
uint32_t HAL_FDCAN_GetRxFifoFillLevel(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t rx_fifo);
HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t rx_location,
    FDCAN_RxHeaderTypeDef *header,
    uint8_t *data);
uint32_t HAL_FDCAN_GetTxFifoFreeLevel(FDCAN_HandleTypeDef *hfdcan);
HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(
    FDCAN_HandleTypeDef *hfdcan,
    const FDCAN_TxHeaderTypeDef *header,
    const uint8_t *data);
HAL_StatusTypeDef HAL_FDCAN_GetTxEvent(
    FDCAN_HandleTypeDef *hfdcan,
    FDCAN_TxEventFifoTypeDef *event);
HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(
    FDCAN_HandleTypeDef *hfdcan,
    const FDCAN_FilterTypeDef *filter);
HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t non_matching_std,
    uint32_t non_matching_ext,
    uint32_t reject_remote_std,
    uint32_t reject_remote_ext);
HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t active_it,
    uint32_t buffer_indexes);
HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *hfdcan);

#endif
