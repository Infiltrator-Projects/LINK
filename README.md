# LINK

LINK is the shared application engine for the LINK vehicle-diagnostics family.

MBLINK and JAGLINK are products built from this same engine. They must not carry independent copies of generic application code.

## Dependency hierarchy

LINK reuses Infiltratr Common wherever possible. LINK must not duplicate facilities that already belong in Common, and generic improvements that are useful beyond vehicle diagnostics should be promoted into Common instead of being implemented privately here.

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns broadly reusable portable primitives and contracts used across projects.

LINK owns vehicle-diagnostics/application behaviour shared by MBLINK and JAGLINK.

MBLINK and JAGLINK own only their product face and vehicle-specific definitions/behaviour.

## Architectural rule

If a change applies to multiple unrelated projects, it probably belongs in Infiltratr Common.

If a change applies to both MBLINK and JAGLINK, but is specifically part of the vehicle-diagnostics application, it belongs in LINK.

If a change applies only to Mercedes or only to Jaguar, it stays in the corresponding product repository.

MBLINK owns only Mercedes-specific material such as branding, icons, product metadata, Mercedes ECU/protocol definitions, and Mercedes-specific diagnostic behaviour.

JAGLINK owns only Jaguar-specific material such as branding, icons, product metadata, Jaguar ECU/protocol definitions, and Jaguar-specific diagnostic behaviour.

Everything else belongs here unless Common already provides it: portable diagnostics logic, transports, ELM327, ISO-TP, OBD-II, UDS, telemetry, parameter scheduling, discovery, safety classification, evidence/logging, OpenPort/J2534 support, Linux/Windows/iOS platform glue, shared UI structure, tests, packaging helpers, and release/build machinery.

## One implementation, multiple faces

Product identity must never create a second implementation of shared behaviour.

The current Discover/OpenPort model is the reference pattern:

```text
LINK Discover + MBLINK face = MBLINK Discover
LINK Discover + JAGLINK face = JAGLINK Discover

LINK Windows scanner + MBLINK face = mblink-discover.exe
LINK Windows scanner + JAGLINK face = jaglink-discover.exe
```

Both Windows executables are built from the same `platform/windows/link-discover.c` and the same `LINK::Core`. Only product-facing identity changes.

See `docs/DISCOVER.md` for the Discover/scanner contract and `docs/PRODUCT_FACES.md` for the general face-only architecture rule.

## Product model

```text
Infiltratr Common
  + LINK shared diagnostics engine
  + MBLINK face + Mercedes definitions
  = MBLINK

Infiltratr Common
  + LINK shared diagnostics engine
  + JAGLINK face + Jaguar definitions
  = JAGLINK
```

The intended end state is that MBLINK and JAGLINK contain only a small product layer, ideally around 5–10 product-specific files or small directories plus their README/version metadata.

## Migration policy

Migration is incremental and test-preserving. A component moves into LINK only after the corresponding MBLINK and JAGLINK implementations are compared, the best generic behaviour is retained, product names are removed from the shared implementation, existing Infiltratr Common facilities are reused wherever appropriate, and both products can consume the shared result.

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

Infiltratr Common is the source of truth for reusable cross-project primitives. LINK is the source of truth for shared MBLINK/JAGLINK vehicle-diagnostics behaviour. Product repositories should pin known Common and LINK revisions rather than copying those implementations into their own trees.

SPDX-License-Identifier: GPL-3.0-or-later
