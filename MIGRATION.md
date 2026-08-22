# LINK migration status

Dependency hierarchy: `Infiltratr Common -> LINK -> MBLINK / JAGLINK`.

## Completed shared ownership through LINK 0.7.0

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
- transport-backed ELM327 command session.

Product-prefixed files may remain only as compatibility aliases/wrappers. They must not contain a second implementation of LINK-owned behaviour.

## Remaining generic migration candidates

1. standard OBD-II;
2. UDS;
3. Apple transport/controller glue;
4. shared Linux application structure;
5. shared iPhone application structure;
6. packaging/release helpers and common CI assertions.

A migration is complete only when LINK is the source of truth, both products consume it, duplicate implementation is removed, regression tests pass, and documentation accurately records ownership.

SPDX-License-Identifier: GPL-3.0-or-later
