// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32-bxcan-hal.h"
#include "link-stm32-bxcan-example.h"
#include "link-stm32-can.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "Requirement failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

#define RX_CAPACITY 4U

static uint32_t fake_tick;
static uint32_t fake_notifications;
static uint32_t fake_tx_free;
static uint32_t fake_tx_mailbox;
static uint32_t fake_tx_pending;
static bool fake_filter_configured;
static bool fake_filter_ok;
static bool fake_notifications_ok;
static bool fake_start_ok;
static bool fake_tx_ok;
static CAN_FilterTypeDef fake_filter;
static CAN_TxHeaderTypeDef fake_tx_header;
static uint8_t fake_tx_data[LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH];
static CAN_RxHeaderTypeDef fake_rx_headers[RX_CAPACITY];
static uint8_t fake_rx_data[RX_CAPACITY][LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH];
static uint8_t fake_rx_head;
static uint8_t fake_rx_tail;

static void reset_fake_hal(void)
{
    fake_tick = 0U;
    fake_notifications = 0U;
    fake_tx_free = 3U;
    fake_tx_mailbox = CAN_TX_MAILBOX1;
    fake_tx_pending = 0U;
    fake_filter_configured = false;
    fake_filter_ok = true;
    fake_notifications_ok = true;
    fake_start_ok = true;
    fake_tx_ok = true;
    memset(&fake_filter, 0, sizeof(fake_filter));
    memset(&fake_tx_header, 0, sizeof(fake_tx_header));
    memset(fake_tx_data, 0, sizeof(fake_tx_data));
    memset(fake_rx_headers, 0, sizeof(fake_rx_headers));
    memset(fake_rx_data, 0, sizeof(fake_rx_data));
    fake_rx_head = 0U;
    fake_rx_tail = 0U;
}

static void queue_rx(
    const CAN_RxHeaderTypeDef *header,
    const uint8_t *data)
{
    const uint8_t index = (uint8_t)(fake_rx_head % RX_CAPACITY);

    fake_rx_headers[index] = *header;
    memcpy(fake_rx_data[index], data, LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH);
    ++fake_rx_head;
}

uint32_t HAL_GetTick(void)
{
    return fake_tick;
}

HAL_StatusTypeDef HAL_CAN_ConfigFilter(
    CAN_HandleTypeDef *hcan,
    const CAN_FilterTypeDef *filter)
{
    if (hcan == NULL || filter == NULL || !fake_filter_ok) {
        return HAL_ERROR;
    }
    fake_filter = *filter;
    fake_filter_configured = true;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_ActivateNotification(
    CAN_HandleTypeDef *hcan,
    uint32_t active_its)
{
    if (hcan == NULL || !fake_notifications_ok) {
        return HAL_ERROR;
    }
    fake_notifications = active_its;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_CAN_Start(CAN_HandleTypeDef *hcan)
{
    return hcan != NULL && fake_start_ok ? HAL_OK : HAL_ERROR;
}

uint32_t HAL_CAN_GetRxFifoFillLevel(
    const CAN_HandleTypeDef *hcan,
    uint32_t rx_fifo)
{
    (void)rx_fifo;
    return hcan != NULL ? (uint32_t)(fake_rx_head - fake_rx_tail) : 0U;
}

HAL_StatusTypeDef HAL_CAN_GetRxMessage(
    CAN_HandleTypeDef *hcan,
    uint32_t rx_fifo,
    CAN_RxHeaderTypeDef *header,
    uint8_t data[])
{
    uint8_t index;

    (void)rx_fifo;
    if (hcan == NULL || header == NULL || data == NULL ||
        fake_rx_tail == fake_rx_head) {
        return HAL_ERROR;
    }
    index = (uint8_t)(fake_rx_tail % RX_CAPACITY);
    *header = fake_rx_headers[index];
    memcpy(data, fake_rx_data[index], LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH);
    ++fake_rx_tail;
    return HAL_OK;
}

uint32_t HAL_CAN_GetTxMailboxesFreeLevel(const CAN_HandleTypeDef *hcan)
{
    return hcan != NULL ? fake_tx_free : 0U;
}

uint32_t HAL_CAN_IsTxMessagePending(
    const CAN_HandleTypeDef *hcan,
    uint32_t tx_mailboxes)
{
    return hcan != NULL && (fake_tx_pending & tx_mailboxes) != 0U ? 1U : 0U;
}

HAL_StatusTypeDef HAL_CAN_AddTxMessage(
    CAN_HandleTypeDef *hcan,
    const CAN_TxHeaderTypeDef *header,
    const uint8_t data[],
    uint32_t *tx_mailbox)
{
    if (hcan == NULL || header == NULL || data == NULL ||
        tx_mailbox == NULL || !fake_tx_ok || fake_tx_free == 0U) {
        return HAL_ERROR;
    }
    fake_tx_header = *header;
    memcpy(fake_tx_data, data, LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH);
    *tx_mailbox = fake_tx_mailbox;
    fake_tx_pending |= fake_tx_mailbox;
    return HAL_OK;
}

static int test_exact_standard_filters(void)
{
    CAN_HandleTypeDef hcan;
    LinkStm32BxCanHal adapter;

    reset_fake_hal();
    memset(&hcan, 0, sizeof(hcan));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 3U, 14U);
    REQUIRE(link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x7e8)));
    REQUIRE(fake_filter_configured);
    REQUIRE(fake_filter.FilterIdHigh == (UINT32_C(0x7e8) << 5U));
    REQUIRE(fake_filter.FilterMaskIdHigh == (UINT32_C(0x7e8) << 5U));
    REQUIRE(fake_filter.FilterIdLow == 0U);
    REQUIRE(fake_filter.FilterMaskIdLow == 0U);
    REQUIRE(fake_filter.FilterMode == CAN_FILTERMODE_IDLIST);
    REQUIRE(fake_filter.FilterScale == CAN_FILTERSCALE_32BIT);
    REQUIRE(fake_filter.FilterFIFOAssignment == CAN_FILTER_FIFO0);
    REQUIRE(fake_filter.FilterActivation == ENABLE);
    REQUIRE(fake_filter.FilterBank == 3U);
    REQUIRE(fake_filter.SlaveStartFilterBank == 14U);
    REQUIRE((fake_notifications & CAN_IT_RX_FIFO0_MSG_PENDING) != 0U);
    REQUIRE((fake_notifications & CAN_IT_TX_MAILBOX_EMPTY) != 0U);
    REQUIRE((fake_notifications & CAN_IT_ERROR) != 0U);

    reset_fake_hal();
    link_stm32_bxcan_hal_init(&adapter, &hcan, 14U, 14U);
    REQUIRE(link_stm32_bxcan_hal_start_standard_dual(
        &adapter, UINT32_C(0x7e0), UINT32_C(0x7df)));
    REQUIRE(fake_filter.FilterIdHigh == (UINT32_C(0x7e0) << 5U));
    REQUIRE(fake_filter.FilterMaskIdHigh == (UINT32_C(0x7df) << 5U));
    REQUIRE(fake_filter.FilterBank == 14U);
    return 0;
}

static int test_classic_tx_completion(void)
{
    CAN_HandleTypeDef hcan;
    LinkStm32BxCanHal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;
    uint64_t completion_us = 0U;

    reset_fake_hal();
    memset(&hcan, 0, sizeof(hcan));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    ops = link_stm32_bxcan_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    memset(&frame, 0, sizeof(frame));
    frame.can_id = UINT32_C(0x7e0);
    frame.length = 4U;
    frame.data[0] = 3U;
    frame.data[1] = 0x22U;
    frame.data[2] = 0xf1U;
    frame.data[3] = 0x90U;
    REQUIRE(link_stm32_can_send(&channel, &frame));
    REQUIRE(fake_tx_header.StdId == UINT32_C(0x7e0));
    REQUIRE(fake_tx_header.ExtId == 0U);
    REQUIRE(fake_tx_header.IDE == CAN_ID_STD);
    REQUIRE(fake_tx_header.RTR == CAN_RTR_DATA);
    REQUIRE(fake_tx_header.DLC == 4U);
    REQUIRE(fake_tx_header.TransmitGlobalTime == DISABLE);
    REQUIRE(memcmp(fake_tx_data, frame.data, frame.length) == 0);
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &completion_us) == LINK_STM32_CAN_TX_PENDING);

    fake_tick = 5U;
    link_stm32_bxcan_hal_tx_complete_irq(&adapter, CAN_TX_MAILBOX0);
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &completion_us) == LINK_STM32_CAN_TX_PENDING);
    fake_tick = 7U;
    fake_tx_pending &= ~CAN_TX_MAILBOX1;
    link_stm32_bxcan_hal_tx_complete_irq(&adapter, CAN_TX_MAILBOX1);
    fake_tick = 11U;
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &completion_us) == LINK_STM32_CAN_TX_COMPLETE);
    REQUIRE(completion_us == UINT64_C(7000));
    REQUIRE(link_stm32_can_tx_ready(&channel));
    return 0;
}

static int test_extended_rx_mapping(void)
{
    CAN_HandleTypeDef hcan;
    LinkStm32BxCanHal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;
    CAN_RxHeaderTypeDef header;
    uint8_t data[LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH] = {
        3U, 0x7fU, 0x22U, 0x31U, 0U, 0U, 0U, 0U
    };
    uint64_t arrival_us = 0U;

    reset_fake_hal();
    memset(&hcan, 0, sizeof(hcan));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    ops = link_stm32_bxcan_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    memset(&header, 0, sizeof(header));
    header.StdId = UINT32_C(0x7e8);
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_REMOTE;
    header.DLC = 8U;
    queue_rx(&header, data);

    memset(&header, 0, sizeof(header));
    header.ExtId = UINT32_C(0x18daf110);
    header.IDE = CAN_ID_EXT;
    header.RTR = CAN_RTR_DATA;
    header.DLC = 8U;
    queue_rx(&header, data);

    fake_tick = 20U;
    link_stm32_can_rx_isr(&channel);
    fake_tick = 25U;
    REQUIRE(link_stm32_can_pop_timed(&channel, &frame, &arrival_us));
    REQUIRE(arrival_us == UINT64_C(20000));
    REQUIRE(frame.extended_id);
    REQUIRE(!frame.can_fd);
    REQUIRE(frame.can_id == UINT32_C(0x18daf110));
    REQUIRE(frame.length == 8U);
    REQUIRE(memcmp(frame.data, data, sizeof(data)) == 0);
    REQUIRE(!link_stm32_can_pop(&channel, &frame));
    return 0;
}

static int test_failures_and_rejections(void)
{
    CAN_HandleTypeDef hcan;
    LinkStm32BxCanHal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;

    reset_fake_hal();
    memset(&hcan, 0, sizeof(hcan));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    ops = link_stm32_bxcan_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    memset(&frame, 0, sizeof(frame));
    frame.can_id = UINT32_C(0x18daf110);
    frame.extended_id = true;
    frame.length = 8U;
    REQUIRE(link_stm32_can_send(&channel, &frame));
    REQUIRE(fake_tx_header.ExtId == UINT32_C(0x18daf110));
    REQUIRE(fake_tx_header.IDE == CAN_ID_EXT);
    fake_tx_pending &= ~CAN_TX_MAILBOX1;
    link_stm32_bxcan_hal_tx_abort_irq(&adapter, CAN_TX_MAILBOX1);
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, NULL) == LINK_STM32_CAN_TX_FAILED);

    REQUIRE(link_stm32_can_send(&channel, &frame));
    link_stm32_bxcan_hal_error_irq(&adapter);
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, NULL) == LINK_STM32_CAN_TX_PENDING);
    fake_tx_pending &= ~CAN_TX_MAILBOX1;
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, NULL) == LINK_STM32_CAN_TX_FAILED);

    frame.can_fd = true;
    REQUIRE(!ops.send(ops.context, &frame));
    fake_tx_free = 0U;
    REQUIRE(!ops.tx_ready(ops.context));

    REQUIRE(!link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x800)));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 28U, 14U);
    REQUIRE(!link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x7e8)));
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    fake_filter_ok = false;
    REQUIRE(!link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x7e8)));
    reset_fake_hal();
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    fake_notifications_ok = false;
    REQUIRE(!link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x7e8)));
    reset_fake_hal();
    link_stm32_bxcan_hal_init(&adapter, &hcan, 0U, 14U);
    fake_start_ok = false;
    REQUIRE(!link_stm32_bxcan_hal_start_standard(
        &adapter, UINT32_C(0x7e8)));
    return 0;
}

static int test_vin_example_end_to_end(void)
{
    static const char vin[] = "WDB12345678901234";
    CAN_HandleTypeDef hcan;
    CAN_RxHeaderTypeDef header;
    uint8_t data[LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH];
    size_t index;

    reset_fake_hal();
    memset(&hcan, 0, sizeof(hcan));
    REQUIRE(link_stm32_bxcan_example_init(&hcan));
    REQUIRE(link_stm32_bxcan_example_state() ==
        LINK_STM32_BXCAN_EXAMPLE_READING_VIN);
    REQUIRE(fake_filter.FilterIdHigh == (UINT32_C(0x7e8) << 5U));

    link_stm32_bxcan_example_process();
    REQUIRE(fake_tx_pending == CAN_TX_MAILBOX1);
    REQUIRE(fake_tx_header.StdId == UINT32_C(0x7e0));
    REQUIRE(fake_tx_header.DLC == 4U);
    REQUIRE(fake_tx_data[0] == 3U);
    REQUIRE(fake_tx_data[1] == 0x22U);
    REQUIRE(fake_tx_data[2] == 0xf1U);
    REQUIRE(fake_tx_data[3] == 0x90U);

    fake_tick = 1U;
    fake_tx_pending = 0U;
    link_stm32_bxcan_example_tx_complete_irq(
        &hcan, CAN_TX_MAILBOX1);
    link_stm32_bxcan_example_process();

    memset(&header, 0, sizeof(header));
    header.StdId = UINT32_C(0x7e8);
    header.IDE = CAN_ID_STD;
    header.RTR = CAN_RTR_DATA;
    header.DLC = 8U;
    memset(data, 0, sizeof(data));
    data[0] = 0x10U;
    data[1] = 0x14U;
    data[2] = 0x62U;
    data[3] = 0xf1U;
    data[4] = 0x90U;
    data[5] = (uint8_t)vin[0];
    data[6] = (uint8_t)vin[1];
    data[7] = (uint8_t)vin[2];
    queue_rx(&header, data);
    fake_tick = 10U;
    link_stm32_bxcan_example_rx_fifo0_irq(&hcan);
    link_stm32_bxcan_example_process();

    REQUIRE(fake_tx_pending == CAN_TX_MAILBOX1);
    REQUIRE(fake_tx_header.StdId == UINT32_C(0x7e0));
    REQUIRE(fake_tx_data[0] == 0x30U);
    REQUIRE(fake_tx_data[1] == 0U);
    REQUIRE(fake_tx_data[2] == 0U);
    fake_tick = 11U;
    fake_tx_pending = 0U;
    link_stm32_bxcan_example_tx_complete_irq(
        &hcan, CAN_TX_MAILBOX1);
    link_stm32_bxcan_example_process();

    memset(data, 0, sizeof(data));
    data[0] = 0x21U;
    for (index = 0U; index < 7U; ++index) {
        data[index + 1U] = (uint8_t)vin[index + 3U];
    }
    queue_rx(&header, data);
    fake_tick = 12U;
    link_stm32_bxcan_example_rx_fifo0_irq(&hcan);
    link_stm32_bxcan_example_process();

    memset(data, 0, sizeof(data));
    data[0] = 0x22U;
    for (index = 0U; index < 7U; ++index) {
        data[index + 1U] = (uint8_t)vin[index + 10U];
    }
    queue_rx(&header, data);
    fake_tick = 13U;
    link_stm32_bxcan_example_rx_fifo0_irq(&hcan);
    link_stm32_bxcan_example_process();

    REQUIRE(link_stm32_bxcan_example_state() ==
        LINK_STM32_BXCAN_EXAMPLE_VIN_READY);
    REQUIRE(link_stm32_bxcan_example_vin() != NULL);
    REQUIRE(strcmp(link_stm32_bxcan_example_vin(), vin) == 0);
    REQUIRE(link_stm32_bxcan_example_negative_response_code() == 0U);
    REQUIRE(link_stm32_bxcan_example_dropped_frames() == 0U);
    return 0;
}

int main(void)
{
    REQUIRE(test_exact_standard_filters() == 0);
    REQUIRE(test_classic_tx_completion() == 0);
    REQUIRE(test_extended_rx_mapping() == 0);
    REQUIRE(test_failures_and_rejections() == 0);
    REQUIRE(test_vin_example_end_to_end() == 0);
    return 0;
}
