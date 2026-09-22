<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32F103 complete UDS ECU/server reference (issue #32)

This directory is the reference implementation requested in LINK issue #32 for
an STM32F103-class ECU with 512 KiB flash, 64 KiB SRAM and Classical CAN at
500 kbit/s. It builds on LINK's allocation-free ISO-TP, UDS server, execution
policy, AES-CMAC/SecurityAccess and STM32 bxCAN transport rather than importing
another protocol stack.

The portable ECU core is:

- `link-stm32f103-uds-ecu.h`
- `link-stm32f103-uds-ecu.c`

A Cube/HAL integration for the reporter's 512 KiB F103 layout is provided at:

- `issue-32/Src-main.c`

## What "complete" means here

The example wires every service in LINK's 27-service ISO 14229 catalogue into
a deterministic ECU/server surface. DiagnosticSessionControl, ECUReset,
SecurityAccess and TesterPresent use LINK's shared built-ins. The remaining
services are registered by the F103 ECU core.

| SID | Service | Reference behaviour |
| --- | --- | --- |
| 10 | DiagnosticSessionControl | LINK session state machine |
| 11 | ECUReset | deferred reset after positive response |
| 14 | ClearDiagnosticInformation | exact/prefix/all DTC clearing, persisted |
| 19 | ReadDTCInformation | all 27 LINK report types over rich DTC records |
| 22 | ReadDataByIdentifier | VIN, software version, persistent user DID, dynamic DID |
| 23 | ReadMemoryByAddress | bounded persistent sandbox |
| 24 | ReadScalingDataByIdentifier | bounded reference scaling record |
| 27 | SecurityAccess | LINK sequencing + target AES-CMAC verification hook |
| 28 | CommunicationControl | state recorded persistently |
| 29 | Authentication | deAuthenticate profile; unsupported profiles return 0x12 |
| 2A | ReadDataByPeriodicIdentifier | deterministic periodic reference data |
| 2C | DynamicallyDefineDataIdentifier | define/clear one persistent dynamic DID |
| 2E | WriteDataByIdentifier | persistent user DID |
| 2F | InputOutputControlByIdentifier | bounded reference control/status echo |
| 31 | RoutineControl | integrity-check routine |
| 34 | RequestDownload | opens a bounded programming-sandbox transfer |
| 35 | RequestUpload | opens a bounded upload from the same sandbox |
| 36 | TransferData | block-sequenced upload/download |
| 37 | RequestTransferExit | validates completion and commits downloads |
| 38 | RequestFileTransfer | bounded virtual-file negotiation surface |
| 3D | WriteMemoryByAddress | bounded persistent sandbox write |
| 3E | TesterPresent | LINK built-in |
| 83 | AccessTimingParameter | exposes active P2/P2* values |
| 84 | SecuredDataTransmission | security-gated bounded record transport |
| 85 | ControlDTCSetting | persistent on/off control |
| 86 | ResponseOnEvent | bounded event-control acknowledgement |
| 87 | LinkControl | verifies link-control modes; runtime baud transition is not faked |

This is a **reference ECU**, not a production bootloader. Complex services are
implemented with bounded demonstrator semantics rather than pretending a
generic library knows a product's certificate PKI, actuator map, filesystem,
signed firmware format or live CAN reconfiguration procedure.

## Flash storage and power-loss model

The F103 reference uses a two-page alternating journal. Each page stores one
complete state image with a magic value, schema, monotonically increasing
generation and CRC-32. A new state is written only after the target page is
erased; it is then read back and CRC-verified before becoming active. On boot,
the newest valid page wins. If one page is torn/corrupt, the previous valid
generation remains usable.

For the requested 512 KiB device the supplied Cube example reserves the final
two 2 KiB pages:

- page A: `0x0807F000`
- page B: `0x0807F800`

The application linker region must end before `0x0807F000`. For an image
starting at `0x08000000`, reserve the final 4 KiB (usable application flash
length `0x7F000`).

Persistent state includes DTC status/FDC/permanent state, snapshot/stored/
extended-data fixtures, a writable DID, dynamic DID definition, DTC-setting and
communication-control state, plus a 256-byte programming/data sandbox.

## Programming safety boundary

RequestDownload/Upload, TransferData, TransferExit and WriteMemoryByAddress are
deliberately confined to the 256-byte data sandbox. They never write the vector
table, application image or bootloader. A real bootloader must replace that
handler with its own signed-image, rollback, range and power-loss policy.

The execution-policy table also requires Programming session + SecurityAccess
level 1 + physical addressing for programming services. ClearDiagnosticInformation
and other state-changing services are likewise unavailable from the default
session.

The example key in `issue-32/Src-main.c` is the public RFC 4493 test key and is
**demonstration material only**. Replace it with target-owned key material or a
hardware-backed verifier. LINK does not contain a Mercedes, Jaguar or other OEM
seed/key algorithm.

## CAN/Cube configuration

For the reporter's board:

- STM32F103, 512 KiB flash / 64 KiB SRAM;
- CAN1 RX PA11, TX PA12;
- Classical CAN, 500 kbit/s;
- physical request `0x7E0`, response `0x7E8`;
- functional request `0x7DF`;
- 8-byte frames padded with `0xCC`.

At a 36 MHz CAN clock, a verified starting point is prescaler 4, BS1 15 TQ,
BS2 2 TQ, SJW 1 TQ (18 TQ/bit = 500 kbit/s). Recalculate if the actual APB1
clock differs.

The Cube project needs only the CAN RX FIFO0 message-pending interrupt. The
supplied `issue-32/Src-main.c` forwards that one receive callback; TX
completion and failure are recovered by polling bxCAN's latched mailbox result
flags. TX-complete callbacks remain an optional precision path, not a
requirement for the reference ECU.

## Verification

`tests/test_stm32f103_uds_ecu.c` uses a byte-accurate two-page flash emulator
and proves:

- all 27 service SIDs have a deterministic positive reference path;
- 0x27 uses a real AES-CMAC key verification round trip;
- 0x14 clearing updates the DTC model and survives a simulated reboot;
- writable DID and memory data survive re-initialisation from flash;
- upload/download block sequencing works;
- unsafe state-changing/programming requests are rejected in Default session;
- journal writes obey erase-before-program semantics.

The normal LINK STM32 transport tests continue to cover ISO-TP, physical versus
functional addressing, deferred frames and bxCAN HAL completion. CI also
cross-compiles the F103 ECU core for Cortex-M3 with `-ffreestanding`.
