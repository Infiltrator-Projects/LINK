// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_STM32C092_SERVER_EXAMPLE_H
#define LINK_STM32C092_SERVER_EXAMPLE_H

#include "fdcan.h"
#include "link/uds_server.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_STM32C092_ECU_REQUEST_ID UINT32_C(0x7e0)
#define LINK_STM32C092_ECU_RESPONSE_ID UINT32_C(0x7e8)

typedef struct {
    uint32_t request_can_id;
    uint32_t response_can_id;
    bool can_fd;
    uint8_t data_length;
} LinkStm32C092ServerConfig;

#define LINK_STM32C092_SERVER_CONFIG_INIT \
    { LINK_STM32C092_ECU_REQUEST_ID, LINK_STM32C092_ECU_RESPONSE_ID, false, 8U }

bool link_stm32c092_server_example_init(
    FDCAN_HandleTypeDef *hfdcan,
    const LinkStm32C092ServerConfig *config);
void link_stm32c092_server_example_process(void);
/* Main-loop fallback for targets where the RX FIFO interrupt is not delivered. */
void link_stm32c092_server_example_poll_rx(FDCAN_HandleTypeDef *hfdcan);
void link_stm32c092_server_example_rx_fifo0_irq(FDCAN_HandleTypeDef *hfdcan);
void link_stm32c092_server_example_tx_event_irq(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t tx_event_fifo_its);
bool link_stm32c092_server_example_set_handler(
    uint8_t service,
    LinkUdsServerHandlerFn handler,
    void *context);
uint8_t link_stm32c092_server_example_session(void);
uint8_t link_stm32c092_server_example_last_nrc(void);
uint32_t link_stm32c092_server_example_completed_requests(void);
uint32_t link_stm32c092_server_example_dropped_frames(void);

#ifdef __cplusplus
}
#endif

#endif
