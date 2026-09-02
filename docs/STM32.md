<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32 embedded target

LINK supports a bare-metal STM32 platform edge without making STM32Cube HAL a dependency of the portable core.

```text
STM32 application
      |
STM32Cube HAL / target CAN peripheral
      |
family-specific LinkStm32CanOps binding
      |
platform/stm32 bounded CAN queue + completion-aware UDS glue
      |
LINK ISO-TP
      |
LINK UDS / 27-service codecs
```

The platform boundary is split deliberately. `platform/stm32/` is HAL-independent and allocation-free. RX queue entries retain the interrupt-time millisecond tick, and only one hardware TX is outstanding at a time. ISO-TP separation/flow-control timing and UDS P2 timing are re-anchored to confirmed hardware transmission rather than FIFO admission.

The concrete STM32C092 binding lives in `examples/stm32c092/`. It uses the FDCAN Tx Event FIFO/message markers for real completion, treats lost Tx events as transport failure, and keeps STM32Cube types outside LINK's portable core. The shared STM32Cube bxCAN binding in `examples/stm32-bxcan/` provides the corresponding implementation for STM32F103, STM32F107 and STM32F767.

The first target is the STM32C092RCTx project supplied in MBLINK issue #27. The attachment is an ECU/server demonstration; LINK's reference target is intentionally a diagnostic tester/client that talks to a vehicle. The attachment's separate UDS implementation is not imported. That issue is now complete at the LINK layer: the C092 example uses LINK's shared ISO-TP/UDS implementation rather than creating an MBLINK-only MCU fork.

See `examples/stm32c092/README.md` for Cube/Keil integration, corrected 48 MHz/500 kbit/s nominal timing guidance and the read-only F190 VIN example.

## bxCAN families

The portable `platform/stm32` layer is not tied to FDCAN. A different STM32 family is supported by supplying the `LinkStm32CanOps` callbacks for its CAN peripheral:

- receive one CAN/CAN-FD frame from the target driver's RX path;
- report whether hardware transmission can be accepted;
- submit one frame;
- report actual TX completion/failure rather than only FIFO admission; and
- supply a wrapping millisecond clock such as `HAL_GetTick()`.

STM32F103, STM32F107 and STM32F767 use the older bxCAN peripheral rather than the C092 FDCAN block. `examples/stm32-bxcan/` now supplies one concrete CubeF1/CubeF7 HAL binding plus a compiled VIN-over-UDS tester example for all three families. It maps standard and extended Classical-CAN frames, installs exact standard-ID list filters, records real mailbox completion from the HAL callbacks and propagates mailbox abort or CAN error as a transport failure.

See `examples/stm32-bxcan/README.md` for exact source/include lists, the complete callback wiring, CAN1/CAN2 filter-bank ownership and separate F103/F107/F767 bit-timing starting points. This completes the implementation requested in LINK issue #4; targets with a different CAN HAL surface still provide their own `LinkStm32CanOps` binding.

## Host regression coverage

The STM32 test suite host-simulates the transport boundary and covers:

- wrapping millisecond clocks;
- bounded RX queue overflow accounting;
- actual TX completion timing;
- stuck/failing hardware transmission;
- Classical CAN and CAN-FD mapping, including the concrete bxCAN and FDCAN bindings;
- standard and extended CAN IDs;
- multi-frame ISO-TP/UDS transactions;
- flow-control overflow; and
- consecutive-frame sequence errors.

This proves the portable state machines plus the bxCAN and C092 binding contracts without pretending that CI can electrically validate a particular board/transceiver. The STM32 CI job independently compiles the bxCAN source set for Cortex-M3 and Cortex-M7 instruction targets.

Manufacturer UI and product presentation remain outside the MCU. MBLINK/JAGLINK can remain host applications while a dedicated interface runs LINK firmware on the MCU.
