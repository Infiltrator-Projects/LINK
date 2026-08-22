<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK Product Face Contract

MBLINK and JAGLINK are not separate applications that happen to share some libraries. They are two branded vehicle products built from the same LINK application engine.

The architectural target is:

```text
Infiltratr Common
        |
       LINK
      /    \
 MBLINK   JAGLINK
  face      face
 +Mercedes +Jaguar
```

## LINK owns behaviour

Anything behavioural that should work the same in both products belongs in LINK unless it is generic enough to belong in Infiltratr Common.

Examples include protocol engines, discovery, evidence/logging, transports, shared UI structure, platform glue, packaging helpers and common build/release behaviour.

## Product repositories own identity and vehicle specificity

A product repository may own:

- its name, executable names and bundle/application identifiers;
- its icon/emblem and other brand assets;
- its README/version/release identity;
- Mercedes-only or Jaguar-only ECU definitions and diagnostic behaviour;
- minimal compatibility façades needed while old product-prefixed APIs are migrated.

A product repository must not own a copied generic LINK implementation.

## Face-only rule

Two product files that differ only because one says MBLINK and the other says JAGLINK are candidates to become one LINK file with face parameters.

A product-specific file is justified only when changing it for one vehicle family would genuinely be wrong for the other.

Branding is data. Behaviour is code. Branding differences must not create behavioural forks.

## Build rule

LINK should expose constructors/helpers that accept product identity and build the common implementation under the requested product name. Product CMake files should select the face rather than duplicate targets or source files.

For example, the Windows Discover scanner is compiled once in source terms from `platform/windows/link-discover.c`; `mblink-discover.exe` and `jaglink-discover.exe` are two outputs of that one implementation.

## Dependency rule

LINK reuses Infiltratr Common wherever possible. If logic is broadly reusable outside the vehicle-diagnostics family, it should move downward into Common rather than be duplicated in LINK.

The intended dependency graph is therefore:

```text
MBLINK -> LINK -> Infiltratr Common
JAGLINK -> LINK -> Infiltratr Common
```

During migration a product may still load Common directly for code not yet promoted into LINK, but the end state should avoid parallel copies and duplicated ownership.

## Regression test

When reviewing a change, ask:

1. Would both MBLINK and JAGLINK want this behaviour? If yes, it belongs in LINK.
2. Would unrelated projects want this primitive? If yes, it probably belongs in Common.
3. Is the only difference name/icon/metadata? If yes, make it a product face parameter.
4. Is it genuinely Mercedes-only or Jaguar-only? If yes, leave it in the product repository.

Any reintroduction of duplicated generic application source into MBLINK and JAGLINK should be treated as an architectural regression.
