// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32c092-server-example.c
 * @brief STM32C092 ECU/server example for LINK UDS over FDCAN.
 */
#include "link-stm32c092-server-example.h"

#include "link-stm32c092-hal.h"
#include "link-stm32-uds-server.h"

#include <stddef.h>
#include <string.h>

#define LINK_STM32C092_SERVER_VIN_DID UINT16_C(0xf190)

static const uint8_t example_vin[17U] = {
    'L','I','N','K','S','T','M','3','2','C','0','9','2','0','0','1','A'
};

static LinkStm32C092Hal example_hal;
static LinkStm32Can example_can;
static LinkUdsServer example_server;
static LinkStm32UdsServer example_transport;
static bool example_reset_pending;
static uint32_t example_reset_requested_ms;
static uint8_t example_reset_type;
static uint8_t example_rx_storage[512U];
static uint8_t example_tx_storage[512U];
static const LinkUdsDtcRecord example_dtc_records[] = {
    {
        UINT32_C(0x123456),
        LINK_UDS_DTC_STATUS_TEST_FAILED |
            LINK_UDS_DTC_STATUS_CONFIRMED_DTC
    },
    {
        UINT32_C(0xabcdef),
        LINK_UDS_DTC_STATUS_CONFIRMED_DTC
    }
};
static const LinkUdsServerDtcStore example_dtc_store = {
    example_dtc_records,
    sizeof(example_dtc_records) / sizeof(example_dtc_records[0]),
    LINK_UDS_DTC_STATUS_MASK_ALL,
    UINT8_C(0xff),
    UINT8_C(0x01)
};

static uint32_t link_stm32c092_server_clock_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static LinkUdsServerHandlerResult link_stm32c092_server_read_did(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *response_data,
    size_t response_data_capacity)
{
    uint16_t did;
    (void)context;

    if (request == NULL || request->pdu == NULL ||
        request->pdu_length != 3U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    did = (uint16_t)(((uint16_t)request->pdu[1] << 8U) | request->pdu[2]);
    if (did != LINK_STM32C092_SERVER_VIN_DID) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
    }
    if (response_data_capacity < sizeof(example_vin) + 2U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response_data[0] = (uint8_t)(did >> 8U);
    response_data[1] = (uint8_t)did;
    memcpy(response_data + 2U, example_vin, sizeof(example_vin));
    return link_uds_server_handler_positive(sizeof(example_vin) + 2U);
}

bool link_stm32c092_server_example_init(
    FDCAN_HandleTypeDef *hfdcan,
    const LinkStm32C092ServerConfig *server_config)
{
    LinkStm32CanOps ops;
    LinkUdsServerConfig uds_config = LINK_UDS_SERVER_CONFIG_INIT;
    LinkStm32UdsServerConfig transport_config;

    if (hfdcan == NULL || server_config == NULL ||
        server_config->request_can_id > UINT32_C(0x7ff) ||
        server_config->response_can_id > UINT32_C(0x7ff) ||
        server_config->request_can_id == server_config->response_can_id ||
        !link_isotp_can_data_length_is_valid(
            server_config->can_fd, server_config->data_length)) {
        return false;
    }

    uds_config.enforce_session_sequence = true;
    uds_config.s3_server_timeout_ms = UINT32_C(5000);
    uds_config.clock_ms = link_stm32c092_server_clock_ms;
    uds_config.clock_context = NULL;

    example_reset_pending = false;
    example_reset_requested_ms = 0U;
    example_reset_type = 0U;

    link_stm32c092_hal_init(&example_hal, hfdcan, server_config->can_fd);
    ops = link_stm32c092_hal_ops(&example_hal);
    if (!link_stm32_can_init(&example_can, &ops) ||
        !link_stm32c092_hal_start_standard(
            &example_hal, server_config->request_can_id) ||
        !link_uds_server_init(&example_server, &uds_config) ||
        !link_uds_server_set_handler(
            &example_server, LINK_UDS_SERVICE_READ_DTC_INFORMATION,
            link_uds_server_dtc_handler, (void *)&example_dtc_store) ||
        !link_uds_server_set_handler(
            &example_server, LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER,
            link_stm32c092_server_read_did, NULL)) {
        return false;
    }

    memset(&transport_config, 0, sizeof(transport_config));
    transport_config.address.tx_can_id = server_config->response_can_id;
    transport_config.address.rx_can_id = server_config->request_can_id;
    transport_config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    transport_config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    transport_config.rx_block_size = 0U;
    transport_config.rx_stmin = 0U;
    transport_config.consecutive_timeout_us = UINT64_C(1000000);
    transport_config.flow_control_timeout_us = UINT64_C(1000000);
    transport_config.max_wait_frames = 3U;
    transport_config.can_fd = server_config->can_fd;
    transport_config.data_length = server_config->data_length;
    transport_config.pad_short_frames = true;
    transport_config.padding_byte = UINT8_C(0xcc);

    return link_stm32_uds_server_init(
        &example_transport, &example_can, &example_server,
        &transport_config,
        example_rx_storage, sizeof(example_rx_storage),
        example_tx_storage, sizeof(example_tx_storage));
}

void link_stm32c092_server_example_process(void)
{
    LinkStm32UdsServerResult result;
    uint8_t reset_type = 0U;

    link_uds_server_tick(&example_server);
    result = link_stm32_uds_server_poll(&example_transport);

    if (result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE &&
        link_uds_server_take_pending_ecu_reset(
            &example_server, &reset_type)) {
        /*
         * Demonstration policy: all accepted 0x11 reset types map to the MCU
         * reset after the positive response has completed on CAN. Products
         * needing different power-management semantics can override 0x11.
         */
        example_reset_type = reset_type;
        example_reset_requested_ms = HAL_GetTick();
        example_reset_pending = true;
    }

}

bool link_stm32c092_server_example_take_reset(uint8_t *reset_type)
{
    if (reset_type == NULL || !example_reset_pending ||
        (uint32_t)(HAL_GetTick() - example_reset_requested_ms) <
            UINT32_C(50)) {
        return false;
    }

    *reset_type = example_reset_type;
    example_reset_pending = false;
    example_reset_type = 0U;
    return true;
}

void link_stm32c092_server_example_poll_rx(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan != NULL && hfdcan == example_hal.hfdcan) {
        /*
         * This deliberately uses the same bounded queue path as the ISR.
         * link_stm32_can_rx_isr() drains HAL FIFO0 through the adapter ops,
         * and is safe to call from the main loop when no interrupt arrived.
         */
        link_stm32_can_rx_isr(&example_can);
    }
}

void link_stm32c092_server_example_rx_fifo0_irq(FDCAN_HandleTypeDef *hfdcan)
{
    if (hfdcan != NULL && hfdcan == example_hal.hfdcan) {
        link_stm32_can_rx_isr(&example_can);
    }
}

void link_stm32c092_server_example_tx_event_irq(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t tx_event_fifo_its)
{
    if (hfdcan != NULL && hfdcan == example_hal.hfdcan) {
        link_stm32c092_hal_tx_event_irq(&example_hal, tx_event_fifo_its);
    }
}

bool link_stm32c092_server_example_set_handler(
    uint8_t service,
    LinkUdsServerHandlerFn handler,
    void *context)
{
    return link_uds_server_set_handler(
        &example_server, service, handler, context);
}

uint8_t link_stm32c092_server_example_session(void)
{
    return link_uds_server_active_session(&example_server);
}

uint8_t link_stm32c092_server_example_last_nrc(void)
{
    return link_uds_server_last_negative_response_code(&example_server);
}

uint32_t link_stm32c092_server_example_completed_requests(void)
{
    return link_stm32_uds_server_completed_requests(&example_transport);
}

uint32_t link_stm32c092_server_example_dropped_frames(void)
{
    return link_stm32_can_rx_dropped(&example_can);
}
