// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_STM32_BXCAN_EXAMPLE_H
#define LINK_STM32_BXCAN_EXAMPLE_H

#include "can.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_STM32_BXCAN_TESTER_REQUEST_ID UINT32_C(0x7e0)
#define LINK_STM32_BXCAN_TESTER_RESPONSE_ID UINT32_C(0x7e8)

typedef struct {
    uint32_t request_can_id;
    uint32_t response_can_id;
    uint32_t filter_bank;
    uint32_t slave_start_filter_bank;
    bool read_vin_on_init;
} LinkStm32BxCanTesterConfig;

#define LINK_STM32_BXCAN_TESTER_CONFIG_INIT { \
    LINK_STM32_BXCAN_TESTER_REQUEST_ID, \
    LINK_STM32_BXCAN_TESTER_RESPONSE_ID, \
    0U, \
    14U, \
    true \
}

typedef enum {
    LINK_STM32_BXCAN_EXAMPLE_IDLE = 0,
    LINK_STM32_BXCAN_EXAMPLE_READING_VIN,
    LINK_STM32_BXCAN_EXAMPLE_VIN_READY,
    LINK_STM32_BXCAN_EXAMPLE_NEGATIVE_RESPONSE,
    LINK_STM32_BXCAN_EXAMPLE_FAILED,
    LINK_STM32_BXCAN_EXAMPLE_READY
} LinkStm32BxCanExampleState;

void link_stm32_bxcan_example_default_tester_config(
    LinkStm32BxCanTesterConfig *config);
bool link_stm32_bxcan_example_init_tester(
    CAN_HandleTypeDef *hcan,
    const LinkStm32BxCanTesterConfig *config);
bool link_stm32_bxcan_example_init(CAN_HandleTypeDef *hcan);
bool link_stm32_bxcan_example_start_vin(void);
void link_stm32_bxcan_example_process(void);

void link_stm32_bxcan_example_rx_fifo0_irq(CAN_HandleTypeDef *hcan);
void link_stm32_bxcan_example_tx_complete_irq(
    CAN_HandleTypeDef *hcan,
    uint32_t mailbox);
void link_stm32_bxcan_example_tx_abort_irq(
    CAN_HandleTypeDef *hcan,
    uint32_t mailbox);
void link_stm32_bxcan_example_error_irq(CAN_HandleTypeDef *hcan);

LinkStm32BxCanExampleState link_stm32_bxcan_example_state(void);
const char *link_stm32_bxcan_example_vin(void);
uint8_t link_stm32_bxcan_example_negative_response_code(void);
uint32_t link_stm32_bxcan_example_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif
