// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-bxcan-hal.c @brief STM32Cube bxCAN binding for LINK. */
#include "link-stm32-bxcan-hal.h"

#include <string.h>

static bool link_stm32_bxcan_mailbox_valid(uint32_t mailbox)
{
    return mailbox == CAN_TX_MAILBOX0 ||
           mailbox == CAN_TX_MAILBOX1 ||
           mailbox == CAN_TX_MAILBOX2;
}

static bool link_stm32_bxcan_receive(
    void *context,
    LinkIsoTpCanFrame *frame)
{
    LinkStm32BxCanHal *adapter = (LinkStm32BxCanHal *)context;
    CAN_RxHeaderTypeDef header;
    uint8_t data[LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH];

    if (adapter == NULL || adapter->hcan == NULL || frame == NULL) {
        return false;
    }

    while (HAL_CAN_GetRxFifoFillLevel(
               adapter->hcan, CAN_RX_FIFO0) != 0U) {
        memset(&header, 0, sizeof(header));
        memset(data, 0, sizeof(data));
        if (HAL_CAN_GetRxMessage(
                adapter->hcan, CAN_RX_FIFO0, &header, data) != HAL_OK) {
            return false;
        }
        if (header.RTR != CAN_RTR_DATA ||
            header.DLC > LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH ||
            (header.IDE != CAN_ID_STD && header.IDE != CAN_ID_EXT)) {
            continue;
        }

        memset(frame, 0, sizeof(*frame));
        frame->extended_id = header.IDE == CAN_ID_EXT;
        frame->can_id = frame->extended_id ? header.ExtId : header.StdId;
        if ((!frame->extended_id && frame->can_id > UINT32_C(0x7ff)) ||
            (frame->extended_id && frame->can_id > UINT32_C(0x1fffffff))) {
            continue;
        }
        frame->length = (uint8_t)header.DLC;
        frame->can_fd = false;
        if (frame->length != 0U) {
            memcpy(frame->data, data, frame->length);
        }
        return true;
    }
    return false;
}

static bool link_stm32_bxcan_tx_ready(void *context)
{
    LinkStm32BxCanHal *adapter = (LinkStm32BxCanHal *)context;

    return adapter != NULL && adapter->hcan != NULL &&
           adapter->pending_mailbox == 0U &&
           HAL_CAN_GetTxMailboxesFreeLevel(adapter->hcan) != 0U;
}

static bool link_stm32_bxcan_send(
    void *context,
    const LinkIsoTpCanFrame *frame)
{
    LinkStm32BxCanHal *adapter = (LinkStm32BxCanHal *)context;
    CAN_TxHeaderTypeDef header;
    uint32_t mailbox = 0U;

    if (adapter == NULL || adapter->hcan == NULL || frame == NULL ||
        adapter->pending_mailbox != 0U || frame->can_fd ||
        frame->length == 0U ||
        frame->length > LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH ||
        (!frame->extended_id && frame->can_id > UINT32_C(0x7ff)) ||
        (frame->extended_id && frame->can_id > UINT32_C(0x1fffffff))) {
        return false;
    }

    memset(&header, 0, sizeof(header));
    header.StdId = frame->extended_id ? 0U : frame->can_id;
    header.ExtId = frame->extended_id ? frame->can_id : 0U;
    header.IDE = frame->extended_id ? CAN_ID_EXT : CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = frame->length;
    header.TransmitGlobalTime = DISABLE;

    adapter->completed_mailbox = 0U;
    adapter->failed_mailbox = 0U;
    adapter->completed_tick_ms = 0U;
    if (HAL_CAN_AddTxMessage(
            adapter->hcan, &header, frame->data, &mailbox) != HAL_OK ||
        !link_stm32_bxcan_mailbox_valid(mailbox)) {
        return false;
    }

    /* A Classical CAN frame cannot finish on the wire before this HAL call
     * returns, so the mailbox token is installed before its completion IRQ. */
    adapter->pending_mailbox = mailbox;
    return true;
}

static LinkStm32CanTxStatus link_stm32_bxcan_tx_status(
    void *context,
    uint32_t *completion_tick_ms)
{
    LinkStm32BxCanHal *adapter = (LinkStm32BxCanHal *)context;
    uint32_t hardware_pending;
    uint32_t pending;

    if (completion_tick_ms != NULL) {
        *completion_tick_ms = 0U;
    }
    if (adapter == NULL || adapter->hcan == NULL) {
        return LINK_STM32_CAN_TX_FAILED;
    }

    pending = adapter->pending_mailbox;
    if (pending == 0U) {
        return LINK_STM32_CAN_TX_IDLE;
    }
    if (adapter->completed_mailbox == pending) {
        if (completion_tick_ms != NULL) {
            *completion_tick_ms = adapter->completed_tick_ms;
        }
        adapter->completed_mailbox = 0U;
        adapter->failed_mailbox = 0U;
        adapter->pending_mailbox = 0U;
        return LINK_STM32_CAN_TX_COMPLETE;
    }
    hardware_pending = HAL_CAN_IsTxMessagePending(adapter->hcan, pending);
    if (adapter->failed_mailbox == pending && hardware_pending == 0U) {
        adapter->completed_mailbox = 0U;
        adapter->failed_mailbox = 0U;
        adapter->pending_mailbox = 0U;
        return LINK_STM32_CAN_TX_FAILED;
    }
    if (hardware_pending != 0U) {
        return LINK_STM32_CAN_TX_PENDING;
    }

    /* A released mailbox without its matching completion callback is a
     * terminal failure. This covers abort/error callbacks and incomplete
     * application callback wiring without inventing a completion timestamp. */
    adapter->completed_mailbox = 0U;
    adapter->failed_mailbox = 0U;
    adapter->pending_mailbox = 0U;
    return LINK_STM32_CAN_TX_FAILED;
}

static uint32_t link_stm32_bxcan_clock_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

void link_stm32_bxcan_hal_init(
    LinkStm32BxCanHal *adapter,
    CAN_HandleTypeDef *hcan,
    uint32_t filter_bank,
    uint32_t slave_start_filter_bank)
{
    if (adapter == NULL) {
        return;
    }
    memset(adapter, 0, sizeof(*adapter));
    adapter->hcan = hcan;
    adapter->filter_bank = filter_bank;
    adapter->slave_start_filter_bank = slave_start_filter_bank;
}

LinkStm32CanOps link_stm32_bxcan_hal_ops(LinkStm32BxCanHal *adapter)
{
    LinkStm32CanOps ops;

    memset(&ops, 0, sizeof(ops));
    ops.context = adapter;
    ops.receive = link_stm32_bxcan_receive;
    ops.tx_ready = link_stm32_bxcan_tx_ready;
    ops.send = link_stm32_bxcan_send;
    ops.tx_status = link_stm32_bxcan_tx_status;
    ops.clock_ms = link_stm32_bxcan_clock_ms;
    return ops;
}

static bool link_stm32_bxcan_hal_start_standard_filter(
    LinkStm32BxCanHal *adapter,
    uint32_t receive_id,
    uint32_t second_id)
{
    CAN_FilterTypeDef filter;
    const uint32_t notifications =
        CAN_IT_RX_FIFO0_MSG_PENDING |
        CAN_IT_RX_FIFO0_FULL |
        CAN_IT_RX_FIFO0_OVERRUN |
        CAN_IT_TX_MAILBOX_EMPTY |
        CAN_IT_ERROR_WARNING |
        CAN_IT_ERROR_PASSIVE |
        CAN_IT_BUSOFF |
        CAN_IT_LAST_ERROR_CODE |
        CAN_IT_ERROR;

    if (adapter == NULL || adapter->hcan == NULL ||
        receive_id > UINT32_C(0x7ff) ||
        second_id > UINT32_C(0x7ff) ||
        adapter->filter_bank > 27U ||
        adapter->slave_start_filter_bank > 27U) {
        return false;
    }

    memset(&filter, 0, sizeof(filter));
    /* In 32-bit list mode these are two complete bxCAN filter words. Shifting
     * a standard identifier by five leaves IDE and RTR clear, so only standard
     * data frames with either requested ID reach FIFO0. */
    filter.FilterIdHigh = receive_id << 5U;
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = second_id << 5U;
    filter.FilterMaskIdLow = 0U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterBank = adapter->filter_bank;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = adapter->slave_start_filter_bank;

    if (HAL_CAN_ConfigFilter(adapter->hcan, &filter) != HAL_OK ||
        HAL_CAN_ActivateNotification(
            adapter->hcan, notifications) != HAL_OK ||
        HAL_CAN_Start(adapter->hcan) != HAL_OK) {
        return false;
    }
    return true;
}

bool link_stm32_bxcan_hal_start_standard(
    LinkStm32BxCanHal *adapter,
    uint32_t receive_id)
{
    return link_stm32_bxcan_hal_start_standard_filter(
        adapter, receive_id, receive_id);
}

bool link_stm32_bxcan_hal_start_standard_dual(
    LinkStm32BxCanHal *adapter,
    uint32_t receive_id,
    uint32_t functional_receive_id)
{
    return link_stm32_bxcan_hal_start_standard_filter(
        adapter, receive_id, functional_receive_id);
}

void link_stm32_bxcan_hal_tx_complete_irq(
    LinkStm32BxCanHal *adapter,
    uint32_t mailbox)
{
    if (adapter == NULL || adapter->pending_mailbox == 0U ||
        mailbox != adapter->pending_mailbox) {
        return;
    }
    adapter->completed_tick_ms = HAL_GetTick();
    adapter->completed_mailbox = mailbox;
}

void link_stm32_bxcan_hal_tx_abort_irq(
    LinkStm32BxCanHal *adapter,
    uint32_t mailbox)
{
    if (adapter != NULL && adapter->pending_mailbox != 0U &&
        mailbox == adapter->pending_mailbox) {
        adapter->failed_mailbox = mailbox;
    }
}

void link_stm32_bxcan_hal_error_irq(LinkStm32BxCanHal *adapter)
{
    if (adapter != NULL && adapter->pending_mailbox != 0U) {
        adapter->failed_mailbox = adapter->pending_mailbox;
    }
}
