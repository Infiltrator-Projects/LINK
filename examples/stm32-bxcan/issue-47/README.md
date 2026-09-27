# STM32F103-V3 UDS ECU integration (LINK #47)

The original `STM32F103-CANV2.zip` has no LINK, UDS, ISO-TP or flash journal
source; it only sends raw CAN `0x777`. The newer `STM32F103-V3.zip` in the issue
comments **does** compile LINK and has a UDS server. Its `uds_init()` starts
CAN before `link_stm32f103_uds_ecu_init()`, so an initial flash write can stall
the newly active CAN receive path. Its Keil IROM region also includes the two
pages reserved for the persistent journal, which is unsafe for future growth.

For the newer V3 Cube project, use `Src-main.c` here in place of
`Core/Src/main.c`. Its `can.c` already exports `hcan`, configures CAN1 on
PA11/PA12 for 500 kbit/s, and provides `MX_CAN_Init()`. Remove the old
`CAN_Handler_Init()` call and `CAN_TX(0x777,...)` loop by replacing main.
Keep the old `HAL_CAN_RxFifo0MsgPendingCallback` in `Core/Src/can.c` disabled;
the LINK main supplies the active callback and feeds queued frames to ISO-TP.
The old `CAN_Handler_Init()` may remain unused, but do not call it: the LINK
adapter installs its own filters and starts CAN after loading flash state.

Update the vendored LINK source to the current revision and add the new
`LINK/src/core/flash_journal.c` to the existing Keil source list. Keep the
existing ISO-TP, UDS, server, DTC lifecycle and STM32 bxCAN sources. Point the
Keil main entry at this file, or copy its contents over the old main. Reserve
the final two 2 KiB pages by ending the
application ROM region at `0x0807F000` (IROM1 start `0x08000000`, size
`0x0007F000`). If the device or linker map differs, change the reserved page
addresses and flash bounds before flashing; never erase a page containing
the application. Keep Cube's clock, GPIO, CAN and interrupt startup files.

The ECU then accepts physical requests on `0x7E0`, functional requests on
`0x7DF`, and sends physical responses on `0x7E8`. CAN still requires a working
transceiver, matching bitrate and a second node to acknowledge frames.
