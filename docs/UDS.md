<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# UDS service coverage

LINK owns the product-neutral ISO 14229 request/response layer shared by MBLINK
and JAGLINK. `include/link/uds.h` retains the typed core client, session,
TesterPresent and ReadDataByIdentifier APIs. `include/link/uds_dtc.h` provides
the read-only ReadDTCInformation helper. `include/link/uds_services.h` completes
the standard service catalogue and supplies bounded request codecs for the
remaining services.

The codec layer only serializes or validates diagnostic PDUs. It does not send
them. Complex records whose meaning is ECU- or application-specific remain raw
caller-owned byte spans rather than being given invented generic semantics.

## Standard service catalogue

| SID | Service | LINK effect classification |
| --- | --- | --- |
| `0x10` | DiagnosticSessionControl | session control |
| `0x11` | ECUReset | state-changing |
| `0x14` | ClearDiagnosticInformation | state-changing |
| `0x19` | ReadDTCInformation | read-only |
| `0x22` | ReadDataByIdentifier | read-only |
| `0x23` | ReadMemoryByAddress | read-only |
| `0x24` | ReadScalingDataByIdentifier | read-only |
| `0x27` | SecurityAccess | security |
| `0x28` | CommunicationControl | state-changing |
| `0x29` | Authentication | security |
| `0x2A` | ReadDataByPeriodicIdentifier | read-only |
| `0x2C` | DynamicallyDefineDataIdentifier | state-changing |
| `0x2E` | WriteDataByIdentifier | state-changing |
| `0x2F` | InputOutputControlByIdentifier | state-changing |
| `0x31` | RoutineControl | state-changing |
| `0x34` | RequestDownload | programming |
| `0x35` | RequestUpload | programming |
| `0x36` | TransferData | programming |
| `0x37` | RequestTransferExit | programming |
| `0x38` | RequestFileTransfer | programming |
| `0x3D` | WriteMemoryByAddress | state-changing |
| `0x3E` | TesterPresent | session control |
| `0x83` | AccessTimingParameter | state-changing |
| `0x84` | SecuredDataTransmission | security |
| `0x85` | ControlDTCSetting | state-changing |
| `0x86` | ResponseOnEvent | state-changing |
| `0x87` | LinkControl | state-changing |

`link_uds_standard_service_count()` is fixed at 27 for this catalogue, and
`link_uds_standard_service_find()` provides metadata without requiring products
to duplicate service-ID tables.

## Request codecs

`uds_services.h` adds named builders for the services that were not already
typed in `uds.h` or `uds_dtc.h`. The common helpers cover:

- registered raw service records;
- standard subfunction encoding including the suppress-positive-response bit;
- DID-prefixed requests and DID echo validation;
- AddressAndLengthFormatIdentifier packing for memory read/write and
  download/upload requests;
- DTC group clearing, including optional memory selection;
- periodic-data request modes;
- routine-control identifier and option records;
- transfer-data block counters and response echoes; and
- bounded positive-response helpers for subfunction, DID, routine, transfer and
  empty-response services.

No helper allocates memory. Callers own every input/output buffer and raw record.

## Safety boundary

Codec availability is not authorization.

Discover remains deny-by-default. Its bounded inventory continues to allow only
the deliberately enabled OBD reads plus UDS `ReadDTCInformation (0x19)` and
`ReadDataByIdentifier (0x22)`. New codecs do not automatically enable
`ReadMemoryByAddress`, periodic reads, TesterPresent, or any state-changing
service.

The safety classifier explicitly blocks ECU reset, DTC clearing, security and
authentication, secured data, write/control services, routines, programming
transfer services and file transfer. Unknown services remain blocked by the
default rule.

An owning product may add a narrower policy only when it has a concrete use
case, ECU-specific validation and appropriate operator controls. The shared
LINK codec layer itself does not contain a bypass.

## Tests

`tests/test_uds.c` verifies:

- exactly 27 unique registered service identifiers;
- request bytes for every service family added by issue #2;
- memory address/size width validation and ALFID encoding;
- response echo validation for subfunction, DID, routine and transfer services;
- bounded-buffer and malformed-argument failure behaviour; and
- preservation of the pre-existing UDS client/session tests.

`tests/test_discover_safety.c` independently proves that adding those codecs
does not broaden the Discover transmit allowlist.

This documentation is an implementation map, not a reproduction of ISO 14229.
For normative protocol requirements, use the applicable licensed standard.
