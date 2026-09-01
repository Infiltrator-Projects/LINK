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

The UDS response is `50 01 00 32 01 F4`: default session, P2=50 ms,
P2*=500 x 10 ms = 5000 ms.

If that request does not enter `HAL_FDCAN_RxFifo0Callback()`, the failure is
below UDS/ISO-TP (physical bus, transceiver, bitrate/clock, FDCAN instance,
NVIC or Cube pin configuration), because this server build's filter accepts
`0x7E0` explicitly.

## `0x19` zero-DTC bench test

Send on `0x7E0`:

```text
03 19 02 FF
```

The empty demonstration DTC store replies on `0x7E8`:

```text
03 59 02 FF
```

Replace the demonstration store with application DTC records to obtain real
DTC lists. The same handler dispatches all 27 LINK ReadDTCInformation report
types.

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
