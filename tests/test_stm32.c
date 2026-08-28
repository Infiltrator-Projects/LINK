// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32-can.h"
#include "link-stm32-uds.h"

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

typedef struct {
    LinkIsoTpCanFrame rx[64U];
    size_t rx_count;
    size_t rx_index;
    LinkIsoTpCanFrame tx[64U];
    size_t tx_count;
    uint32_t tick_ms;
    bool tx_ready;
    LinkStm32CanTxStatus tx_status;
    uint32_t tx_complete_tick_ms;
} FakeCan;

static bool fake_receive(void *context, LinkIsoTpCanFrame *frame)
{
    FakeCan *fake = (FakeCan *)context;
    if (fake == NULL || frame == NULL || fake->rx_index >= fake->rx_count) {
        return false;
    }
    *frame = fake->rx[fake->rx_index++];
    return true;
}

static bool fake_tx_ready(void *context)
{
    const FakeCan *fake = (const FakeCan *)context;
    return fake != NULL && fake->tx_ready && fake->tx_count < 64U;
}

static bool fake_send(void *context, const LinkIsoTpCanFrame *frame)
{
    FakeCan *fake = (FakeCan *)context;
    if (fake == NULL || frame == NULL || !fake_tx_ready(fake)) return false;
    fake->tx[fake->tx_count++] = *frame;
    fake->tx_status = LINK_STM32_CAN_TX_PENDING;
    fake->tx_complete_tick_ms = 0U;
    return true;
}

static LinkStm32CanTxStatus fake_tx_status(
    void *context,
    uint32_t *completion_tick_ms)
{
    const FakeCan *fake = (const FakeCan *)context;
    if (completion_tick_ms != NULL) {
        *completion_tick_ms = fake == NULL ? 0U : fake->tx_complete_tick_ms;
    }
    return fake == NULL ? LINK_STM32_CAN_TX_FAILED : fake->tx_status;
}

static uint32_t fake_clock_ms(void *context)
{
    const FakeCan *fake = (const FakeCan *)context;
    return fake == NULL ? 0U : fake->tick_ms;
}

static LinkStm32CanOps fake_ops(FakeCan *fake)
{
    LinkStm32CanOps ops;
    memset(&ops, 0, sizeof(ops));
    ops.context = fake;
    ops.receive = fake_receive;
    ops.tx_ready = fake_tx_ready;
    ops.send = fake_send;
    ops.tx_status = fake_tx_status;
    ops.clock_ms = fake_clock_ms;
    return ops;
}

static void fake_push_rx(FakeCan *fake, const LinkIsoTpCanFrame *frame)
{
    if (fake != NULL && frame != NULL && fake->rx_count < 64U) {
        fake->rx[fake->rx_count++] = *frame;
    }
}

static void fake_complete_tx(FakeCan *fake)
{
    fake->tx_complete_tick_ms = fake->tick_ms;
    fake->tx_status = LINK_STM32_CAN_TX_COMPLETE;
}

static LinkIsoTpCanFrame classic_frame(uint32_t id)
{
    LinkIsoTpCanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.length = 8U;
    return frame;
}

static LinkStm32UdsConfig test_config(void)
{
    LinkStm32UdsConfig config;
    memset(&config, 0, sizeof(config));
    config.address.tx_can_id = 0x7e0U;
    config.address.rx_can_id = 0x7e8U;
    config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    config.consecutive_timeout_us = UINT64_C(1000000);
    config.flow_control_timeout_us = UINT64_C(1000000);
    config.max_wait_frames = 3U;
    config.p2_timeout_us = UINT64_C(100000);
    config.p2_star_timeout_us = UINT64_C(5000000);
    config.data_length = LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH;
    return config;
}

static int test_queue_clock_and_tx_completion(void)
{
    FakeCan fake;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkIsoTpCanFrame frame;
    uint64_t event_us = 0U;
    size_t index;

    memset(&fake, 0, sizeof(fake));
    fake.tx_ready = true;
    fake.tick_ms = UINT32_MAX - 1U;
    ops = fake_ops(&fake);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    fake.tick_ms = UINT32_MAX;
    REQUIRE(link_stm32_can_now_us(&channel) == UINT64_C(1000));
    frame = classic_frame(0x700U);
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    fake.tick_ms = 2U;
    REQUIRE(link_stm32_can_pop_timed(&channel, &frame, &event_us));
    REQUIRE(event_us == UINT64_C(1000));

    fake.rx_count = 0U;
    fake.rx_index = 0U;
    for (index = 0U; index < LINK_STM32_CAN_RX_QUEUE_CAPACITY + 2U; ++index) {
        frame = classic_frame(0x700U + (uint32_t)index);
        fake_push_rx(&fake, &frame);
    }
    link_stm32_can_rx_isr(&channel);
    REQUIRE(link_stm32_can_rx_dropped(&channel) == 2U);
    for (index = 0U; index < LINK_STM32_CAN_RX_QUEUE_CAPACITY; ++index) {
        REQUIRE(link_stm32_can_pop(&channel, &frame));
    }

    frame = classic_frame(0x7e0U);
    REQUIRE(link_stm32_can_send(&channel, &frame));
    REQUIRE(link_stm32_can_tx_in_flight(&channel));
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &event_us) == LINK_STM32_CAN_TX_PENDING);
    fake.tick_ms = 7U;
    fake_complete_tx(&fake);
    fake.tick_ms = 10U;
    REQUIRE(link_stm32_can_poll_tx_status(
        &channel, &event_us) == LINK_STM32_CAN_TX_COMPLETE);
    REQUIRE(event_us == UINT64_C(9000));
    REQUIRE(!link_stm32_can_tx_in_flight(&channel));
    return 0;
}

static int test_delayed_rx_and_multiframe_tx(void)
{
    FakeCan fake;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkStm32UdsClient client;
    LinkStm32UdsConfig config;
    uint8_t rx_storage[128U];
    uint8_t tx_storage[64U];
    uint8_t request[20U];
    LinkIsoTpCanFrame frame;
    size_t index;

    memset(&fake, 0, sizeof(fake));
    fake.tx_ready = true;
    ops = fake_ops(&fake);
    REQUIRE(link_stm32_can_init(&channel, &ops));
    config = test_config();
    REQUIRE(link_stm32_uds_init(
        &client, &channel, &config,
        rx_storage, sizeof(rx_storage),
        tx_storage, sizeof(tx_storage)));

    request[0] = 0x2eU;
    request[1] = 0xf1U;
    request[2] = 0x90U;
    for (index = 3U; index < sizeof(request); ++index) request[index] = (uint8_t)index;

    REQUIRE(link_stm32_uds_start(
        &client, request, sizeof(request)) == LINK_STM32_UDS_RESULT_OK);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 1U);

    fake.tick_ms = 900U;
    fake_complete_tx(&fake);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);

    fake.tick_ms = 1800U;
    frame = classic_frame(0x7e8U);
    frame.length = 3U;
    frame.data[0] = 0x30U;
    frame.data[1] = 0U;
    frame.data[2] = 5U;
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 2U);

    fake.tick_ms = 1810U;
    fake_complete_tx(&fake);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    fake.tick_ms = 1814U;
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 2U);
    fake.tick_ms = 1815U;
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 3U);

    fake.tick_ms = 1820U;
    fake_complete_tx(&fake);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);

    fake.tick_ms = 1830U;
    frame = classic_frame(0x7e8U);
    frame.length = 4U;
    frame.data[0] = 3U;
    frame.data[1] = 0x6eU;
    frame.data[2] = 0xf1U;
    frame.data[3] = 0x90U;
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_COMPLETE);

    link_stm32_uds_reset(&client);
    {
        const uint8_t read_did[3U] = {0x22U, 0xf1U, 0x90U};
        REQUIRE(link_stm32_uds_start(
            &client, read_did, sizeof(read_did)) == LINK_STM32_UDS_RESULT_OK);
        REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
        fake.tick_ms = 1840U;
        fake_complete_tx(&fake);
        REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);

        fake.tick_ms = 1890U;
        frame = classic_frame(0x7e8U);
        frame.length = 6U;
        frame.data[0] = 5U;
        frame.data[1] = 0x62U;
        frame.data[2] = 0xf1U;
        frame.data[3] = 0x90U;
        frame.data[4] = 0x41U;
        frame.data[5] = 0x42U;
        fake_push_rx(&fake, &frame);
        link_stm32_can_rx_isr(&channel);
        fake.tick_ms = 1990U;
        REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_COMPLETE);
    }
    return 0;
}

static int test_stuck_hardware_tx_is_bounded(void)
{
    FakeCan fake;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkStm32UdsClient client;
    LinkStm32UdsConfig config;
    uint8_t rx_storage[64U];
    uint8_t tx_storage[16U];
    const uint8_t request[3U] = {0x22U, 0xf1U, 0x90U};

    memset(&fake, 0, sizeof(fake));
    fake.tx_ready = true;
    ops = fake_ops(&fake);
    REQUIRE(link_stm32_can_init(&channel, &ops));
    config = test_config();
    REQUIRE(link_stm32_uds_init(
        &client, &channel, &config,
        rx_storage, sizeof(rx_storage),
        tx_storage, sizeof(tx_storage)));
    REQUIRE(link_stm32_uds_start(
        &client, request, sizeof(request)) == LINK_STM32_UDS_RESULT_OK);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);
    fake.tick_ms = 1001U;
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_CAN_IO);
    return 0;
}

int main(void)
{
    REQUIRE(test_queue_clock_and_tx_completion() == 0);
    REQUIRE(test_delayed_rx_and_multiframe_tx() == 0);
    REQUIRE(test_stuck_hardware_tx_is_bounded() == 0);
    return 0;
}
