// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_STM32_BXCAN_H
#define LINK_TEST_STM32_BXCAN_H

#include <stdint.h>

typedef enum {
    HAL_OK = 0,
    HAL_ERROR = 1
} HAL_StatusTypeDef;

typedef enum {
    DISABLE = 0,
    ENABLE = 1
} FunctionalState;

typedef struct {
    uint32_t opaque;
} CAN_HandleTypeDef;

typedef struct {
    uint32_t FilterIdHigh;
    uint32_t FilterIdLow;
    uint32_t FilterMaskIdHigh;
    uint32_t FilterMaskIdLow;
    uint32_t FilterFIFOAssignment;
    uint32_t FilterBank;
    uint32_t FilterMode;
    uint32_t FilterScale;
    uint32_t FilterActivation;
    uint32_t SlaveStartFilterBank;
} CAN_FilterTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    FunctionalState TransmitGlobalTime;
} CAN_TxHeaderTypeDef;

typedef struct {
    uint32_t StdId;
    uint32_t ExtId;
    uint32_t IDE;
    uint32_t RTR;
    uint32_t DLC;
    uint32_t Timestamp;
    uint32_t FilterMatchIndex;
} CAN_RxHeaderTypeDef;

#define CAN_ID_STD UINT32_C(0)
#define CAN_ID_EXT UINT32_C(4)
#define CAN_RTR_DATA UINT32_C(0)
#define CAN_RTR_REMOTE UINT32_C(2)
#define CAN_RX_FIFO0 UINT32_C(0)
#define CAN_FILTERMODE_IDLIST UINT32_C(1)
#define CAN_FILTERSCALE_32BIT UINT32_C(1)
#define CAN_FILTER_FIFO0 UINT32_C(0)
#define CAN_TX_MAILBOX0 UINT32_C(1)
#define CAN_TX_MAILBOX1 UINT32_C(2)
#define CAN_TX_MAILBOX2 UINT32_C(4)

#define CAN_IT_TX_MAILBOX_EMPTY UINT32_C(0x00000001)
#define CAN_IT_RX_FIFO0_MSG_PENDING UINT32_C(0x00000002)
#define CAN_IT_RX_FIFO0_FULL UINT32_C(0x00000004)
#define CAN_IT_RX_FIFO0_OVERRUN UINT32_C(0x00000008)
#define CAN_IT_ERROR_WARNING UINT32_C(0x00000010)
#define CAN_IT_ERROR_PASSIVE UINT32_C(0x00000020)
#define CAN_IT_BUSOFF UINT32_C(0x00000040)
#define CAN_IT_LAST_ERROR_CODE UINT32_C(0x00000080)
#define CAN_IT_ERROR UINT32_C(0x00000100)

uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_CAN_ConfigFilter(
    CAN_HandleTypeDef *hcan,
    const CAN_FilterTypeDef *filter);
HAL_StatusTypeDef HAL_CAN_ActivateNotification(
    CAN_HandleTypeDef *hcan,
    uint32_t active_its);
HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan);
uint32_t HAL_CAN_GetRxFifoFillLevel(
    const CAN_HandleTypeDef *hcan,
    uint32_t rx_fifo);
HAL_StatusTypeDef HAL_CAN_GetRxMessage(
    CAN_HandleTypeDef *hcan,
    uint32_t rx_fifo,
    CAN_RxHeaderTypeDef *header,
    uint8_t data[]);
uint32_t HAL_CAN_GetTxMailboxesFreeLevel(
    const CAN_HandleTypeDef *hcan);
uint32_t HAL_CAN_IsTxMessagePending(
    const CAN_HandleTypeDef *hcan,
    uint32_t tx_mailboxes);
HAL_StatusTypeDef HAL_CAN_AddTxMessage(
    CAN_HandleTypeDef *hcan,
    const CAN_TxHeaderTypeDef *header,
    const uint8_t data[],
    uint32_t *tx_mailbox);

#endif
