// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32c092-hal.c @brief STM32C092 FDCAN binding for LINK. */
#include "link-stm32c092-hal.h"

#include <string.h>

static bool link_stm32c092_dlc_to_length(uint32_t dlc, uint8_t *length)
{
    if (length == NULL) {
        return false;
    }
    switch (dlc) {
    case FDCAN_DLC_BYTES_0: *length = 0U; return true;
    case FDCAN_DLC_BYTES_1: *length = 1U; return true;
    case FDCAN_DLC_BYTES_2: *length = 2U; return true;
    case FDCAN_DLC_BYTES_3: *length = 3U; return true;
    case FDCAN_DLC_BYTES_4: *length = 4U; return true;
    case FDCAN_DLC_BYTES_5: *length = 5U; return true;
    case FDCAN_DLC_BYTES_6: *length = 6U; return true;
    case FDCAN_DLC_BYTES_7: *length = 7U; return true;
    case FDCAN_DLC_BYTES_8: *length = 8U; return true;
    case FDCAN_DLC_BYTES_12: *length = 12U; return true;
    case FDCAN_DLC_BYTES_16: *length = 16U; return true;
    case FDCAN_DLC_BYTES_20: *length = 20U; return true;
    case FDCAN_DLC_BYTES_24: *length = 24U; return true;
    case FDCAN_DLC_BYTES_32: *length = 32U; return true;
    case FDCAN_DLC_BYTES_48: *length = 48U; return true;
    case FDCAN_DLC_BYTES_64: *length = 64U; return true;
    default: *length = 0U; return false;
    }
}

static bool link_stm32c092_length_to_dlc(uint8_t length, uint32_t *dlc)
{
    if (dlc == NULL) {
        return false;
    }
    switch (length) {
    case 0U: *dlc = FDCAN_DLC_BYTES_0; return true;
    case 1U: *dlc = FDCAN_DLC_BYTES_1; return true;
    case 2U: *dlc = FDCAN_DLC_BYTES_2; return true;
    case 3U: *dlc = FDCAN_DLC_BYTES_3; return true;
    case 4U: *dlc = FDCAN_DLC_BYTES_4; return true;
    case 5U: *dlc = FDCAN_DLC_BYTES_5; return true;
    case 6U: *dlc = FDCAN_DLC_BYTES_6; return true;
    case 7U: *dlc = FDCAN_DLC_BYTES_7; return true;
    case 8U: *dlc = FDCAN_DLC_BYTES_8; return true;
    case 12U: *dlc = FDCAN_DLC_BYTES_12; return true;
    case 16U: *dlc = FDCAN_DLC_BYTES_16; return true;
    case 20U: *dlc = FDCAN_DLC_BYTES_20; return true;
    case 24U: *dlc = FDCAN_DLC_BYTES_24; return true;
    case 32U: *dlc = FDCAN_DLC_BYTES_32; return true;
    case 48U: *dlc = FDCAN_DLC_BYTES_48; return true;
    case 64U: *dlc = FDCAN_DLC_BYTES_64; return true;
    default: *dlc = FDCAN_DLC_BYTES_0; return false;
    }
}

static bool link_stm32c092_receive(void *context, LinkIsoTpCanFrame *frame)
{
    LinkStm32C092Hal *adapter = (LinkStm32C092Hal *)context;
    FDCAN_RxHeaderTypeDef header;
    uint8_t data[LINK_ISOTP_CAN_FD_MAX_DATA_LENGTH];
    uint8_t length;

    if (adapter == NULL || adapter->hfdcan == NULL || frame == NULL) {
        return false;
    }

    while (HAL_FDCAN_GetRxFifoFillLevel(adapter->hfdcan, FDCAN_RX_FIFO0) != 0U) {
        memset(&header, 0, sizeof(header));
        memset(data, 0, sizeof(data));
        if (HAL_FDCAN_GetRxMessage(
                adapter->hfdcan, FDCAN_RX_FIFO0, &header, data) != HAL_OK) {
            return false;
        }
        if (header.RxFrameType != FDCAN_DATA_FRAME ||
            !link_stm32c092_dlc_to_length(header.DataLength, &length)) {
            continue;
        }

        memset(frame, 0, sizeof(*frame));
        frame->can_id = header.Identifier;
        frame->extended_id = header.IdType == FDCAN_EXTENDED_ID;
        frame->length = length;
        frame->can_fd = header.FDFormat == FDCAN_FD_CAN;
        if (length != 0U) {
            memcpy(frame->data, data, length);
        }
        return true;
    }

    return false;
}

static bool link_stm32c092_tx_ready(void *context)
{
    LinkStm32C092Hal *adapter = (LinkStm32C092Hal *)context;
    return adapter != NULL && adapter->hfdcan != NULL &&
           HAL_FDCAN_GetTxFifoFreeLevel(adapter->hfdcan) != 0U;
}

static bool link_stm32c092_send(
    void *context,
    const LinkIsoTpCanFrame *frame)
{
    LinkStm32C092Hal *adapter = (LinkStm32C092Hal *)context;
    FDCAN_TxHeaderTypeDef header;
    uint32_t dlc;

    if (adapter == NULL || adapter->hfdcan == NULL || frame == NULL ||
        !link_stm32c092_length_to_dlc(frame->length, &dlc)) {
        return false;
    }

    memset(&header, 0, sizeof(header));
    header.Identifier = frame->can_id;
    header.IdType = frame->extended_id ? FDCAN_EXTENDED_ID : FDCAN_STANDARD_ID;
    header.TxFrameType = FDCAN_DATA_FRAME;
    header.DataLength = dlc;
    header.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    header.BitRateSwitch = frame->can_fd && adapter->fd_bit_rate_switch
        ? FDCAN_BRS_ON : FDCAN_BRS_OFF;
    header.FDFormat = frame->can_fd ? FDCAN_FD_CAN : FDCAN_CLASSIC_CAN;
    header.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    header.MessageMarker = 0U;

    return HAL_FDCAN_AddMessageToTxFifoQ(
        adapter->hfdcan, &header, frame->data) == HAL_OK;
}

static uint32_t link_stm32c092_clock_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

void link_stm32c092_hal_init(
    LinkStm32C092Hal *adapter,
    FDCAN_HandleTypeDef *hfdcan,
    bool fd_bit_rate_switch)
{
    if (adapter == NULL) {
        return;
    }
    adapter->hfdcan = hfdcan;
    adapter->fd_bit_rate_switch = fd_bit_rate_switch;
}

LinkStm32CanOps link_stm32c092_hal_ops(LinkStm32C092Hal *adapter)
{
    LinkStm32CanOps ops;
    memset(&ops, 0, sizeof(ops));
    ops.context = adapter;
    ops.receive = link_stm32c092_receive;
    ops.tx_ready = link_stm32c092_tx_ready;
    ops.send = link_stm32c092_send;
    ops.clock_ms = link_stm32c092_clock_ms;
    return ops;
}

bool link_stm32c092_hal_start_standard(
    LinkStm32C092Hal *adapter,
    uint32_t response_id)
{
    FDCAN_FilterTypeDef filter;

    if (adapter == NULL || adapter->hfdcan == NULL || response_id > 0x7ffU) {
        return false;
    }

    memset(&filter, 0, sizeof(filter));
    filter.IdType = FDCAN_STANDARD_ID;
    filter.FilterIndex = 0U;
    filter.FilterType = FDCAN_FILTER_MASK;
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1 = response_id;
    filter.FilterID2 = 0x7ffU;

    if (HAL_FDCAN_ConfigFilter(adapter->hfdcan, &filter) != HAL_OK ||
        HAL_FDCAN_ConfigGlobalFilter(
            adapter->hfdcan,
            FDCAN_REJECT,
            FDCAN_REJECT,
            FDCAN_FILTER_REMOTE,
            FDCAN_FILTER_REMOTE) != HAL_OK ||
        HAL_FDCAN_ActivateNotification(
            adapter->hfdcan,
            FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
            0U) != HAL_OK ||
        HAL_FDCAN_Start(adapter->hfdcan) != HAL_OK) {
        return false;
    }

    return true;
}
