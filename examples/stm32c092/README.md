<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32C092 FDCAN UDS example

This is the LINK port for the STM32C092 project supplied in MBLINK issue #27. It deliberately ports LINK's own ISO-TP/UDS engine rather than importing the separate protocol stack bundled in the attachment.

The supplied project establishes the target: STM32C092RCTx, 48 MHz system/FDCAN clock, Classical CAN at 500 kbit/s, RX FIFO0 interrupts and `HAL_GetTick()`.

The attachment itself demonstrates an ECU/server role: receive tester requests on `0x7E0/0x7DF` and reply on `0x7E8`. LINK's reference firmware deliberately implements the diagnostic-tester/client role needed to talk to a vehicle: transmit on `0x7E0`, receive the ECU response on `0x7E8`.

## Hardware project and licensing

Keep the Cube-generated `Drivers/`, `Inc/`, `Src/`, `.ioc`, startup file and Keil project. Do not import the attachment's `library/src`, `library/crypto` or old `stm32c092/uds_*` protocol implementation. Those files identify themselves with a separate `LicenseRef-STM32-UDS-Research-Education-Commercial-1.0` marker while the attachment does not contain that referenced licence text; LINK already provides the corresponding GPL-3.0-or-later protocol implementation.

## Nominal CAN timing

Do not blindly retain the attachment's nominal timing fields. Its values calculate to 500 kbit/s, but `NominalSyncJumpWidth = 12` with `NominalTimeSeg2 = 2` is not a sound relationship.

For a 48 MHz FDCAN clock, use this conservative 500 kbit/s Classical-CAN starting point:

```text
NominalPrescaler     = 6
NominalTimeSeg1      = 13
NominalTimeSeg2      = 2
NominalSyncJumpWidth = 2
```

That is 16 time quanta per bit with an 87.5% sample point. Confirm the generated values in CubeMX against the exact board clock tree before flashing hardware.

## Sources to add

```text
LINK/src/core/isotp.c
LINK/src/uds/uds.c
LINK/src/uds/uds_services.c
LINK/platform/stm32/link-stm32-can.c
LINK/platform/stm32/link-stm32-uds.c
LINK/examples/stm32c092/link-stm32c092-hal.c
LINK/examples/stm32c092/link-stm32c092-example.c
LINK/src/infiltratr-common/src/core.c
```

Include paths:

```text
LINK/include
LINK/platform/stm32
LINK/examples/stm32c092
LINK/src/infiltratr-common/include
<your Cube project>/Inc
```

Normal LINK/MBLINK/JAGLINK product builds do not compile STM32Cube HAL.

## main.c integration

After the Cube-generated GPIO/FDCAN/UART initialisation:

```c
#include "link-stm32c092-example.h"

if (!link_stm32c092_example_init(&hfdcan1)) {
    Error_Handler();
}

while (1) {
    link_stm32c092_example_process();
}
```

Use both FDCAN callbacks:

```c
void HAL_FDCAN_RxFifo0Callback(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t RxFifo0ITs)
{
    if ((RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE) != 0U) {
        link_stm32c092_example_rx_fifo0_irq(hfdcan);
    }
}

void HAL_FDCAN_TxEventFifoCallback(
    FDCAN_HandleTypeDef *hfdcan,
    uint32_t TxEventFifoITs)
{
    link_stm32c092_example_tx_event_irq(hfdcan, TxEventFifoITs);
}
```

RX FIFO0 is drained into an eight-frame bounded queue and every frame keeps the interrupt-time `HAL_GetTick()` value. Main-loop latency therefore does not turn an on-time queued response into a false ISO-TP/UDS timeout.

TX uses `FDCAN_STORE_TX_EVENTS` plus a unique message marker. LINK does not advance ISO-TP separation/flow-control timing or start the UDS P2 timer merely because `HAL_FDCAN_AddMessageToTxFifoQ()` accepted a frame; it waits for the matching Tx Event FIFO record. A lost Tx-event FIFO element becomes an explicit transport failure, and a hardware TX that never completes is bounded by the configured flow-control timeout.

## Example operation

At startup the example issues the read-only request `22 F1 90` for VIN. LINK performs ISO-TP reassembly and flow control. On success:

```c
if (link_stm32c092_example_state() == LINK_STM32C092_EXAMPLE_VIN_READY) {
    const char *vin = link_stm32c092_example_vin();
}
```

Negative responses are retained through `link_stm32c092_example_negative_response_code()`, and RX overflow is visible through `link_stm32c092_example_dropped_frames()`.

### ReadDTCInformation hardware testing

After the initial VIN transaction reaches `LINK_STM32C092_EXAMPLE_VIN_READY` (or returns a normal UDS negative response), the same example can issue any of LINK's 27 supported `ReadDTCInformation (0x19)` report types. The caller supplies the parameters required by that report type; the example uses `link_uds_build_read_dtc_information_request()`, sends the request through the existing STM32 ISO-TP/UDS client, and validates a positive response with `link_uds_decode_read_dtc_information_response()`.

For example, `reportDTCByStatusMask (0x02)`:

```c
LinkUdsDtcInformationRequest request =
    LINK_UDS_DTC_INFORMATION_REQUEST_INIT;

request.subfunction = LINK_UDS_DTC_REPORT_BY_STATUS_MASK;
request.status_mask = LINK_UDS_DTC_STATUS_MASK_ALL;

if (!link_stm32c092_example_start_dtc_report(&request)) {
    Error_Handler();
}

while (link_stm32c092_example_state() ==
       LINK_STM32C092_EXAMPLE_READING_DTC) {
    link_stm32c092_example_process();
}

if (link_stm32c092_example_state() ==
    LINK_STM32C092_EXAMPLE_DTC_READY) {
    const LinkUdsDtcInformationResponse *dtc =
        link_stm32c092_example_dtc_response();
    /* Inspect dtc->record_format, metadata and records/records_length. */
}
```

The single entry point covers the complete catalogue: `0x01..0x19`, `0x42` and `0x55`. Populate the request fields according to the catalogue's request shape:

- status mask: `0x01`, `0x02`, `0x0F`, `0x11`, `0x12`, `0x13`;
- no extra parameters: `0x03`, `0x0A..0x0E`, `0x14`, `0x15`;
- DTC plus record number: `0x04`, `0x06`, `0x10`;
- record number: `0x05`, `0x16`;
- severity mask plus status mask: `0x07`, `0x08`;
- DTC: `0x09`;
- status mask plus memory selection: `0x17`;
- DTC plus record number plus memory selection: `0x18`, `0x19`;
- functional-group identifier plus status and severity masks: `0x42`;
- functional-group identifier: `0x55`.

Use ECU-appropriate DTC, record, memory-selection and functional-group values when a report type requires them. A standards-compliant negative response such as “subfunction not supported” or “request out of range” is a valid hardware observation and does not by itself mean the LINK integration failed. The legacy mirror/emissions report types retained for older ECUs are still available even though several were withdrawn from ISO 14229-1:2020.

The test entry point intentionally requires a positive response (the suppress-positive-response bit must be clear) so that the returned PDU can be decoded and inspected.

The example remains intentionally read-only. LINK's wider UDS catalogue is not automatically authorised by this firmware.

## Timing resolution and CAN FD

The supplied project exposes `HAL_GetTick()` at 1 ms resolution. LINK extends the wrapping 32-bit clock to monotonic 64-bit microseconds and preserves the ISR tick for RX and TX-completion events. Sub-millisecond STmin values are therefore handled conservatively; they are never transmitted early.

The HAL mapper understands canonical CAN-FD DLC sizes through 64 bytes, but the supplied project/example remains Classical CAN with bit-rate switching disabled. Enabling CAN FD requires a matching Cube FDCAN configuration change.
