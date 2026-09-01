// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32-uds-server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do {     if (!(c)) {         fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c);         return 1;     } } while (0)

#define MOCK_MAX_FRAMES 16U

typedef struct {
    LinkIsoTpCanFrame rx[MOCK_MAX_FRAMES];
    size_t rx_head;
    size_t rx_tail;
    LinkIsoTpCanFrame tx[MOCK_MAX_FRAMES];
    size_t tx_count;
    uint32_t tick_ms;
    bool hw_pending;
} MockCan;

static bool mock_receive(void *context, LinkIsoTpCanFrame *frame)
{
    MockCan *mock = (MockCan *)context;
    if (mock->rx_tail == mock->rx_head) return false;
    *frame = mock->rx[mock->rx_tail++ % MOCK_MAX_FRAMES];
    return true;
}

static bool mock_tx_ready(void *context)
{
    MockCan *mock = (MockCan *)context;
    return !mock->hw_pending && mock->tx_count < MOCK_MAX_FRAMES;
}

static bool mock_send(void *context, const LinkIsoTpCanFrame *frame)
{
    MockCan *mock = (MockCan *)context;
    if (mock->hw_pending || mock->tx_count >= MOCK_MAX_FRAMES) return false;
    mock->tx[mock->tx_count++] = *frame;
    mock->hw_pending = true;
    return true;
}

static LinkStm32CanTxStatus mock_tx_status(
    void *context,
    uint32_t *completion_tick_ms)
{
    MockCan *mock = (MockCan *)context;
    if (!mock->hw_pending) return LINK_STM32_CAN_TX_IDLE;
    mock->hw_pending = false;
    if (completion_tick_ms != NULL) *completion_tick_ms = mock->tick_ms;
    return LINK_STM32_CAN_TX_COMPLETE;
}

static uint32_t mock_clock_ms(void *context)
{
    return ((MockCan *)context)->tick_ms;
}

static void mock_push(MockCan *mock, const LinkIsoTpCanFrame *frame)
{
    mock->rx[mock->rx_head++ % MOCK_MAX_FRAMES] = *frame;
}

static int poll_until_complete(
    LinkStm32UdsServer *transport,
    MockCan *mock)
{
    unsigned int index;
    for (index = 0U; index < 16U; ++index) {
        LinkStm32UdsServerResult result =
            link_stm32_uds_server_poll(transport);
        if (result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE) {
            return 0;
        }
        if (result != LINK_STM32_UDS_SERVER_RESULT_OK &&
            result != LINK_STM32_UDS_SERVER_RESULT_WAITING) {
            return 1;
        }
        mock->tick_ms++;
    }
    return 1;
}

int main(void)
{
    MockCan mock;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkUdsServer uds;
    LinkUdsServerConfig uds_config = LINK_UDS_SERVER_CONFIG_INIT;
    LinkUdsServerDtcStore dtc_store = LINK_UDS_SERVER_DTC_STORE_INIT;
    LinkStm32UdsServer transport;
    LinkStm32UdsServerConfig transport_config;
    uint8_t rx_storage[256U];
    uint8_t tx_storage[256U];
    LinkIsoTpCanFrame request;
    const uint8_t expected_session[] = {
        0x06U, 0x50U, 0x01U, 0x00U, 0x32U, 0x01U, 0xf4U
    };
    const uint8_t expected_dtc[] = {
        0x03U, 0x59U, 0x02U, 0xffU
    };

    memset(&mock, 0, sizeof(mock));
    memset(&ops, 0, sizeof(ops));
    ops.context = &mock;
    ops.receive = mock_receive;
    ops.tx_ready = mock_tx_ready;
    ops.send = mock_send;
    ops.tx_status = mock_tx_status;
    ops.clock_ms = mock_clock_ms;

    CHECK(link_stm32_can_init(&channel, &ops));
    CHECK(link_uds_server_init(&uds, &uds_config));
    CHECK(link_uds_server_set_handler(
        &uds, 0x19U, link_uds_server_dtc_handler, &dtc_store));

    memset(&transport_config, 0, sizeof(transport_config));
    transport_config.address.tx_can_id = 0x7e8U;
    transport_config.address.rx_can_id = 0x7e0U;
    transport_config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    transport_config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    transport_config.consecutive_timeout_us = UINT64_C(1000000);
    transport_config.flow_control_timeout_us = UINT64_C(1000000);
    transport_config.max_wait_frames = 3U;
    transport_config.can_fd = false;
    transport_config.data_length = 8U;

    CHECK(link_stm32_uds_server_init(
        &transport, &channel, &uds, &transport_config,
        rx_storage, sizeof(rx_storage),
        tx_storage, sizeof(tx_storage)));

    memset(&request, 0, sizeof(request));
    request.can_id = 0x7e0U;
    request.length = 3U;
    request.data[0] = 0x02U;
    request.data[1] = 0x10U;
    request.data[2] = 0x01U;
    mock_push(&mock, &request);
    link_stm32_can_rx_isr(&channel);

    CHECK(poll_until_complete(&transport, &mock) == 0);
    CHECK(mock.tx_count == 1U);
    CHECK(mock.tx[0].can_id == 0x7e8U);
    CHECK(mock.tx[0].length == sizeof(expected_session));
    CHECK(memcmp(
        mock.tx[0].data, expected_session, sizeof(expected_session)) == 0);

    memset(&request, 0, sizeof(request));
    request.can_id = 0x7e0U;
    request.length = 4U;
    request.data[0] = 0x03U;
    request.data[1] = 0x19U;
    request.data[2] = 0x02U;
    request.data[3] = 0xffU;
    mock_push(&mock, &request);
    link_stm32_can_rx_isr(&channel);

    CHECK(poll_until_complete(&transport, &mock) == 0);
    CHECK(mock.tx_count == 2U);
    CHECK(mock.tx[1].can_id == 0x7e8U);
    CHECK(mock.tx[1].length == sizeof(expected_dtc));
    CHECK(memcmp(
        mock.tx[1].data, expected_dtc, sizeof(expected_dtc)) == 0);

    memset(&request, 0, sizeof(request));
    request.can_id = 0x7e8U;
    request.length = 3U;
    request.data[0] = 0x02U;
    request.data[1] = 0x10U;
    request.data[2] = 0x01U;
    mock_push(&mock, &request);
    link_stm32_can_rx_isr(&channel);

    CHECK(link_stm32_uds_server_poll(&transport) ==
          LINK_STM32_UDS_SERVER_RESULT_WAITING);
    CHECK(mock.tx_count == 2U);

    puts("stm32 uds server tests passed");
    return EXIT_SUCCESS;
}
