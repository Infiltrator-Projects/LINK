# Reporter STM32C092 project fixes

This directory contains the exact source replacements derived from the STM32C092_UDS.zip supplied by chenyurong22 in the LINK/MBLINK issue threads.

Apply these files over the submitted project, preserving their paths. Unchanged STM32 HAL/CMSIS, Cube, KEIL and generated files are intentionally not duplicated here.

Changed files:
- Src/fdcan.c
- Src/main.c
- library/src/uds_dtc.c
- stm32c092/can_transport_fdcan.c
- stm32c092/uds_app_fdcan.c
- stm32c092/uds_app_fdcan.h

Hardware-driven corrections:
- 48 MHz / 500 kbit/s nominal FDCAN timing uses SJW 2 with TSEG2 2.
- Physical 0x7E0 and functional 0x7DF requests are accepted explicitly.
- RX uses an 8-frame ISR-facing FIFO plus an 8-frame deferred-request FIFO.
- FlowControl bypasses deferred ordinary requests during an active segmented response.
- The main loop drains FDCAN FIFO0 as a fallback when the RX callback is missed.
- TX completion is driven by matching FDCAN Tx Events, not by an idle-state guess.
- DLC conversion recognises valid lengths through 64 bytes.
- ReadDTCInformation request lengths are corrected for all 27 LINK catalogue subfunctions.
- F190 returns a deterministic 17-byte VIN.
- The demo DTC store contains 123456/09 and ABCDEF/08.
- Advanced 0x19 reports without backing snapshot, severity, extended-data, mirror, permanent, user-memory or WWH data return NRC 0x31 instead of fabricated positive payloads.

PCAN smoke vectors:
- 02 10 01 CC CC CC CC CC -> 06 50 01 00 32 01 F4 CC
- 03 19 02 FF CC CC CC CC -> segmented positive response beginning 59 02 FF 12 34 56 09; tester then sends 30 00 00 CC CC CC CC CC
- repeated 02 3E 00 CC CC CC CC CC requests are queued rather than silently discarded during an active response

Host verification performed before publication:
- changed application/STM32 C files pass C11 syntax checking against the supplied STM32C0 HAL/CMSIS headers;
- all 27 ReadDTCInformation subfunctions accept their corrected request length and reject one-byte-short/one-byte-long variants;
- the supplied UDS dispatcher was exercised for 10 01, positive two-DTC 19 02, advanced-report 7F 19 31 fallbacks, and invalid-length 7F 19 13.

The portable LINK implementation remains authoritative. This overlay exists so the reporter can retest the exact KEIL/Cube project they supplied without copying fixes out of issue comments.
