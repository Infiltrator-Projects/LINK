# STM32C092 port for MBLINK issue #27

This directory is the concrete integration for the STM32Cube/Keil project attached to `Infiltrator-Projects/MBLINK#27`.

The attached project was already an STM32C092 FDCAN application, but its local `library/` and `stm32c092/` code implemented a UDS ECU/server. MBLINK/LINK is the diagnostic tester/client. The literal port therefore replaces the project-local UDS server entry points with LINK's tested STM32C092 diagnostic client edge.

## What to keep from the attached project

Keep the Cube-generated platform files and configuration:

- `Drivers/`
- `Inc/`
- `Src/fdcan.c`
- `Src/gpio.c`
- `Src/stm32c0xx_hal_msp.c`
- `Src/stm32c0xx_it.c`
- `Src/system_stm32c0xx.c`
- `Src/usart.c`
- `FDCAN_Classic_Frame_Networking.ioc`
- the startup file and linker/device configuration in `MDK-ARM/`

The existing FDCAN peripheral initialization remains owned by CubeMX.

## Replace the old application glue

Do not compile the attached project's old UDS server files for this tester/client build:

- `library/src/endpoint.c`
- `library/src/isotp.c`
- `library/src/uds.c`
- `library/src/uds_did.c`
- `library/src/uds_download.c`
- `library/src/uds_dtc.c`
- `library/src/uds_services.c`
- `stm32c092/can_transport_fdcan.c`
- `stm32c092/uds_app_fdcan.c`
- `stm32c092/uds_platform_fdcan.c`

Those files are a separate ECU/server implementation and would duplicate/conflict with LINK's ISO-TP/UDS tester stack.

Use `Src-main.c` from this directory as the application-side replacement for the attached project's `Src/main.c`. It preserves the generated clock/peripheral setup but delegates FDCAN receive, transmit-event completion, timing and UDS processing to LINK.

## LINK sources to add to the Keil target

Add these LINK implementation files to the target:

- `src/core/isotp.c`
- `src/uds/uds.c`
- `src/uds/uds_services.c`
- `platform/stm32/link-stm32-can.c`
- `platform/stm32/link-stm32-uds.c`
- `examples/stm32c092/link-stm32c092-hal.c`
- `examples/stm32c092/link-stm32c092-example.c`

Add these include directories:

- `include`
- `platform/stm32`
- `examples/stm32c092`
- the attached project's existing `Inc`
- the existing STM32C0 HAL/CMSIS include directories already present in the Keil project

The concrete HAL adapter uses the submitter's requested primitives directly:

- RX FIFO 0 through `HAL_FDCAN_GetRxMessage()`
- TX FIFO through `HAL_FDCAN_AddMessageToTxFifoQ()`
- TX event FIFO for real completion tracking
- `HAL_GetTick()` as the 1 ms clock

## Behaviour of this port

On startup the board configures a physical ISO-TP route `0x7E0 -> 0x7E8`, starts LINK's STM32 client and sends UDS `ReadDataByIdentifier (0x22)` for VIN DID `0xF190`.

Call `link_stm32c092_example_process()` continuously from the main loop. When the transaction completes:

- `link_stm32c092_example_state()` becomes `LINK_STM32C092_EXAMPLE_VIN_READY` on success;
- `link_stm32c092_example_vin()` returns the 17-character VIN;
- a UDS negative response is exposed through `link_stm32c092_example_negative_response_code()`;
- dropped ISR frames are exposed through `link_stm32c092_example_dropped_frames()`.

This proves the literal port path without copying MBLINK's phone/desktop UI into an MCU. The reusable protocol implementation remains LINK-owned; the attached STM32 project is the concrete hardware target.

## Hardware boundary

LINK's host regression tests cover the C092 HAL mapping, queue overflow, timer wrap, multi-frame UDS, 29-bit/CAN-FD paths and transmit completion/loss handling. The final remaining validation is on the submitter's physical STM32C092 board/transceiver and target ECU.