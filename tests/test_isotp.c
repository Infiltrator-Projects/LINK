// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/isotp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Requirement failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static LinkIsoTpAddress request_address(void)
{
    LinkIsoTpAddress address;
    memset(&address, 0, sizeof(address));
    address.tx_can_id = 0x7e0U;
    address.rx_can_id = 0x7e8U;
    address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    return address;
}

static LinkIsoTpAddress response_address(void)
{
    LinkIsoTpAddress address = request_address();
    address.tx_can_id = 0x7e8U;
    address.rx_can_id = 0x7e0U;
    return address;
}

static int round_trip(
    const uint8_t *payload,
    size_t payload_length,
    bool can_fd,
    uint8_t data_length,
    bool expect_extended_first_frame)
{
    uint8_t *received = NULL;
    LinkIsoTpRx rx;
    LinkIsoTpRxConfig rx_config;
    LinkIsoTpTx tx;
    LinkIsoTpTxConfig tx_config;
    LinkIsoTpCanFrame frame;
    LinkIsoTpCanFrame flow_control;
    LinkIsoTpResult tx_result;
    LinkIsoTpResult rx_result;
    bool flow_control_ready = false;
    size_t received_length = 0U;
    uint64_t now_us = 1000U;
    int status = 1;

    received = (uint8_t *)malloc(payload_length);
    if (received == NULL) {
        fprintf(stderr, "allocation failed for %zu-byte test payload\n",
                payload_length);
        return 1;
    }

    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = response_address();
    rx_config.consecutive_timeout_us = 100000U;
    rx_config.can_fd = can_fd;
    rx_config.data_length = data_length;

    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = request_address();
    tx_config.flow_control_timeout_us = 100000U;
    tx_config.max_wait_frames = 2U;
    tx_config.can_fd = can_fd;
    tx_config.data_length = data_length;

    if (link_isotp_rx_init(&rx, &rx_config, received, payload_length) !=
            LINK_ISOTP_RESULT_OK ||
        link_isotp_tx_init(&tx, &tx_config, payload, payload_length) !=
            LINK_ISOTP_RESULT_OK) {
        fprintf(stderr, "round-trip init failed\n");
        goto cleanup;
    }

    tx_result = link_isotp_tx_start(&tx, now_us, &frame);
    if (frame.can_fd != can_fd) {
        fprintf(stderr, "wrong link-layer frame type\n");
        goto cleanup;
    }

    if (expect_extended_first_frame) {
        if (tx_result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL ||
            frame.data[0] != 0x10U ||
            frame.data[1] != 0x00U ||
            frame.data[2] != (uint8_t)(payload_length >> 24U) ||
            frame.data[3] != (uint8_t)(payload_length >> 16U) ||
            frame.data[4] != (uint8_t)(payload_length >> 8U) ||
            frame.data[5] != (uint8_t)payload_length) {
            fprintf(stderr, "extended first-frame encoding mismatch\n");
            goto cleanup;
        }
    }

    rx_result = link_isotp_rx_feed(
        &rx, &frame, now_us, &flow_control, &flow_control_ready);

    if (tx_result == LINK_ISOTP_RESULT_COMPLETE) {
        if (rx_result != LINK_ISOTP_RESULT_COMPLETE ||
            flow_control_ready) {
            fprintf(stderr, "single-frame receive mismatch\n");
            goto cleanup;
        }
    } else if (tx_result == LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
        if (rx_result != LINK_ISOTP_RESULT_OK ||
            !flow_control_ready ||
            flow_control.can_id != 0x7e8U ||
            flow_control.can_fd != can_fd ||
            link_isotp_tx_accept_flow_control(
                &tx, &flow_control, now_us) != LINK_ISOTP_RESULT_OK) {
            fprintf(stderr, "first-frame/flow-control mismatch\n");
            goto cleanup;
        }

        for (;;) {
            now_us++;
            tx_result = link_isotp_tx_next(&tx, now_us, &frame);
            if (tx_result != LINK_ISOTP_RESULT_OK &&
                tx_result != LINK_ISOTP_RESULT_COMPLETE) {
                fprintf(stderr, "consecutive transmit failed: %s\n",
                        link_isotp_result_name(tx_result));
                goto cleanup;
            }

            flow_control_ready = false;
            rx_result = link_isotp_rx_feed(
                &rx, &frame, now_us, &flow_control, &flow_control_ready);
            if (rx_result != LINK_ISOTP_RESULT_OK &&
                rx_result != LINK_ISOTP_RESULT_COMPLETE) {
                fprintf(stderr, "consecutive receive failed: %s\n",
                        link_isotp_result_name(rx_result));
                goto cleanup;
            }
            if (flow_control_ready) {
                if (link_isotp_tx_accept_flow_control(
                        &tx, &flow_control, now_us) != LINK_ISOTP_RESULT_OK) {
                    fprintf(stderr, "block flow-control failed\n");
                    goto cleanup;
                }
            }

            if (tx_result == LINK_ISOTP_RESULT_COMPLETE) {
                if (rx_result != LINK_ISOTP_RESULT_COMPLETE) {
                    fprintf(stderr, "receiver did not complete with sender\n");
                    goto cleanup;
                }
                break;
            }
        }
    } else {
        fprintf(stderr, "unexpected initial transmit result: %s\n",
                link_isotp_result_name(tx_result));
        goto cleanup;
    }

    if (link_isotp_rx_payload(&rx, &received_length) == NULL ||
        received_length != payload_length ||
        memcmp(received, payload, payload_length) != 0) {
        fprintf(stderr, "round-trip payload mismatch\n");
        goto cleanup;
    }

    status = 0;

cleanup:
    free(received);
    return status;
}

int main(void)
{
    uint32_t stmin_us = 0U;
    uint8_t rx_buffer[32];
    const uint8_t classic_payload[] = {0x22U, 0xf1U, 0x90U};
    uint8_t fd_eight[8];
    uint8_t fd_sixty_two[62];
    uint8_t fd_hundred[100];
    uint8_t fd_extended[5000];
    LinkIsoTpRx rx;
    LinkIsoTpRxConfig rx_config;
    LinkIsoTpTx tx;
    LinkIsoTpTxConfig tx_config;
    LinkIsoTpCanFrame frame;
    LinkIsoTpCanFrame flow_control;
    bool flow_control_ready = false;
    size_t length = 0U;
    size_t index;

    REQUIRE(link_isotp_can_data_length_is_valid(false, 0U));
    REQUIRE(link_isotp_can_data_length_is_valid(false, 8U));
    REQUIRE(!link_isotp_can_data_length_is_valid(false, 12U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 0U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 8U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 12U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 16U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 20U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 24U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 32U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 48U));
    REQUIRE(link_isotp_can_data_length_is_valid(true, 64U));
    REQUIRE(!link_isotp_can_data_length_is_valid(true, 63U));

    REQUIRE(link_isotp_stmin_to_us(5U, &stmin_us));
    REQUIRE(stmin_us == 5000U);
    REQUIRE(link_isotp_stmin_to_us(0xf3U, &stmin_us));
    REQUIRE(stmin_us == 300U);
    REQUIRE(!link_isotp_stmin_to_us(0x80U, &stmin_us));

    /* Historical Classical-CAN behavior remains unchanged with zero defaults. */
    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = request_address();
    rx_config.consecutive_timeout_us = 100000U;
    REQUIRE(link_isotp_rx_init(&rx, &rx_config, rx_buffer,
                               sizeof(rx_buffer)) == LINK_ISOTP_RESULT_OK);

    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7e8U;
    frame.length = 4U;
    frame.data[0] = 3U;
    memcpy(&frame.data[1], classic_payload, sizeof(classic_payload));
    REQUIRE(link_isotp_rx_feed(&rx, &frame, 0U, &flow_control,
                               &flow_control_ready) == LINK_ISOTP_RESULT_COMPLETE);
    REQUIRE(!flow_control_ready);
    REQUIRE(link_isotp_rx_payload(&rx, &length) != NULL);
    REQUIRE(length == sizeof(classic_payload));
    REQUIRE(memcmp(rx_buffer, classic_payload, sizeof(classic_payload)) == 0);

    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = request_address();
    tx_config.flow_control_timeout_us = 100000U;
    tx_config.max_wait_frames = 2U;
    REQUIRE(link_isotp_tx_init(&tx, &tx_config, classic_payload,
                               sizeof(classic_payload)) == LINK_ISOTP_RESULT_OK);
    REQUIRE(link_isotp_tx_start(&tx, 0U, &frame) == LINK_ISOTP_RESULT_COMPLETE);
    REQUIRE(frame.can_id == 0x7e0U);
    REQUIRE(!frame.can_fd);
    REQUIRE(frame.length == 4U);
    REQUIRE(frame.data[0] == 3U);
    REQUIRE(memcmp(&frame.data[1], classic_payload, sizeof(classic_payload)) == 0);

    /* Optional ISO-TP padding expands short CAN frames to 8 bytes using 0xCC. */
    tx_config.pad_short_frames = true;
    tx_config.padding_byte = 0xccU;
    REQUIRE(link_isotp_tx_init(&tx, &tx_config, classic_payload,
                               sizeof(classic_payload)) == LINK_ISOTP_RESULT_OK);
    REQUIRE(link_isotp_tx_start(&tx, 0U, &frame) == LINK_ISOTP_RESULT_COMPLETE);
    REQUIRE(frame.length == 8U);
    REQUIRE(frame.data[0] == 3U);
    REQUIRE(memcmp(&frame.data[1], classic_payload, sizeof(classic_payload)) == 0);
    REQUIRE(frame.data[4] == 0xccU);
    REQUIRE(frame.data[5] == 0xccU);
    REQUIRE(frame.data[6] == 0xccU);
    REQUIRE(frame.data[7] == 0xccU);
    tx_config.pad_short_frames = false;

    for (index = 0U; index < sizeof(fd_eight); ++index) {
        fd_eight[index] = (uint8_t)(0x80U + index);
    }
    for (index = 0U; index < sizeof(fd_sixty_two); ++index) {
        fd_sixty_two[index] = (uint8_t)(index ^ 0x5aU);
    }
    for (index = 0U; index < sizeof(fd_hundred); ++index) {
        fd_hundred[index] = (uint8_t)(index * 3U);
    }
    for (index = 0U; index < sizeof(fd_extended); ++index) {
        fd_extended[index] = (uint8_t)(index * 17U + 3U);
    }

    /* CAN FD uses the escaped single-frame length above the short-SF limit. */
    REQUIRE(round_trip(fd_eight, sizeof(fd_eight), true, 64U, false) == 0);
    memset(&tx_config, 0, sizeof(tx_config));
    tx_config.address = request_address();
    tx_config.flow_control_timeout_us = 100000U;
    tx_config.can_fd = true;
    tx_config.data_length = 64U;
    REQUIRE(link_isotp_tx_init(&tx, &tx_config, fd_eight,
                               sizeof(fd_eight)) == LINK_ISOTP_RESULT_OK);
    REQUIRE(link_isotp_tx_start(&tx, 0U, &frame) == LINK_ISOTP_RESULT_COMPLETE);
    REQUIRE(frame.can_fd);
    REQUIRE(frame.length == 12U);
    REQUIRE(frame.data[0] == 0x00U);
    REQUIRE(frame.data[1] == sizeof(fd_eight));

    /* A normal-addressed 64-byte CAN-FD SF carries up to 62 PDU bytes. */
    REQUIRE(round_trip(fd_sixty_two, sizeof(fd_sixty_two),
                       true, 64U, false) == 0);
    REQUIRE(link_isotp_tx_init(&tx, &tx_config, fd_sixty_two,
                               sizeof(fd_sixty_two)) == LINK_ISOTP_RESULT_OK);
    REQUIRE(link_isotp_tx_start(&tx, 0U, &frame) == LINK_ISOTP_RESULT_COMPLETE);
    REQUIRE(frame.length == 64U);
    REQUIRE(frame.data[0] == 0x00U);
    REQUIRE(frame.data[1] == 62U);

    /* CAN-FD multi-frame transport uses 64-byte FF/CF capacity. */
    REQUIRE(round_trip(fd_hundred, sizeof(fd_hundred),
                       true, 64U, false) == 0);

    /* FF_DL escape 0x1000 + 32-bit length supports PDUs above 4095 bytes. */
    REQUIRE(round_trip(fd_extended, sizeof(fd_extended),
                       true, 64U, true) == 0);

    /* The escaped SF length is CAN-FD-only; Classical CAN rejects it. */
    memset(&rx_config, 0, sizeof(rx_config));
    rx_config.address = request_address();
    rx_config.consecutive_timeout_us = 100000U;
    REQUIRE(link_isotp_rx_init(&rx, &rx_config, rx_buffer,
                               sizeof(rx_buffer)) == LINK_ISOTP_RESULT_OK);
    memset(&frame, 0, sizeof(frame));
    frame.can_id = 0x7e8U;
    frame.length = 8U;
    frame.data[0] = 0x00U;
    frame.data[1] = 7U;
    REQUIRE(link_isotp_rx_feed(&rx, &frame, 0U, &flow_control,
                               &flow_control_ready) == LINK_ISOTP_RESULT_INVALID_FRAME);

    return 0;
}
