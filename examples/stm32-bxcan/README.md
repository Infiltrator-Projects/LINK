<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32F103/F107/F767 bxCAN UDS example

This directory is the concrete STM32Cube bxCAN implementation requested in
LINK issue #4. It supports STM32F103, STM32F107 and STM32F767 through the CAN
HAL API shared by STM32CubeF1 and STM32CubeF7.

`link-stm32-bxcan-hal.c` binds the Cube-generated `CAN_HandleTypeDef` to
`LinkStm32CanOps`. `link-stm32-bxcan-tester-example.c` is a complete allocation-free
UDS tester/client: it sends `22 F1 90` on `0x7E0`, accepts the ECU response on
`0x7E8`, performs ISO-TP reassembly and exposes the 17-byte VIN.

The MCU still requires an external CAN transceiver and the correct board-level
pin, clock and interrupt configuration. LINK does not replace Cube's generated
GPIO, RCC, CAN MSP or NVIC setup.

## Choose the role first: tester/client or ECU/server

There are two deliberately different examples in this directory tree:

- `link-stm32-bxcan-tester-example.c` is a **tester/client**. It transmits a
  diagnostic request such as `22 F1 90` and waits for another ECU at 0x7E8 to
  answer. It will never behave as the ECU that sends that response.
- `link-stm32f103-uds-ecu.c` together with `issue-32/Src-main.c` is the
  **STM32F103 ECU/server**. It receives requests at 0x7E0/0x7DF and produces UDS
  responses at 0x7E8.

Do not transplant the tester example into a project whose requirement is
"STM32F103 acts as the ECU/server". Issue #36 showed that the previous generic
file name made these opposite roles too easy to confuse.


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
retransmission enabled and the RX FIFO0 message-pending IRQ in the NVIC. LINK's
default bxCAN path does not require CAN TX or error IRQs. The adapter installs
an exact 32-bit-list hardware filter for the configured standard response ID,
drains received frames into LINK's bounded queue and obtains real TX completion
from bxCAN's latched mailbox result flags.

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
LINK/examples/stm32-bxcan/link-stm32-bxcan-tester-example.c
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
#include "link-stm32-bxcan-tester-example.h"

MX_CAN1_Init();

if (!link_stm32_bxcan_tester_example_init(&hcan1)) {
    Error_Handler();
}

while (1) {
    link_stm32_bxcan_tester_example_process();
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

if (!link_stm32_bxcan_tester_example_init_tester(&hcan2, &tester)) {
    Error_Handler();
}
```

## HAL integration: one required callback

The default LINK bxCAN integration needs only the receive callback below. The
ISR drains valid CAN frames into LINK's bounded queue; ISO-TP and UDS continue
later in normal main-loop processing.

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    link_stm32_bxcan_tester_example_rx_fifo0_irq(hcan);
}
```

No TX or error callback is required. After `HAL_CAN_AddTxMessage()` admits a
frame to a mailbox, LINK waits for actual hardware completion by polling that
mailbox's sticky RQCP/TXOK/ALST/TERR result bits. A successful TX is therefore
not declared merely because the HAL accepted it. Arbitration loss and transmit
error remain failures, and LINK records the polling tick as a conservative
completion time.

Projects that deliberately enable the bxCAN TX NVIC may optionally forward the
three successful mailbox-complete callbacks to preserve the exact ISR tick:

| HAL callback | LINK forward |
| --- | --- |
| `HAL_CAN_TxMailbox0CompleteCallback` | `link_stm32_bxcan_tester_example_tx_complete_irq(hcan, CAN_TX_MAILBOX0)` |
| `HAL_CAN_TxMailbox1CompleteCallback` | `link_stm32_bxcan_tester_example_tx_complete_irq(hcan, CAN_TX_MAILBOX1)` |
| `HAL_CAN_TxMailbox2CompleteCallback` | `link_stm32_bxcan_tester_example_tx_complete_irq(hcan, CAN_TX_MAILBOX2)` |

Do not enable a HAL TX ISR and then discard its successful mailbox callback:
the HAL may clear the sticky result bits before LINK polls them. LINK treats a
released mailbox with neither latched success nor a forwarded success callback
as a transport failure. Abort/error forwarding is unnecessary.

The default one-callback path therefore keeps the simple MCU port surface
requested in issue #35 while retaining LINK's stronger transport contract:
exact diagnostic-ID filtering, queued RX outside ISO-TP processing, and real
wire-completion-aware timing.

## Reading the result

```c
if (link_stm32_bxcan_tester_example_state() ==
    LINK_STM32_BXCAN_TESTER_EXAMPLE_VIN_READY) {
    const char *vin = link_stm32_bxcan_tester_example_vin();
}
```

A normal UDS negative response is exposed through
`link_stm32_bxcan_tester_example_negative_response_code()`. Queue overflow is visible
through `link_stm32_bxcan_tester_example_dropped_frames()`.

Host tests validate the HAL mapping, exact physical/functional filters,
standard and extended IDs, remote-frame rejection, mailbox-specific completion,
abort/error propagation and interrupt-time timestamps. CI also cross-compiles
the adapter and UDS example as freestanding Cortex-M3 (F103/F107 class) and
Cortex-M7 (F767 class) code. Electrical timing, pins, transceiver behaviour and
vehicle communication remain hardware validation steps.


## Complete STM32F103 ECU/server reference

LINK issue #32 adds a server-role reference for the 512 KiB STM32F103 rather
than extending the VIN tester into a second ad-hoc protocol stack. See
[`issue-32/README.md`](issue-32/README.md).

The F103 ECU core wires all 27 standard LINK UDS service IDs, routes the full
ReadDTCInformation surface through the shared rich DTC handler, implements
persistent ClearDiagnosticInformation state, and stores bounded application
state in an alternating two-page CRC-protected flash journal. Programming
services are deliberately limited to a non-executable data sandbox; production
firmware programming still belongs to the target bootloader/security policy.

The supplied Cube integration targets CAN1 on PA11/PA12 at 500 kbit/s and
reserves the final two 2 KiB pages of a 512 KiB device.


## ClearDiagnosticInformation and ReadDTCInformation verification

The STM32F103 reference follows the ISO session table for
`0x14 ClearDiagnosticInformation`: the service is available in the default and
non-default diagnostic sessions and LINK does not invent a SecurityAccess
requirement for it. Product/OEM policy can still add a stronger restriction
outside the portable reference when required.

A successful all-group clear:

`14 FF FF FF`

returns positive service `54`.

Immediately after that clear LINK sets the DTC status to
`testNotCompletedSinceLastClear | testNotCompletedThisOperationCycle`
(`0x50`). Consequently `19 01 FF` can still count those DTC definitions:
`0x50 & 0xFF != 0`. To verify that fault-indicating state was cleared, query a
fault-oriented mask such as `0x0D`
(`testFailed | pendingDTC | confirmedDTC`); the regression suite requires that
count to become zero after the successful clear.
