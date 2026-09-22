// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * STM32F103Zx / 512 KiB Cube integration for LINK issue #32.
 *
 * Cube owns clock/GPIO/CAN initialization. Reserve the final two 2 KiB flash
 * pages in the linker script before using this example:
 *   page A 0x0807F000, page B 0x0807F800.
 *
 * CAN1 is expected on PA11/PA12 at 500 kbit/s using an external transceiver.
 */
#include "main.h"
#include "can.h"
#include "gpio.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"

#include "link-stm32-can.h"
#include "link-stm32-uds-server.h"
#include "link-stm32-bxcan-hal.h"
#include "link-stm32f103-uds-ecu.h"

#include <string.h>

extern CAN_HandleTypeDef hcan1;

static LinkStm32BxCanHal uds_hal;
static LinkStm32Can uds_can;
static LinkStm32F103UdsEcu uds_ecu;
static LinkStm32UdsServer uds_transport;
static uint8_t uds_rx[512U];
static uint8_t uds_tx[512U];
static bool reset_pending;
static uint32_t reset_requested_ms;

/*
 * DEMO ONLY. Replace with target-owned key material or a hardware-backed key.
 * LINK never assumes this RFC 4493 test key is appropriate for a vehicle.
 */
static const uint8_t demo_security_key[16U] = {
    0x2bU,0x7eU,0x15U,0x16U,0x28U,0xaeU,0xd2U,0xa6U,
    0xabU,0xf7U,0x15U,0x88U,0x09U,0xcfU,0x4fU,0x3cU
};

static bool reserved_page(uint32_t address)
{
    return address == LINK_STM32F103_UDS_STATE_PAGE_A ||
           address == LINK_STM32F103_UDS_STATE_PAGE_B;
}

static bool ecu_flash_read(
    void *context, uint32_t address, void *data, size_t length)
{
    (void)context;
    if (!reserved_page(address) || data == NULL ||
        length > LINK_STM32F103_FLASH_PAGE_BYTES) {
        return false;
    }
    memcpy(data, (const void *)(uintptr_t)address, length);
    return true;
}

static bool ecu_flash_erase(void *context, uint32_t address)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    (void)context;
    if (!reserved_page(address)) return false;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1U;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return status == HAL_OK;
}

static bool ecu_flash_program(
    void *context, uint32_t address, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t offset;

    (void)context;
    if (!reserved_page(address) || data == NULL ||
        length > LINK_STM32F103_FLASH_PAGE_BYTES) {
        return false;
    }

    HAL_FLASH_Unlock();
    for (offset = 0U; offset < length; offset += 2U) {
        uint16_t halfword = bytes[offset];
        if (offset + 1U < length) {
            halfword |= (uint16_t)((uint16_t)bytes[offset + 1U] << 8U);
        } else {
            halfword |= UINT16_C(0xff00);
        }
        if (HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_HALFWORD,
                address + (uint32_t)offset,
                halfword) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    HAL_FLASH_Lock();
    return true;
}

static uint32_t ecu_clock_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static bool uds_init(void)
{
    LinkStm32CanOps ops;
    LinkStm32F103UdsEcuConfig ecu_config;
    LinkStm32UdsServerConfig transport_config;

    memset(&ecu_config, 0, sizeof(ecu_config));
    ecu_config.flash.read = ecu_flash_read;
    ecu_config.flash.erase_page = ecu_flash_erase;
    ecu_config.flash.program = ecu_flash_program;
    ecu_config.flash.page_a_address = LINK_STM32F103_UDS_STATE_PAGE_A;
    ecu_config.flash.page_b_address = LINK_STM32F103_UDS_STATE_PAGE_B;
    ecu_config.flash.page_size = LINK_STM32F103_FLASH_PAGE_BYTES;
    ecu_config.clock_ms = ecu_clock_ms;
    ecu_config.security_key = demo_security_key;

    link_stm32_bxcan_hal_init(&uds_hal, &hcan1, 0U, 14U);
    ops = link_stm32_bxcan_hal_ops(&uds_hal);
    if (!link_stm32_can_init(&uds_can, &ops) ||
        !link_stm32_bxcan_hal_start_standard_dual(
            &uds_hal, UINT32_C(0x7e0), UINT32_C(0x7df)) ||
        !link_stm32f103_uds_ecu_init(&uds_ecu, &ecu_config)) {
        return false;
    }

    memset(&transport_config, 0, sizeof(transport_config));
    transport_config.address.tx_can_id = UINT32_C(0x7e8);
    transport_config.address.rx_can_id = UINT32_C(0x7e0);
    transport_config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    transport_config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    transport_config.functional_address_enabled = true;
    transport_config.functional_address.tx_can_id = UINT32_C(0x7e8);
    transport_config.functional_address.rx_can_id = UINT32_C(0x7df);
    transport_config.functional_address.addressing_mode =
        LINK_ISOTP_ADDRESSING_NORMAL;
    transport_config.functional_address.target_type =
        LINK_ISOTP_TARGET_FUNCTIONAL;
    transport_config.consecutive_timeout_us = UINT64_C(1000000);
    transport_config.flow_control_timeout_us = UINT64_C(1000000);
    transport_config.max_wait_frames = 3U;
    transport_config.can_fd = false;
    transport_config.data_length = 8U;
    transport_config.pad_short_frames = true;
    transport_config.padding_byte = 0xccU;

    return link_stm32_uds_server_init(
        &uds_transport,
        &uds_can,
        link_stm32f103_uds_ecu_server(&uds_ecu),
        &transport_config,
        uds_rx, sizeof(uds_rx),
        uds_tx, sizeof(uds_tx));
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_CAN_Init();

    if (!uds_init()) Error_Handler();

    for (;;) {
        LinkStm32UdsServerResult result;
        uint8_t reset_type = 0U;

        link_uds_server_tick(link_stm32f103_uds_ecu_server(&uds_ecu));
        result = link_stm32_uds_server_poll(&uds_transport);

        if (result == LINK_STM32_UDS_SERVER_RESULT_REQUEST_COMPLETE &&
            link_uds_server_take_pending_ecu_reset(
                link_stm32f103_uds_ecu_server(&uds_ecu), &reset_type)) {
            (void)reset_type;
            reset_pending = true;
            reset_requested_ms = HAL_GetTick();
        }

        if (reset_pending &&
            (uint32_t)(HAL_GetTick() - reset_requested_ms) >= 50U) {
            NVIC_SystemReset();
        }
    }
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    if (hcan == &hcan1) link_stm32_can_rx_isr(&uds_can);
}

