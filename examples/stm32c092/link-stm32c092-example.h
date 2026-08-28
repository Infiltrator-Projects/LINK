// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32c092-example.h
 * @brief Copy-ready STM32C092 LINK UDS tester example.
 */
#ifndef LINK_STM32C092_EXAMPLE_H
#define LINK_STM32C092_EXAMPLE_H

#include "fdcan.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LINK_STM32C092_EXAMPLE_IDLE = 0,
    LINK_STM32C092_EXAMPLE_READING_VIN,
    LINK_STM32C092_EXAMPLE_VIN_READY,
    LINK_STM32C092_EXAMPLE_NEGATIVE_RESPONSE,
    LINK_STM32C092_EXAMPLE_FAILED
} LinkStm32C092ExampleState;

/**
 * Configure the supplied project's FDCAN1-compatible handle and begin one
 * read-only UDS F190 VIN request on physical 0x7E0/0x7E8 addressing.
 */
bool link_stm32c092_example_init(FDCAN_HandleTypeDef *hfdcan);

/** Progress LINK in the main while(1) loop. */
void link_stm32c092_example_process(void);

/** Call from HAL_FDCAN_RxFifo0Callback for the same FDCAN handle. */
void link_stm32c092_example_rx_fifo0_irq(FDCAN_HandleTypeDef *hfdcan);

LinkStm32C092ExampleState link_stm32c092_example_state(void);
const char *link_stm32c092_example_vin(void);
uint8_t link_stm32c092_example_negative_response_code(void);
uint32_t link_stm32c092_example_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif
