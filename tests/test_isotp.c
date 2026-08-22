// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/isotp.h"

#include <assert.h>
#include <string.h>

static LinkIsoTpAddress normal_address(void)
{
    LinkIsoTpAddress address;
    memset(&address, 0, sizeof(address));
    address.tx_can_id = 0x7e0U;
    address.rx_can_id = 0x7e8U;
    address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    return address;
}

int main(void)
{
    uint32_t stmin_us = 0U;
    uint8_t rx_buffer[32];
    const uint8_t payload[] = {0x22U, 0xf1U, 0x90U};
    LinkIsoTpRx rx;
    LinkIsoTpRxConfig rx_config;
    LinkIsoTpTx tx;
    LinkIsoTpTxConfig tx_config;
    LinkIsoTpCanFrame frame;
    LinkIsoTpCanFrame flow_control;
    bool flow_control_ready = false;
    size_t length = 0U;

    assert(link_isotp_stmin_to_us(5U, &stmin_us));
    assert(stmin_us == 5000U);
    assert(link_isotp_stmin_to_us(0xf3U, &stmin_us));
    assert(stmin_us == 300U);
    assert(!link_isotp_stmin_to_us(0x80U, &stmin_us));

    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = normal_address();
    rx_config.consecutive_timeout_us = 100000U;
    assert(link_isotp_rx_init(&rx, &rx_config, rx_buffer,
                              sizeof(rx_buffer)) == LINK_ISOTP_RESULT_OK);

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7e8U;
    frame.length = 4U;
    frame.data[0] = 3U;
    memcpy(&frame.data[1], payload, sizeof(payload));
    assert(link_isotp_rx_feed(&rx, &frame, 0U, &flow_control,
                              &flow_control_ready) == LINK_ISOTP_RESULT_COMPLETE);
    assert(!flow_control_ready);
    assert(link_isotp_rx_payload(&rx, &length) != NULL);
    assert(length == sizeof(payload));
    assert(memcmp(rx_buffer, payload, sizeof(payload)) == 0);

    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = normal_address();
    tx_config.flow_control_timeout_us = 100000U;
    tx_config.max_wait_frames = 2U;
    assert(link_isotp_tx_init(&tx, &tx_config, payload,
                              sizeof(payload)) == LINK_ISOTP_RESULT_OK);
    assert(link_isotp_tx_start(&tx, 0U, &frame) == LINK_ISOTP_RESULT_COMPLETE);
    assert(frame.can_id == 0x7e0U);
    assert(frame.length == 4U);
    assert(frame.data[0] == 3U);
    assert(memcmp(&frame.data[1], payload, sizeof(payload)) == 0);

    return 0;
}
