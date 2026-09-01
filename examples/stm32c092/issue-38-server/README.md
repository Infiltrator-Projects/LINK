<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# MBLINK issue #38: STM32C092 UDS ECU/server port

This directory is the concrete server-role integration requested by
`chenyurong22` in MBLINK issue #38. It is intentionally the opposite CAN
role from LINK's original STM32 diagnostic-tester example.

The STM32C092 is the ECU/server:

```text
PC / CAN UDS tester                 STM32C092 LINK ECU/server
-------------------                 ---------------------------
request  0x7E0  -------------------> RX 0x7E0
response 0x7E8  <------------------- TX 0x7E8
```

Therefore a tester transmitting `10 01` to CAN ID `0x7E0` is supposed to
enter the STM32 RX FIFO0 interrupt. LINK installs an exact `0x7E0` hardware
filter in this server build. The earlier tester/client example filtered
`0x7E8`, which is why a `0x7E0` request could never reach its RX callback.

## What is implemented

The shared LINK core now contains `LinkUdsServer`, an allocation-free server
dispatcher using the same 27-service ISO 14229 catalogue as the client codec
layer. The server has portable built-in implementations for:

- `0x10 DiagnosticSessionControl`, including P2/P2* timing response fields;
- `0x3E TesterPresent`, including suppress-positive-response handling;
- `0x19 ReadDTCInformation` through `link_uds_server_dtc_handler()`, covering
  every LINK report type `0x01..0x19`, `0x42`, and `0x55`;
- `0x22 ReadDataByIdentifier` for DID `F190` in this example, returning a
  17-byte demonstration VIN.

Every service in LINK's 27-service catalogue can be bound to an ECU
application callback with `link_uds_server_set_handler()`. This is important
for services such as SecurityAccess, RoutineControl, IOControl, memory access,
and RequestDownload/TransferData: the protocol dispatcher is generic, while
the actual ECU memory, flash, security, routine and actuator operations must
be supplied by the target application rather than invented inside LINK.

The STM32 transport in `platform/stm32/link-stm32-uds-server.c` performs
ISO-TP request reassembly, generated Flow Control, response segmentation,
Flow Control reception for multi-frame responses, real FDCAN TX-completion
tracking, and timeout handling. It supports Classical CAN and CAN FD using
LINK's existing ISO-TP implementation, including extended FF_DL above 4095
bytes when caller buffers are large enough.

## Files to add to the KEIL target

```text
LINK/src/core/isotp.c
LINK/src/uds/uds.c
LINK/src/uds/uds_services.c
LINK/src/uds/uds_server.c
LINK/src/infiltratr-common/src/core.c
LINK/platform/stm32/link-stm32-can.c
LINK/platform/stm32/link-stm32-uds-server.c
LINK/examples/stm32c092/link-stm32c092-hal.c
LINK/examples/stm32c092/link-stm32c092-server-example.c
```

Include paths:

```text
LINK/include
LINK/platform/stm32
LINK/examples/stm32c092
LINK/src/infiltratr-common/include
<STM32 project>/Inc
```

Do not compile the old duplicate project-local ISO-TP/UDS server stack at the
same time as LINK. Keep the Cube-generated peripheral/startup files and use
`Src-main.c` in this directory as the application integration reference.

## FDCAN timing

For the supplied 48 MHz FDCAN clock and 500 kbit/s Classical CAN setup:

```text
NominalPrescaler     = 6
NominalTimeSeg1      = 13
NominalTimeSeg2      = 2
NominalSyncJumpWidth = 2
```

The original `SJW=12` / `TSEG2=2` combination should not be retained.

## First hardware test: `10 01`

Transmit this ISO-TP Single Frame from the PC/CAN tester on CAN ID `0x7E0`:

```text
02 10 01
```

LINK responds from CAN ID `0x7E8` with:

```text
06 50 01 00 32 01 F4
```

When Classical CAN padding is enabled in the submitted KEIL project the full
8-byte frame is `06 50 01 00 32 01 F4 CC`.

The UDS response is `50 01 00 32 01 F4`: default session, P2=50 ms,
P2*=500 x 10 ms = 5000 ms.

If that request does not enter `HAL_FDCAN_RxFifo0Callback()`, the failure is
below UDS/ISO-TP (physical bus, transceiver, bitrate/clock, FDCAN instance,
NVIC or Cube pin configuration), because this server build's filter accepts
`0x7E0` explicitly.

## `0x19` bench test

The server example now contains two bounded demonstration DTC records so the
actual `ReadDTCInformation` data path is exercised rather than returning an
empty list:

- DTC `0x123456`, status `0x09`;
- DTC `0xABCDEF`, status `0x08`.

Send on `0x7E0`:

```text
03 19 02 FF
```

The UDS positive response begins:

```text
59 02 FF 12 34 56 09 AB CD EF 08
```

That is 11 UDS bytes, so on Classical CAN it is a multi-frame ISO-TP response.
The tester must return a Flow Control frame on `0x7E0` after the First Frame,
for example `30 00 00`.

The same handler dispatches all 27 LINK ReadDTCInformation report types.
Replace the demonstration store with the ECU application's real DTC provider
for production use.

## Submitted KEIL project findings and validation

The reporter's supplied project was also tested independently of LINK's own
example. The final repair made these additional changes in the submitted
project structure:

- keeps STM32 as the ECU/server: physical request `0x7E0`, functional request
  `0x7DF`, response `0x7E8`;
- adds a main-loop FIFO0 drain fallback, so a frame that reaches FDCAN can
  still be processed when the RX callback is not delivered;
- explicitly assigns RX FIFO0 to interrupt line 0;
- fixes the submitted `uds_dtc.c` request-length table to the ISO 14229-1
  parameter shapes for all `0x01..0x19`, `0x42`, and `0x55`;
- installs a real bounded DTC/DID demonstration backend instead of calling the
  submitted project's empty `uds_c092_app_init_default()`, which otherwise
  returned `7F 19 11` for ReadDTCInformation;
- expands FDCAN DLC conversion and transport handling through 64-byte CAN FD.

One earlier diagnosis was corrected during this work: in the STM32C0 HAL used
by the submitted project, `FDCAN_DLC_BYTES_8` is already the literal value
8. Therefore the old direct cast did **not** explain the reporter's Classical
CAN `10 01` interrupt symptom. Explicit DLC conversion is still required for
CAN FD DLCs 12 through 64.

The repaired submitted-project sources were exercised with:

- strict C11 host builds;
- Cortex-M0+ syntax checking against the submitted STM32C0 HAL/CMSIS headers;
- `10 01 -> 50 01 00 32 01 F4`;
- all 27 standards-shaped `0x19` requests returning positive `0x59`
  responses from the demonstration backend;
- `22 F190` VIN response;
- the submitted `uds_app_fdcan + endpoint + isotp + can_transport_fdcan`
  path with a mocked FDCAN HAL, including multi-frame DTC response and Flow
  Control;
- CAN FD 64-byte Single Frames and a 5000-byte extended ISO-TP First Frame
  `10 00 00 00 13 88`.

A physical board/transceiver test remains the final proof of the electrical
CAN path.

### RX interrupt fallback

The issue-38 `Src-main.c` now calls
`link_stm32c092_server_example_poll_rx()` before processing the server.
The normal `HAL_FDCAN_RxFifo0Callback()` path remains active. If the board
receives into FIFO0 but the IRQ is not delivered, the main loop therefore
still drains and processes the frame.

## F190 VIN test

Send on `0x7E0`:

```text
03 22 F1 90
```

The example returns DID `F190` plus a 17-byte demonstration VIN using
multi-frame ISO-TP. Replace `example_vin` in
`link-stm32c092-server-example.c` with the ECU application's VIN provider.

## Other UDS services

Register target behaviour before entering the main loop:

```c
link_stm32c092_server_example_set_handler(
    LINK_UDS_SERVICE_ROUTINE_CONTROL,
    my_routine_control_handler,
    my_context);
```

The same mechanism applies to all 27 registered service IDs, including
`0x11`, `0x14`, `0x23`, `0x24`, `0x27`, `0x28`, `0x29`, `0x2A`, `0x2C`,
`0x2E`, `0x2F`, `0x31`, `0x34..0x38`, `0x3D`, `0x83..0x87`.
