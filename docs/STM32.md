<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32 embedded target

LINK supports a bare-metal STM32 platform edge without making STM32Cube HAL a dependency of the portable core.

```text
STM32 application
      |
STM32Cube HAL/FDCAN
      |
platform/stm32 bounded CAN queue + completion-aware UDS glue
      |
LINK ISO-TP
      |
LINK UDS / 27-service codecs
```

The platform boundary is split deliberately. `platform/stm32/` is HAL-independent and allocation-free. RX queue entries retain the interrupt-time millisecond tick, and only one hardware TX is outstanding at a time. ISO-TP separation/flow-control timing and UDS P2 timing are re-anchored to confirmed hardware transmission rather than FIFO admission.

The concrete STM32C092 binding lives in `examples/stm32c092/`. It uses the FDCAN Tx Event FIFO/message markers for real completion, treats lost Tx events as transport failure, and keeps STM32Cube types outside LINK's portable core.

The first target is the STM32C092RCTx project supplied in MBLINK issue #27. The attachment is an ECU/server demonstration; LINK's reference target is intentionally a diagnostic tester/client that talks to a vehicle. The attachment's separate UDS implementation is not imported.

See `examples/stm32c092/README.md` for Cube/Keil integration, corrected 48 MHz/500 kbit/s nominal timing guidance and the read-only F190 VIN example.

Manufacturer UI and product presentation remain outside the MCU. MBLINK/JAGLINK can remain host applications while a future dedicated interface runs LINK firmware on the MCU.
