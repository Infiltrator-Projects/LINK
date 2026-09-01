// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_STM32C092_EXAMPLE_H
#define LINK_STM32C092_EXAMPLE_H

#include "fdcan.h"
#include "link/uds_dtc.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_STM32C092_EXAMPLE_IDLE = 0,
    LINK_STM32C092_EXAMPLE_READING_VIN,
    LINK_STM32C092_EXAMPLE_VIN_READY,
    LINK_STM32C092_EXAMPLE_READING_DTC,
    LINK_STM32C092_EXAMPLE_DTC_READY,
    LINK_STM32C092_EXAMPLE_NEGATIVE_RESPONSE,
    LINK_STM32C092_EXAMPLE_FAILED
} LinkStm32C092ExampleState;

bool link_stm32c092_example_init(FDCAN_HandleTypeDef *hfdcan);
bool link_stm32c092_example_start_dtc_report(
    const LinkUdsDtcInformationRequest *request);
void link_stm32c092_example_process(void);
void link_stm32c092_example_rx_fifo0_irq(FDCAN_HandleTypeDef *hfdcan);
void link_stm32c092_example_tx_event_irq(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t tx_event_fifo_its);
LinkStm32C092ExampleState link_stm32c092_example_state(void);
const char *link_stm32c092_example_vin(void);
const LinkUdsDtcInformationResponse *
link_stm32c092_example_dtc_response(void);
LinkUdsResult link_stm32c092_example_dtc_decode_result(void);
uint8_t link_stm32c092_example_negative_response_code(void);
uint32_t link_stm32c092_example_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif
