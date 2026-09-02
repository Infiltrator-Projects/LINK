// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32-can.h"
#include "link-stm32c092-hal.h"

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

static uint32_t fake_tick;
static uint32_t fake_notifications;
static bool fake_filter_configured;
static FDCAN_FilterTypeDef fake_filter;
static uint32_t fake_tx_free = 3U;
static bool fake_rx_available;
static FDCAN_RxHeaderTypeDef fake_rx_header;
static uint8_t fake_rx_data[64U];
static FDCAN_TxHeaderTypeDef fake_tx_header;
static uint8_t fake_tx_data[64U];
static bool fake_tx_event_available;
static uint32_t fake_tx_event_get_calls;
static FDCAN_TxEventFifoTypeDef fake_tx_event;

static void reset_fake_hal(void)
{
    fake_tick = 0U;
    fake_notifications = 0U;
    fake_filter_configured = false;
    memset(&fake_filter, 0, sizeof(fake_filter));
    fake_tx_free = 3U;
    fake_rx_available = false;
    memset(&fake_rx_header, 0, sizeof(fake_rx_header));
    memset(fake_rx_data, 0, sizeof(fake_rx_data));
    memset(&fake_tx_header, 0, sizeof(fake_tx_header));
    memset(fake_tx_data, 0, sizeof(fake_tx_data));
    fake_tx_event_available = false;
    fake_tx_event_get_calls = 0U;
    memset(&fake_tx_event, 0, sizeof(fake_tx_event));
}

uint32_t HAL_GetTick(void) { return fake_tick; }

uint32_t HAL_FDCAN_GetRxFifoFillLevel(
    FDCAN_HandleTypeDef *hfdcan, uint32_t rx_fifo)
{
    (void)hfdcan; (void)rx_fifo;
    return fake_rx_available ? 1U : 0U;
}

HAL_StatusTypeDef HAL_FDCAN_GetRxMessage(
    FDCAN_HandleTypeDef *hfdcan, uint32_t rx_location,
    FDCAN_RxHeaderTypeDef *header, uint8_t *data)
{
    (void)hfdcan; (void)rx_location;
    if (!fake_rx_available || header == NULL || data == NULL) return HAL_ERROR;
    *header = fake_rx_header;
    memcpy(data, fake_rx_data, sizeof(fake_rx_data));
    fake_rx_available = false;
    return HAL_OK;
}

uint32_t HAL_FDCAN_GetTxFifoFreeLevel(FDCAN_HandleTypeDef *hfdcan)
{
    (void)hfdcan;
    return fake_tx_free;
}

HAL_StatusTypeDef HAL_FDCAN_AddMessageToTxFifoQ(
    FDCAN_HandleTypeDef *hfdcan,
    const FDCAN_TxHeaderTypeDef *header,
    const uint8_t *data)
{
    (void)hfdcan;
    if (header == NULL || data == NULL || fake_tx_free == 0U) return HAL_ERROR;
    fake_tx_header = *header;
    memcpy(fake_tx_data, data, sizeof(fake_tx_data));
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_GetTxEvent(
    FDCAN_HandleTypeDef *hfdcan,
    FDCAN_TxEventFifoTypeDef *event)
{
    (void)hfdcan;
    ++fake_tx_event_get_calls;
    if (!fake_tx_event_available || event == NULL) return HAL_ERROR;
    *event = fake_tx_event;
    fake_tx_event_available = false;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_ConfigFilter(
    FDCAN_HandleTypeDef *hfdcan,
    const FDCAN_FilterTypeDef *filter)
{
    (void)hfdcan;
    if (filter == NULL) return HAL_ERROR;
    fake_filter = *filter;
    fake_filter_configured = true;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_ConfigGlobalFilter(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t non_matching_std, uint32_t non_matching_ext,
    uint32_t reject_remote_std, uint32_t reject_remote_ext)
{
    (void)hfdcan; (void)non_matching_std; (void)non_matching_ext;
    (void)reject_remote_std; (void)reject_remote_ext;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_ActivateNotification(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t active_it,
    uint32_t buffer_indexes)
{
    (void)hfdcan; (void)buffer_indexes;
    fake_notifications = active_it;
    return HAL_OK;
}

HAL_StatusTypeDef HAL_FDCAN_Start(FDCAN_HandleTypeDef *hfdcan)
{
    return hfdcan == NULL ? HAL_ERROR : HAL_OK;
}

static int test_classic_tx_and_completion(void)
{
    reset_fake_hal();
    FDCAN_HandleTypeDef hfdcan;
    LinkStm32C092Hal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;
    uint64_t completion_us = 0U;
    uint8_t marker;

    memset(&hfdcan, 0, sizeof(hfdcan));
    link_stm32c092_hal_init(&adapter, &hfdcan, false);
    ops = link_stm32c092_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));
    REQUIRE(link_stm32c092_hal_start_standard(&adapter, 0x7e8U));
    REQUIRE(fake_filter_configured);
    REQUIRE(fake_filter.FilterType == FDCAN_FILTER_MASK);
    REQUIRE(fake_filter.FilterID1 == 0x7e8U);
    REQUIRE(fake_filter.FilterID2 == 0x7ffU);
    REQUIRE((fake_notifications & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U);
    REQUIRE((fake_notifications & FDCAN_IT_TX_EVT_FIFO_NEW_DATA) != 0U);
    REQUIRE((fake_notifications & FDCAN_IT_TX_EVT_FIFO_ELT_LOST) != 0U);

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7e0U;
    frame.length = 4U;
    frame.data[0] = 3U;
    frame.data[1] = 0x22U;
    frame.data[2] = 0xf1U;
    frame.data[3] = 0x90U;
    REQUIRE(link_stm32_can_send(&channel, &frame));
    REQUIRE(fake_tx_header.DataLength == FDCAN_DLC_BYTES_4);
    REQUIRE(fake_tx_header.TxEventFifoControl == FDCAN_STORE_TX_EVENTS);
    marker = (uint8_t)fake_tx_header.MessageMarker;
    REQUIRE(marker != 0U);

    fake_tick = 7U;
    memset(&fake_tx_event, 0, sizeof(fake_tx_event));
    fake_tx_event.MessageMarker = marker;
    fake_tx_event.EventType = FDCAN_TX_EVENT;
    fake_tx_event_available = true;
    link_stm32c092_hal_tx_event_irq(
        &adapter, FDCAN_IT_TX_EVT_FIFO_NEW_DATA);
    REQUIRE(fake_tx_event_get_calls == 1U);
    fake_tick = 11U;
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &completion_us) == LINK_STM32_CAN_TX_COMPLETE);
    REQUIRE(completion_us == UINT64_C(7000));
    return 0;
}

static int test_dual_physical_functional_filter(void)
{
    FDCAN_HandleTypeDef hfdcan;
    LinkStm32C092Hal adapter;

    reset_fake_hal();
    memset(&hfdcan, 0, sizeof(hfdcan));
    link_stm32c092_hal_init(&adapter, &hfdcan, false);
    REQUIRE(link_stm32c092_hal_start_standard_dual(
        &adapter, UINT32_C(0x7e0), UINT32_C(0x7df)));
    REQUIRE(fake_filter_configured);
    REQUIRE(fake_filter.FilterType == FDCAN_FILTER_DUAL);
    REQUIRE(fake_filter.FilterID1 == UINT32_C(0x7e0));
    REQUIRE(fake_filter.FilterID2 == UINT32_C(0x7df));
    return 0;
}

static int test_can_fd_and_extended_id_mapping(void)
{
    reset_fake_hal();
    FDCAN_HandleTypeDef hfdcan;
    LinkStm32C092Hal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;

    memset(&hfdcan, 0, sizeof(hfdcan));
    link_stm32c092_hal_init(&adapter, &hfdcan, true);
    ops = link_stm32c092_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x18daf110U;
    frame.extended_id = true;
    frame.can_fd = true;
    frame.length = 12U;
    REQUIRE(link_stm32_can_send(&channel, &frame));
    REQUIRE(fake_tx_header.IdType == FDCAN_EXTENDED_ID);
    REQUIRE(fake_tx_header.FDFormat == FDCAN_FD_CAN);
    REQUIRE(fake_tx_header.BitRateSwitch == FDCAN_BRS_ON);
    REQUIRE(fake_tx_header.DataLength == FDCAN_DLC_BYTES_12);
    return 0;
}

static int test_rx_mapping_and_event_loss(void)
{
    reset_fake_hal();
    FDCAN_HandleTypeDef hfdcan;
    LinkStm32C092Hal adapter;
    LinkStm32CanOps ops;
    LinkStm32Can channel;
    LinkIsoTpCanFrame frame;
    uint64_t arrival_us = 0U;

    memset(&hfdcan, 0, sizeof(hfdcan));
    link_stm32c092_hal_init(&adapter, &hfdcan, false);
    ops = link_stm32c092_hal_ops(&adapter);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    fake_tick = 20U;
    memset(&fake_rx_header, 0, sizeof(fake_rx_header));
    fake_rx_header.Identifier = 0x7e8U;
    fake_rx_header.IdType = FDCAN_STANDARD_ID;
    fake_rx_header.RxFrameType = FDCAN_DATA_FRAME;
    fake_rx_header.DataLength = FDCAN_DLC_BYTES_8;
    fake_rx_header.FDFormat = FDCAN_CLASSIC_CAN;
    memset(fake_rx_data, 0, sizeof(fake_rx_data));
    fake_rx_data[0] = 3U;
    fake_rx_data[1] = 0x7fU;
    fake_rx_data[2] = 0x22U;
    fake_rx_data[3] = 0x31U;
    fake_rx_available = true;
    link_stm32_can_rx_isr(&channel);
    fake_tick = 25U;
    REQUIRE(link_stm32_can_pop_timed(&channel, &frame, &arrival_us));
    REQUIRE(arrival_us == UINT64_C(20000));
    REQUIRE(frame.can_id == 0x7e8U);
    REQUIRE(frame.length == 8U);

    frame.can_id = 0x7e0U;
    frame.length = 8U;
    REQUIRE(link_stm32_can_send(&channel, &frame));
    link_stm32c092_hal_tx_event_irq(
        &adapter, FDCAN_IT_TX_EVT_FIFO_ELT_LOST);
    REQUIRE(fake_tx_event_get_calls == 0U);
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &arrival_us) == LINK_STM32_CAN_TX_FAILED);
    return 0;
}

int main(void)
{
    REQUIRE(test_classic_tx_and_completion() == 0);
    REQUIRE(test_dual_physical_functional_filter() == 0);
    REQUIRE(test_can_fd_and_extended_id_mapping() == 0);
    REQUIRE(test_rx_mapping_and_event_loss() == 0);
    return 0;
}
