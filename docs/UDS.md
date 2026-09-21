<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# UDS service coverage

LINK owns the product-neutral ISO 14229 request/response layer shared by all
LINK-family products. `include/link/uds.h` retains the typed core client,
session, TesterPresent and ReadDataByIdentifier APIs. `include/link/uds_dtc.h`
provides the read-only ReadDTCInformation helper. `include/link/uds_services.h`
declares the complete standard service catalogue and bounded codec API; the
implementation is compiled once in `src/uds/uds_services.c` as part of
`LINK::Core`.

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

The service API supplies named builders for services that were not already typed
in `uds.h` or `uds_dtc.h`. The common helpers cover:

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

## Server execution policy

The portable UDS server can optionally consume a caller-owned
`LinkUdsServerPolicy` table. Policy remains product-neutral: LINK supplies the
enforcement mechanism while the ECU/application supplies the actual rules.

A service-wide entry can restrict a SID by:

- the four standard session bits (Default, Programming, Extended Diagnostic and
  Safety System Diagnostic);
- a 64-level security mask, where level 0 represents the locked/unsecured state;
- physical addressing, functional addressing, or both.

A subfunction-specific entry for a service that uses subfunctions overrides the
service-wide entry for that exact 7-bit subfunction. Duplicate policy keys,
unknown services, impossible masks and subfunction rules attached to services
without subfunctions are rejected during server initialization.

`link_uds_server_handle()` remains source-compatible and treats direct callers
as physically addressed. Transports that know the addressing context use
`link_uds_server_handle_with_context()`; the STM32 server path now passes
physical versus functional addressing explicitly.

Policy rejection occurs before the application handler runs. A disallowed
service/subfunction session produces NRC `0x7F`/`0x7E`, a disallowed
addressing form produces `0x11`/`0x12`, and an otherwise valid request whose
active security level is not permitted produces `0x33`. Functional-address
transport rules may then suppress the standard negative-response classes as
required by the transport/application contract.

Security state is explicit server state rather than an embedded key algorithm.
A successful product SecurityAccess handler can call
`link_uds_server_set_security_level()`; LINK does not invent OEM seed/key
logic. By default a real session change, S3 fallback to Default, explicit server
session reset and ECU reset return the active security level to 0.

## SecurityAccess hook and AES-CMAC

LINK provides an allocation-free AES-128/AES-CMAC primitive in
`link/aes_cmac.h`. Its deterministic tests use the published RFC 4493/NIST
AES zero-block and 0-, 16-, 40- and 64-byte CMAC vectors. The primitive is
protocol-neutral: availability does not assert that any vehicle or ECU uses
CMAC for SecurityAccess.

The UDS server may optionally own the generic `0x27 SecurityAccess` state
machine through `LinkUdsSecurityAccessConfig`. A target supplies a seed
callback and a key-verification callback. LINK maps requestSeed/sendKey pairs
to ordinal security levels 1 through 63, enforces request-seed-before-send-key
sequencing, updates the active security level only after successful
verification, and optionally owns invalid-key attempt/delay state.

Generic rejection uses NRC `0x24` for sequence errors, `0x35` for an
invalid key, `0x36` when the configured attempt limit is reached, and
`0x37` while the delay remains active. Session transitions clear any pending
seed/key sequence but do not erase an active lockout timer.

A custom `0x27` handler still overrides the built-in facility and can call
`link_uds_server_set_security_level()` after its own verification. No
Mercedes, Jaguar or other OEM seed/key algorithm is embedded in LINK.

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

## ReadDTCInformation conformance boundary

The complete 27-report implementation matrix is maintained in
[`UDS-READ-DTC-CONFORMANCE.md`](UDS-READ-DTC-CONFORMANCE.md). The matrix
separates the fixed ISO-shaped envelope that LINK can validate generically from
snapshot and extended-data tails whose record sizes depend on ECU/application
definitions.

LINK exposes typed, allocation-free views for fixed-width DTC/status,
snapshot-identification, severity, fault-detection-counter and WWH severity
record families. Variable snapshot and extended-data payloads remain bounded raw
spans until a higher layer supplies the DID/data-length knowledge needed to
interpret them without guessing.

For transaction-aware callers, `link_uds_decode_read_dtc_information_response_for_request()`
adds a second validation layer using the exact request that was transmitted.
It rejects mismatched MemorySelection, record-number, functional-group and
requested-DTC echoes. This prevents malformed responses with omitted fields
from being accepted merely because the remaining bytes form a structurally
plausible envelope.

The decoder rejects truncated positive responses that omit a mandatory
DTCAndStatusRecord for reports `0x04`, `0x06`, `0x10`, `0x18` or
`0x19`. User-defined-memory reports `0x18` and `0x19` additionally
require the memory-selection echo before that fixed DTC-and-status envelope.

## Tests

`tests/test_uds.c` verifies:

- exactly 27 unique registered service identifiers;
- request bytes for every service family added by issue #2;
- memory address/size width validation and ALFID encoding;
- response echo validation for subfunction, DID, routine and transfer services;
- bounded-buffer and malformed-argument failure behaviour; and
- preservation of the pre-existing UDS client/session tests; and
- RFC 4493 AES-CMAC vectors, invalid arguments and tag verification.

`tests/test_uds_server.c` additionally verifies built-in SecurityAccess
requestSeed/sendKey sequencing, a CMAC-backed sample verifier, invalid-key
attempt counting, delay expiry, NRC selection and configuration rejection. It
also verifies service-wide and exact
subfunction policy precedence, Default/Programming/Extended session gating,
security-level gating, physical/functional addressing, the expected UDS NRC for
each rejection class, security reset on session change, invalid policy-table
rejection, and backward-compatible unrestricted operation when no policy table
is configured.

`tests/test_discover_safety.c` independently proves that adding those codecs
does not broaden the Discover transmit allowlist.

The installed-package consumer test also verifies that the exported `LINK::Core`
target supplies this compiled service implementation and the 64-byte CAN-FD
ISO-TP contract outside the LINK source tree.

This documentation is an implementation map, not a reproduction of ISO 14229.
For normative protocol requirements, use the applicable licensed standard.
