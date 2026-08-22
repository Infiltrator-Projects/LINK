# LINK

LINK is the shared C11 vehicle-diagnostics/application engine used by MBLINK and JAGLINK.

## Dependency hierarchy

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns portable facilities that are useful outside vehicle diagnostics. LINK owns automotive functionality that is shared by MBLINK and JAGLINK. The product repositories own branding, product metadata and genuinely manufacturer-specific definitions/behaviour.

The dependency direction is one-way: Common must not depend on LINK or either product; LINK may depend on Common but never on a product; products consume LINK and must not fork LINK-owned behaviour.

## Current shared implementation

LINK 0.6.0 currently owns:

- the diagnostic workspace model;
- Classical-CAN ISO-TP;
- diagnostic parameter definitions, keys, formatting and bounded parameter history;
- parameter scheduling;
- telemetry storage and CSV recording;
- Discover deny-by-default safety classification;
- JSON Lines diagnostic evidence writing and operator annotations;
- the Windows OpenPort 2.0/J2534 Discover scanner.

The Windows scanner is the reference product-face pattern. MBLINK and JAGLINK compile the same `platform/windows/link-discover.c` and `LINK::Core`; only product identity and resources differ.

## Remaining migration

The next generic automotive blocks to promote into LINK are:

1. ELM327 command/parser/session/CAN/probe support;
2. standard OBD-II;
3. UDS;
4. shared Apple transport/controller glue;
5. shared Linux and iPhone application structure where behaviour is genuinely common;
6. packaging, CI and release helpers.

Manufacturer-specific Mercedes and Jaguar definitions remain outside LINK.

## Engineering rules

- C is the default implementation language; C++ is appropriate where it materially improves the design.
- Platform-required languages belong at narrow UI/interop edges rather than becoming alternate protocol engines.
- Public APIs document ownership, lifetime, failure behaviour, invariants and non-obvious constraints.
- Comments explain rationale and contracts; they do not narrate obvious syntax.
- Product identity is data/configuration. It must not create a second implementation of shared behaviour.
- Unknown or unsafe diagnostic operations remain denied by default.
- Generic improvements useful beyond automotive diagnostics should be promoted to Infiltratr Common rather than duplicated here.

See `MIGRATION.md`, `docs/DISCOVER.md` and `docs/PRODUCT_FACES.md` for the migration and product-face contracts.

SPDX-License-Identifier: GPL-3.0-or-later
