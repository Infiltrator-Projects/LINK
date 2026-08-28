<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32 embedded target

LINK supports a bare-metal STM32 platform edge without making STM32Cube HAL a dependency of the portable core.

```text
STM32 application
      |
STM32Cube HAL/FDCAN
      |
platform/stm32 bounded CAN queue + UDS transaction glue
      |
LINK ISO-TP
      |
LINK UDS / 27-service codecs
```

The platform boundary is deliberately split in two. `platform/stm32/` contains HAL-independent, allocation-free queue/timing/orchestration code that can be host-tested in normal LINK CI. A concrete MCU family example supplies the STM32Cube calls needed to turn FDCAN RX/TX and `HAL_GetTick()` into those generic operations.

The first concrete target is the STM32C092RCTx project supplied in MBLINK issue #27. See `examples/stm32c092/README.md` for the exact Cube/Keil integration and the read-only F190 VIN example.

This is a LINK platform port, not a second MBLINK protocol implementation. Manufacturer UI and product presentation remain outside the MCU. A future dedicated hardware interface may run LINK on the MCU while MBLINK/JAGLINK remain host applications.
