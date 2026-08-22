# LINK migration status

Dependency hierarchy: `Infiltratr Common -> LINK -> MBLINK / JAGLINK`.

## Completed shared ownership through LINK 0.9.1

- diagnostic workspace;
- Classical-CAN ISO-TP;
- parameter definitions/store/history;
- scheduler;
- telemetry/CSV;
- Discover safety/evidence;
- Windows OpenPort/J2534 scanner and native Windows shell;
- byte-stream transport ABI;
- ELM327 command framing/parser/init;
- ELM327 adapter/protocol probe;
- ELM327-managed CAN channel;
- transport-backed ELM327 command session;
- standard SAE OBD-II request/response, PID, readiness, VIN and DTC engine;
- ISO 14229 UDS request/response, DID and client-state engine;
- read-only UDS ReadDTCInformation codec and DTC status helpers;
- portable product-neutral diagnostic-flow orchestration used by the Apple controllers, including the manufacturer-extension boundary.

Product-prefixed files may remain only as compatibility aliases/wrappers. They must not contain a second implementation of LINK-owned behaviour.

## Remaining generic migration candidates

1. reusable Apple BLE recovery/discovery policy behind a platform-neutral C transport coordinator;
2. shared Linux application structure;
3. shared iPhone application structure, with Swift/Objective-C limited to presentation/platform glue;
4. packaging/release helpers and common CI assertions.

Vehicle-specific Mercedes and Jaguar definitions, probes and presentation assets stay in their product repositories.

A migration is complete only when LINK is the source of truth, both products consume it, duplicate implementation is removed, regression tests pass, and documentation accurately records ownership.

SPDX-License-Identifier: GPL-3.0-or-later
