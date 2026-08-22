# LINK

LINK is the shared C11 vehicle-diagnostics/application engine used by MBLINK and JAGLINK.

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns portable facilities useful outside vehicle diagnostics. LINK owns automotive functionality shared by the products. Product repositories own branding, metadata and genuinely manufacturer-specific behaviour.

## LINK 0.7.0 shared implementation

LINK now owns:

- platform-neutral byte-stream transport ABI;
- complete ELM327 command/parser/init engine;
- ELM327 adapter/protocol probing;
- ELM327-managed ISO 15765 CAN channel;
- transport-backed ELM327 command session;
- diagnostic workspace model;
- Classical-CAN ISO-TP;
- parameter definitions/store/history, scheduler and telemetry/CSV;
- Discover safety/evidence;
- Windows OpenPort 2.0/J2534 Discover scanner.

The ELM327 migration preserves the existing product-prefixed public API through thin compatibility aliases/wrappers while removing the duplicated algorithm from MBLINK and JAGLINK.

## Remaining migration

1. standard OBD-II;
2. UDS;
3. shared Apple transport/controller glue;
4. genuinely shared Linux/iPhone application structure;
5. packaging/CI/release helpers.

## Engineering rules

C is the default implementation language; C++ is used where it materially improves a design. Platform-required languages remain narrow UI/interop edges. Public APIs document ownership, lifetime, failure behaviour and invariants; source comments explain rationale and non-obvious state-machine constraints without narrating obvious syntax.

SPDX-License-Identifier: GPL-3.0-or-later
