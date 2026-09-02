<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32F103/F107/F767 bxCAN UDS example

This directory is the concrete STM32Cube bxCAN implementation requested in
LINK issue #4. It supports STM32F103, STM32F107 and STM32F767 through the CAN
HAL API shared by STM32CubeF1 and STM32CubeF7.

`link-stm32-bxcan-hal.c` binds the Cube-generated `CAN_HandleTypeDef` to
`LinkStm32CanOps`. `link-stm32-bxcan-example.c` is a complete allocation-free
UDS tester/client: it sends `22 F1 90` on `0x7E0`, accepts the ECU response on
`0x7E8`, performs ISO-TP reassembly and exposes the 17-byte VIN.

The MCU still requires an external CAN transceiver and the correct board-level
pin, clock and interrupt configuration. LINK does not replace Cube's generated
GPIO, RCC, CAN MSP or NVIC setup.

## Cube configuration by family

The values below are verified arithmetic starting points for 500 kbit/s with
an 88.9% sample point. Use them only when the actual APB1/CAN clock matches the
table; otherwise recalculate the prescaler from the real clock tree.

| Family | Typical CAN clock | Prescaler | BS1 | BS2 | SJW | CAN/filter choice |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| STM32F103 | 36 MHz | 4 | 15 TQ | 2 TQ | 1 TQ | CAN1, bank 0; slave split 14 is ignored on a single-CAN part |
| STM32F107 | 36 MHz | 4 | 15 TQ | 2 TQ | 1 TQ | CAN1: bank 0/split 14; CAN2: bank 14/split 14 |
| STM32F767 | 54 MHz | 6 | 15 TQ | 2 TQ | 1 TQ | CAN1: bank 0/split 14; CAN2: bank 14/split 14 |

For all three families configure Classical CAN normal mode, automatic
retransmission enabled, RX FIFO0 message-pending interrupt enabled and the
corresponding CAN TX/RX IRQs in the NVIC. The adapter installs an exact
32-bit-list hardware filter for the configured standard response ID and enables
the TX-completion, abort and error interrupt sources it needs.

The default example uses CAN1, filter bank 0 and a CAN1/CAN2 split at bank 14.
When using CAN2 on F107/F767, set `filter_bank = 14` and retain
`slave_start_filter_bank = 14`. Coordinate those numbers with every other CAN
filter owner in the application.

## Sources to add

```text
LINK/src/core/isotp.c
LINK/src/uds/uds.c
LINK/src/uds/uds_services.c
LINK/platform/stm32/link-stm32-can.c
LINK/platform/stm32/link-stm32-uds.c
LINK/examples/stm32-bxcan/link-stm32-bxcan-hal.c
LINK/examples/stm32-bxcan/link-stm32-bxcan-example.c
LINK/src/infiltratr-common/src/core.c
```

Include paths:

```text
LINK/include
LINK/platform/stm32
LINK/examples/stm32-bxcan
LINK/src/infiltratr-common/include
<your Cube project>/Core/Inc
```

The generated project must provide `can.h`, `hcan1` (or the selected handle),
`MX_CAN1_Init()` and `HAL_GetTick()`. No STM32Cube header enters LINK's portable
core or its normal host build.

## main.c integration

Add the LINK header in a Cube `USER CODE` include section. Initialise LINK only
after Cube has configured the selected CAN peripheral:

```c
#include "link-stm32-bxcan-example.h"

MX_CAN1_Init();

if (!link_stm32_bxcan_example_init(&hcan1)) {
    Error_Handler();
}

while (1) {
    link_stm32_bxcan_example_process();
}
```

Use the explicit configuration for CAN2 or non-default diagnostic IDs:

```c
LinkStm32BxCanTesterConfig tester =
    LINK_STM32_BXCAN_TESTER_CONFIG_INIT;

tester.request_can_id = 0x7E0U;   /* STM32 -> ECU/simulator */
tester.response_can_id = 0x7E8U;  /* ECU/simulator -> STM32 */
tester.filter_bank = 14U;          /* CAN2 on a 14/14 split */
tester.slave_start_filter_bank = 14U;
tester.read_vin_on_init = true;

if (!link_stm32_bxcan_example_init_tester(&hcan2, &tester)) {
    Error_Handler();
}
```

## Required HAL callbacks

Forward all callbacks below. Merely accepting a frame into a bxCAN mailbox is
not a completed ISO-TP transmission; LINK advances timing only after the
matching HAL completion callback.

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_rx_fifo0_irq(hcan);
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_complete_irq(hcan, CAN_TX_MAILBOX0);
}

void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_complete_irq(hcan, CAN_TX_MAILBOX1);
}

void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_complete_irq(hcan, CAN_TX_MAILBOX2);
}

void HAL_CAN_TxMailbox0AbortCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_abort_irq(hcan, CAN_TX_MAILBOX0);
}

void HAL_CAN_TxMailbox1AbortCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_abort_irq(hcan, CAN_TX_MAILBOX1);
}

void HAL_CAN_TxMailbox2AbortCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_tx_abort_irq(hcan, CAN_TX_MAILBOX2);
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_example_error_irq(hcan);
}
```

The adapter permits one LINK-owned hardware frame at a time, matches completion
or abort to the exact HAL mailbox token, snapshots `HAL_GetTick()` in the ISR
and rejects CAN-FD frames because bxCAN is a Classical-CAN controller.

## Reading the result

```c
if (link_stm32_bxcan_example_state() ==
    LINK_STM32_BXCAN_EXAMPLE_VIN_READY) {
    const char *vin = link_stm32_bxcan_example_vin();
}
```

A normal UDS negative response is exposed through
`link_stm32_bxcan_example_negative_response_code()`. Queue overflow is visible
through `link_stm32_bxcan_example_dropped_frames()`.

Host tests validate the HAL mapping, exact physical/functional filters,
standard and extended IDs, remote-frame rejection, mailbox-specific completion,
abort/error propagation and interrupt-time timestamps. CI also cross-compiles
the adapter and UDS example as freestanding Cortex-M3 (F103/F107 class) and
Cortex-M7 (F767 class) code. Electrical timing, pins, transceiver behaviour and
vehicle communication remain hardware validation steps.
