// SPDX-License-Identifier: GPL-3.0-or-later
/** @file link-stm32-bxcan-example.c @brief bxCAN UDS tester example. */
#include "link-stm32-bxcan-example.h"

#include "link-stm32-bxcan-hal.h"
#include "link-stm32-uds.h"
#include "link/uds.h"

#include <stddef.h>
#include <string.h>

#define LINK_STM32_BXCAN_VIN_DID UINT16_C(0xf190)
#define LINK_STM32_BXCAN_VIN_LENGTH 17U

static LinkStm32BxCanHal example_hal;
static LinkStm32Can example_can;
static LinkStm32UdsClient example_uds;
static uint8_t example_rx_storage[64U];
static uint8_t example_tx_storage[16U];
static char example_vin[LINK_STM32_BXCAN_VIN_LENGTH + 1U];
static uint8_t example_nrc;
static LinkStm32BxCanExampleState example_state =
    LINK_STM32_BXCAN_EXAMPLE_IDLE;

static bool link_stm32_bxcan_begin_vin(void)
{
    uint8_t request[3U];
    size_t request_length = 0U;

    if (link_uds_build_read_did_request(
            LINK_STM32_BXCAN_VIN_DID,
            request,
            sizeof(request),
            &request_length) != LINK_UDS_RESULT_OK) {
        return false;
    }
    return link_stm32_uds_start(
        &example_uds,
        request,
        request_length) == LINK_STM32_UDS_RESULT_OK;
}

void link_stm32_bxcan_example_default_tester_config(
    LinkStm32BxCanTesterConfig *config)
{
    const LinkStm32BxCanTesterConfig defaults =
        LINK_STM32_BXCAN_TESTER_CONFIG_INIT;

    if (config != NULL) {
        *config = defaults;
    }
}

bool link_stm32_bxcan_example_init_tester(
    CAN_HandleTypeDef *hcan,
    const LinkStm32BxCanTesterConfig *tester_config)
{
    LinkStm32CanOps ops;
    LinkStm32UdsConfig config;

    if (hcan == NULL || tester_config == NULL ||
        tester_config->request_can_id > UINT32_C(0x7ff) ||
        tester_config->response_can_id > UINT32_C(0x7ff) ||
        tester_config->request_can_id == tester_config->response_can_id ||
        tester_config->filter_bank > 27U ||
        tester_config->slave_start_filter_bank > 27U) {
        return false;
    }

    memset(example_vin, 0, sizeof(example_vin));
    example_nrc = 0U;
    example_state = LINK_STM32_BXCAN_EXAMPLE_IDLE;

    link_stm32_bxcan_hal_init(
        &example_hal,
        hcan,
        tester_config->filter_bank,
        tester_config->slave_start_filter_bank);
    ops = link_stm32_bxcan_hal_ops(&example_hal);
    if (!link_stm32_can_init(&example_can, &ops) ||
        !link_stm32_bxcan_hal_start_standard(
            &example_hal, tester_config->response_can_id)) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return false;
    }

    memset(&config, 0, sizeof(config));
    config.address.tx_can_id = tester_config->request_can_id;
    config.address.rx_can_id = tester_config->response_can_id;
    config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
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
            sizeof(example_tx_storage))) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return false;
    }

    if (!tester_config->read_vin_on_init) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_READY;
        return true;
    }
    if (!link_stm32_bxcan_begin_vin()) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return false;
    }
    example_state = LINK_STM32_BXCAN_EXAMPLE_READING_VIN;
    return true;
}

bool link_stm32_bxcan_example_init(CAN_HandleTypeDef *hcan)
{
    LinkStm32BxCanTesterConfig config =
        LINK_STM32_BXCAN_TESTER_CONFIG_INIT;

    return link_stm32_bxcan_example_init_tester(hcan, &config);
}

bool link_stm32_bxcan_example_start_vin(void)
{
    if (example_state != LINK_STM32_BXCAN_EXAMPLE_READY &&
        example_state != LINK_STM32_BXCAN_EXAMPLE_VIN_READY &&
        example_state != LINK_STM32_BXCAN_EXAMPLE_NEGATIVE_RESPONSE) {
        return false;
    }

    memset(example_vin, 0, sizeof(example_vin));
    example_nrc = 0U;
    if (!link_stm32_bxcan_begin_vin()) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return false;
    }
    example_state = LINK_STM32_BXCAN_EXAMPLE_READING_VIN;
    return true;
}

void link_stm32_bxcan_example_process(void)
{
    LinkStm32UdsResult result;
    const LinkUdsResponse *response;

    if (example_state != LINK_STM32_BXCAN_EXAMPLE_READING_VIN) {
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
        example_state = LINK_STM32_BXCAN_EXAMPLE_NEGATIVE_RESPONSE;
        return;
    }
    if (result != LINK_STM32_UDS_RESULT_COMPLETE) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return;
    }

    response = link_stm32_uds_response(&example_uds);
    if (response == NULL || response->kind != LINK_UDS_RESPONSE_POSITIVE ||
        response->request_service != LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER ||
        response->data_length != LINK_STM32_BXCAN_VIN_LENGTH + 2U ||
        response->data[0] != 0xf1U || response->data[1] != 0x90U) {
        example_state = LINK_STM32_BXCAN_EXAMPLE_FAILED;
        return;
    }

    memcpy(example_vin, &response->data[2], LINK_STM32_BXCAN_VIN_LENGTH);
    example_vin[LINK_STM32_BXCAN_VIN_LENGTH] = '\0';
    example_state = LINK_STM32_BXCAN_EXAMPLE_VIN_READY;
}

void link_stm32_bxcan_example_rx_fifo0_irq(CAN_HandleTypeDef *hcan)
{
    if (hcan != NULL && hcan == example_hal.hcan) {
        link_stm32_can_rx_isr(&example_can);
    }
}

void link_stm32_bxcan_example_tx_complete_irq(
    CAN_HandleTypeDef *hcan,
    uint32_t mailbox)
{
    if (hcan != NULL && hcan == example_hal.hcan) {
        link_stm32_bxcan_hal_tx_complete_irq(&example_hal, mailbox);
    }
}

void link_stm32_bxcan_example_tx_abort_irq(
    CAN_HandleTypeDef *hcan,
    uint32_t mailbox)
{
    if (hcan != NULL && hcan == example_hal.hcan) {
        link_stm32_bxcan_hal_tx_abort_irq(&example_hal, mailbox);
    }
}

void link_stm32_bxcan_example_error_irq(CAN_HandleTypeDef *hcan)
{
    if (hcan != NULL && hcan == example_hal.hcan) {
        link_stm32_bxcan_hal_error_irq(&example_hal);
    }
}

LinkStm32BxCanExampleState link_stm32_bxcan_example_state(void)
{
    return example_state;
}

const char *link_stm32_bxcan_example_vin(void)
{
    return example_state == LINK_STM32_BXCAN_EXAMPLE_VIN_READY
        ? example_vin : NULL;
}

uint8_t link_stm32_bxcan_example_negative_response_code(void)
{
    return example_nrc;
}

uint32_t link_stm32_bxcan_example_dropped_frames(void)
{
    return link_stm32_can_rx_dropped(&example_can);
}
