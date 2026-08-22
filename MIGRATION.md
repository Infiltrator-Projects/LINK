# LINK migration status

Dependency hierarchy: `Infiltratr Common -> LINK -> MBLINK / JAGLINK`.

## Completed shared ownership through LINK 0.8.0

- diagnostic workspace;
- Classical-CAN ISO-TP;
- parameter definitions/store/history;
- scheduler;
- telemetry/CSV;
- Discover safety/evidence;
- Windows OpenPort/J2534 scanner;
- byte-stream transport ABI;
- ELM327 command framing/parser/init;
- ELM327 adapter/protocol probe;
- ELM327-managed CAN channel;
- transport-backed ELM327 command session;
- standard SAE OBD-II request/response, PID, readiness, VIN and DTC engine;
- ISO 14229 UDS request/response, DID and client-state engine.

Product-prefixed files may remain only as compatibility aliases/wrappers. They must not contain a second implementation of LINK-owned behaviour.

## Remaining generic migration candidates

1. C/C++ portable diagnostic-session orchestration currently embedded in Apple controllers;
2. Apple BLE transport adapter glue, kept thin around the C/C++ transport/session core;
3. shared Linux application structure;
4. shared iPhone application structure, with Swift limited to presentation/platform glue;
5. packaging/release helpers and common CI assertions.

Vehicle-specific Mercedes and Jaguar definitions, probes and presentation assets stay in their product repositories.

A migration is complete only when LINK is the source of truth, both products consume it, duplicate implementation is removed, regression tests pass, and documentation accurately records ownership.

SPDX-License-Identifier: GPL-3.0-or-later
