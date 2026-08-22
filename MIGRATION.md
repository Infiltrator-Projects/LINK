# LINK migration status

LINK is being populated incrementally from the historically separate MBLINK and JAGLINK implementations. Product-specific Mercedes and Jaguar behaviour is deliberately excluded.

The dependency hierarchy is:

```text
Infiltratr Common -> LINK -> MBLINK / JAGLINK
```

A component is promoted only after the product implementations are compared, the best generic behaviour is retained, product identity is removed from the shared implementation, existing Common facilities are reused where appropriate, and both products can consume the shared result without behavioural regression.

## Completed shared ownership

The following are implementation-complete in LINK 0.6.0:

- diagnostic workspace model;
- Classical-CAN ISO-TP;
- parameter definitions, keys and formatting;
- bounded parameter store/history;
- parameter scheduler;
- telemetry store and CSV writer;
- Discover deny-by-default safety classifier;
- JSON Lines evidence writer;
- Windows OpenPort 2.0/J2534 Discover scanner.

MBLINK and JAGLINK retain small product-prefixed compatibility/adaptor files where their existing public C APIs still need to delegate into LINK. Those files are not independent implementations.

Native iPhone builds compile the exact pinned LINK C sources required by the product target rather than carrying product-local copies.

## Remaining generic migration candidates

1. ELM327 transport/session/CAN/probe;
2. standard OBD-II;
3. UDS;
4. Apple BLE transport/controller glue;
5. shared Linux application structure;
6. shared iPhone application structure;
7. packaging/release helpers and common CI assertions.

Mercedes- and Jaguar-specific definitions and diagnostic behaviour remain in their product repositories.

## Completion rule

A migration is not considered complete merely because LINK contains a copy. LINK must be the source of truth, both products must consume it, duplicate generic implementation must be removed or reduced to compatibility delegation, tests must pass in both products, and the documentation must describe the resulting ownership accurately.

SPDX-License-Identifier: GPL-3.0-or-later
