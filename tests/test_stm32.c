// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32-can.h"
#include "link-stm32-uds.h"
#include "link/uds.h"

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
    LinkIsoTpCanFrame rx[32U];
    size_t rx_count;
    size_t rx_index;
    LinkIsoTpCanFrame tx[32U];
    size_t tx_count;
    uint32_t tick_ms;
    bool tx_ready;
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
    return fake != NULL && fake->tx_ready && fake->tx_count < 32U;
}

static bool fake_send(void *context, const LinkIsoTpCanFrame *frame)
{
    FakeCan *fake = (FakeCan *)context;
    if (fake == NULL || frame == NULL || !fake_tx_ready(fake)) {
        return false;
    }
    fake->tx[fake->tx_count++] = *frame;
    return true;
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
    ops.clock_ms = fake_clock_ms;
    return ops;
}

static void fake_push_rx(FakeCan *fake, const LinkIsoTpCanFrame *frame)
{
    if (fake != NULL && frame != NULL && fake->rx_count < 32U) {
        fake->rx[fake->rx_count++] = *frame;
    }
}

static LinkIsoTpCanFrame classic_frame(uint32_t id)
{
    LinkIsoTpCanFrame frame;
    memset(&frame, 0, sizeof(frame));
    frame.can_id = id;
    frame.length = 8U;
    return frame;
}

static int test_queue_and_clock(void)
{
    FakeCan fake;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkIsoTpCanFrame frame;
    size_t index;

    memset(&fake, 0, sizeof(fake));
    fake.tx_ready = true;
    ops = fake_ops(&fake);
    REQUIRE(link_stm32_can_init(&channel, &ops));

    fake.tick_ms = UINT32_MAX - 1U;
    REQUIRE(link_stm32_can_now_us(&channel) == 0U);
    fake.tick_ms = UINT32_MAX;
    REQUIRE(link_stm32_can_now_us(&channel) == UINT64_C(1000));
    fake.tick_ms = 0U;
    REQUIRE(link_stm32_can_now_us(&channel) == UINT64_C(2000));

    for (index = 0U; index < LINK_STM32_CAN_RX_QUEUE_CAPACITY + 2U; ++index) {
        frame = classic_frame(0x700U + (uint32_t)index);
        fake_push_rx(&fake, &frame);
    }
    link_stm32_can_rx_isr(&channel);
    REQUIRE(link_stm32_can_rx_dropped(&channel) == 2U);

    for (index = 0U; index < LINK_STM32_CAN_RX_QUEUE_CAPACITY; ++index) {
        REQUIRE(link_stm32_can_pop(&channel, &frame));
        REQUIRE(frame.can_id == 0x700U + (uint32_t)index);
    }
    REQUIRE(!link_stm32_can_pop(&channel, &frame));
    return 0;
}

static int test_uds_vin_round_trip(void)
{
    static const uint8_t vin[17U] = {
        'W','D','D','2','0','7','3','0','2','2','F','1','2','3','4','5','6'
    };
    FakeCan fake;
    LinkStm32Can channel;
    LinkStm32CanOps ops;
    LinkStm32UdsClient client;
    LinkStm32UdsConfig config;
    uint8_t rx_storage[128U];
    uint8_t tx_storage[64U];
    uint8_t request[3U];
    size_t request_length = 0U;
    LinkIsoTpCanFrame frame;
    LinkStm32UdsResult result;
    const LinkUdsResponse *response;

    memset(&fake, 0, sizeof(fake));
    fake.tx_ready = true;
    ops = fake_ops(&fake);
    REQUIRE(link_stm32_can_init(&channel, &ops));

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

    REQUIRE(link_stm32_uds_init(
        &client, &channel, &config,
        rx_storage, sizeof(rx_storage),
        tx_storage, sizeof(tx_storage)));
    REQUIRE(link_uds_build_read_did_request(
        0xf190U, request, sizeof(request), &request_length) == LINK_UDS_RESULT_OK);
    REQUIRE(request_length == 3U);
    REQUIRE(link_stm32_uds_start(
        &client, request, request_length) == LINK_STM32_UDS_RESULT_OK);

    result = link_stm32_uds_poll(&client);
    REQUIRE(result == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 1U);
    REQUIRE(fake.tx[0].can_id == 0x7e0U);
    REQUIRE(fake.tx[0].length == 4U);
    REQUIRE(fake.tx[0].data[0] == 3U);
    REQUIRE(fake.tx[0].data[1] == 0x22U);
    REQUIRE(fake.tx[0].data[2] == 0xf1U);
    REQUIRE(fake.tx[0].data[3] == 0x90U);

    frame = classic_frame(0x7e8U);
    frame.data[0] = 0x10U;
    frame.data[1] = 0x14U;
    frame.data[2] = 0x62U;
    frame.data[3] = 0xf1U;
    frame.data[4] = 0x90U;
    memcpy(&frame.data[5], &vin[0], 3U);
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    result = link_stm32_uds_poll(&client);
    REQUIRE(result == LINK_STM32_UDS_RESULT_WAITING);
    REQUIRE(fake.tx_count == 2U);
    REQUIRE(fake.tx[1].can_id == 0x7e0U);
    REQUIRE((fake.tx[1].data[0] & 0xf0U) == 0x30U);

    frame = classic_frame(0x7e8U);
    frame.data[0] = 0x21U;
    memcpy(&frame.data[1], &vin[3], 7U);
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    REQUIRE(link_stm32_uds_poll(&client) == LINK_STM32_UDS_RESULT_WAITING);

    frame = classic_frame(0x7e8U);
    frame.data[0] = 0x22U;
    memcpy(&frame.data[1], &vin[10], 7U);
    fake_push_rx(&fake, &frame);
    link_stm32_can_rx_isr(&channel);
    result = link_stm32_uds_poll(&client);
    REQUIRE(result == LINK_STM32_UDS_RESULT_COMPLETE);

    response = link_stm32_uds_response(&client);
    REQUIRE(response != NULL);
    REQUIRE(response->kind == LINK_UDS_RESPONSE_POSITIVE);
    REQUIRE(response->request_service == 0x22U);
    REQUIRE(response->data_length == 19U);
    REQUIRE(response->data[0] == 0xf1U && response->data[1] == 0x90U);
    REQUIRE(memcmp(&response->data[2], vin, sizeof(vin)) == 0);
    return 0;
}

int main(void)
{
    REQUIRE(test_queue_and_clock() == 0);
    REQUIRE(test_uds_vin_round_trip() == 0);
    return 0;
}
