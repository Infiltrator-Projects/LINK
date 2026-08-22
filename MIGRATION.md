# Migration staging

This repository is being populated incrementally from MBLINK and JAGLINK.

Raw imports under `migration/` are temporary comparison snapshots only. They are not the final LINK API. Product-specific Mercedes and Jaguar code is intentionally excluded. Shared candidates are promoted into neutral `include/link`, `src`, `platform`, `app`, `tests`, and build/release paths only after comparison and neutralisation.

The dependency hierarchy remains: Infiltratr Common -> LINK -> MBLINK/JAGLINK.

## Completed shared ownership

- Discover deny-by-default safety classifier
- JSON Lines evidence writer
- Windows OpenPort 2.0/J2534 Discover scanner implementation
- top-level diagnostic workspace model

The workspace migration is complete at implementation level: `src/core/workspace.c` and `include/link/workspace.h` are the source of truth. MBLINK and JAGLINK retain only product-prefixed source compatibility aliases. Normal CMake builds consume the model through `LINK::Core`; native iPhone builds compile that exact source from the pinned LINK submodule instead of carrying a product-local copy.

## Remaining generic migration candidates

1. ISO-TP
2. parameter/store/scheduler/telemetry core
3. ELM327 transport/session/CAN/probe
4. OBD-II
5. UDS
6. shared Apple transport/controller glue
7. shared Linux application structure
8. shared iPhone application structure
9. packaging/release helpers

Mercedes- and Jaguar-specific definitions and behaviour remain in their product repositories.
