// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32c092-example.c @brief STM32C092 LINK UDS VIN example. */
#include "link-stm32c092-example.h"

#include "link-stm32c092-hal.h"
#include "link-stm32-uds.h"
#include "link/uds.h"

#include <stddef.h>
#include <string.h>

#define LINK_STM32C092_REQUEST_ID 0x7e0U
#define LINK_STM32C092_RESPONSE_ID 0x7e8U
#define LINK_STM32C092_VIN_DID 0xf190U
#define LINK_STM32C092_VIN_LENGTH 17U

static LinkStm32C092Hal example_hal;
static LinkStm32Can example_can;
static LinkStm32UdsClient example_uds;
static uint8_t example_rx_storage[256U];
static uint8_t example_tx_storage[64U];
static char example_vin[LINK_STM32C092_VIN_LENGTH + 1U];
static uint8_t example_nrc;
static LinkStm32C092ExampleState example_state = LINK_STM32C092_EXAMPLE_IDLE;

static bool link_stm32c092_begin_vin(void)
{
    uint8_t request[3U];
    size_t request_length = 0U;

    if (link_uds_build_read_did_request(
            LINK_STM32C092_VIN_DID,
            request,
            sizeof(request),
            &request_length) != LINK_UDS_RESULT_OK) {
        return false;
    }

    return link_stm32_uds_start(
        &example_uds, request, request_length) == LINK_STM32_UDS_RESULT_OK;
}

bool link_stm32c092_example_init(FDCAN_HandleTypeDef *hfdcan)
{
    LinkStm32CanOps ops;
    LinkStm32UdsConfig config;

    if (hfdcan == NULL) {
        return false;
    }

    memset(example_vin, 0, sizeof(example_vin));
    example_nrc = 0U;
    example_state = LINK_STM32C092_EXAMPLE_IDLE;

    link_stm32c092_hal_init(&example_hal, hfdcan, false);
    ops = link_stm32c092_hal_ops(&example_hal);
    if (!link_stm32_can_init(&example_can, &ops) ||
        !link_stm32c092_hal_start_standard(
            &example_hal, LINK_STM32C092_RESPONSE_ID)) {
        example_state = LINK_STM32C092_EXAMPLE_FAILED;
        return false;
    }

    memset(&config, 0, sizeof(config));
    config.address.tx_can_id = LINK_STM32C092_REQUEST_ID;
    config.address.rx_can_id = LINK_STM32C092_RESPONSE_ID;
    config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    config.rx_block_size = 0U;
    config.rx_stmin = 0U;
    config.consecutive_timeout_us = UINT64_C(1000000);
    config.flow_control_timeout_us = UINT64_C(1000000);
    config.max_wait_frames = 3U;
    config.p2_timeout_us = UINT64_C(100000);
    config.p2_star_timeout_us = UINT64_C(5000000);
    config.can_fd = false;
    config.data_length = LINK_ISOTP_CLASSIC_CAN_DATA_LENGTH;

    if (!link_stm32_uds_init(
            &example_uds,
            &example_can,
            &config,
            example_rx_storage,
            sizeof(example_rx_storage),
            example_tx_storage,
            sizeof(example_tx_storage)) ||
        !link_stm32c092_begin_vin()) {
        example_state = LINK_STM32C092_EXAMPLE_FAILED;
        return false;
    }

    example_state = LINK_STM32C092_EXAMPLE_READING_VIN;
    return true;
}

void link_stm32c092_example_process(void)
{
    LinkStm32UdsResult result;
    const LinkUdsResponse *response;

    if (example_state != LINK_STM32C092_EXAMPLE_READING_VIN) {
        return;
    }

    result = link_stm32_uds_poll(&example_uds);
    if (result == LINK_STM32_UDS_RESULT_WAITING ||
        result == LINK_STM32_UDS_RESULT_OK) {
        return;
    }
    if (result == LINK_STM32_UDS_RESULT_NEGATIVE_RESPONSE) {
        response = link_stm32_uds_response(&example_uds);
        if (response != NULL) {
            example_nrc = response->negative_response_code;
        }
        example_state = LINK_STM32C092_EXAMPLE_NEGATIVE_RESPONSE;
        return;
    }
    if (result != LINK_STM32_UDS_RESULT_COMPLETE) {
        example_state = LINK_STM32C092_EXAMPLE_FAILED;
        return;
    }

    response = link_stm32_uds_response(&example_uds);
    if (response == NULL || response->kind != LINK_UDS_RESPONSE_POSITIVE ||
        response->request_service != 0x22U ||
        response->data_length != LINK_STM32C092_VIN_LENGTH + 2U ||
        response->data[0] != 0xf1U || response->data[1] != 0x90U) {
        example_state = LINK_STM32C092_EXAMPLE_FAILED;
        return;
    }

    memcpy(example_vin, &response->data[2], LINK_STM32C092_VIN_LENGTH);
    example_vin[LINK_STM32C092_VIN_LENGTH] = '\0';
    example_state = LINK_STM32C092_EXAMPLE_VIN_READY;
}

void link_stm32c092_example_rx_fifo0_irq(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan != NULL && hfdcan == example_hal.hfdcan) {
        link_stm32_can_rx_isr(&example_can);
    }
}

LinkStm32C092ExampleState link_stm32c092_example_state(void)
{
    return example_state;
}

const char *link_stm32c092_example_vin(void)
{
    return example_state == LINK_STM32C092_EXAMPLE_VIN_READY
        ? example_vin : NULL;
}

uint8_t link_stm32c092_example_negative_response_code(void)
{
    return example_nrc;
}

uint32_t link_stm32c092_example_dropped_frames(void)
{
    return link_stm32_can_rx_dropped(&example_can);
}
