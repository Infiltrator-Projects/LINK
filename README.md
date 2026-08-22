# LINK

LINK is the shared application engine for the LINK vehicle-diagnostics family.

MBLINK and JAGLINK are products built from this same engine. They must not carry independent copies of generic application code.

## Architectural rule

If a change applies to both products, the implementation belongs in LINK.

MBLINK owns only Mercedes-specific material such as branding, icons, product metadata, Mercedes ECU/protocol definitions, and Mercedes-specific diagnostic behaviour.

JAGLINK owns only Jaguar-specific material such as branding, icons, product metadata, Jaguar ECU/protocol definitions, and Jaguar-specific diagnostic behaviour.

Everything else belongs here: portable core logic, transports, ELM327, ISO-TP, OBD-II, UDS, telemetry, parameter scheduling, discovery, safety classification, evidence/logging, OpenPort/J2534 support, Linux/Windows/iOS platform glue, shared UI structure, tests, packaging helpers, and release/build machinery.

## Product model

```text
LINK shared engine
  + MBLINK face + Mercedes definitions
  = MBLINK

LINK shared engine
  + JAGLINK face + Jaguar definitions
  = JAGLINK
```

The intended end state is that MBLINK and JAGLINK contain only a small product layer, ideally around 5–10 product-specific files or small directories plus their README/version metadata.

## Migration policy

Migration is incremental and test-preserving. A component moves into LINK only after the corresponding MBLINK and JAGLINK implementations are compared, the best generic behaviour is retained, product names are removed from the shared implementation, and both products can consume the shared result.

Initial migration order:

1. parameter/store/scheduler/telemetry core
2. ISO-TP
3. ELM327
4. OBD-II and UDS
5. discovery, safety and evidence
6. OpenPort/J2534 and platform transports
7. Apple, Linux and Windows shared application glue
8. shared UI structure
9. packaging, CI and release machinery

Mercedes and Jaguar vehicle-specific code remains outside LINK.

## Source of truth

LINK is the source of truth for shared behaviour. Product repositories should pin a known LINK revision rather than copying LINK files into their own trees.

SPDX-License-Identifier: GPL-3.0-or-later
