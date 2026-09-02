<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK Product Face Contract

MBLINK and JAGLINK are two branded vehicle products built from the same LINK application engine. Each manufacturer repository may expose more than one branded application target, but those targets remain part of the same manufacturer product family rather than becoming separate repositories by default.

The architectural target is:

```text
Infiltratr Common
        |
       LINK
      /    \
 MBLINK   JAGLINK
  |  |      |  |
  |  +-- Discover
  +----- main app
             +-- Discover
             +-- main app
```

The current specialist second target is Discover. MBLINK Discover and JAGLINK Discover are the manufacturer-branded ECU/module discovery, identification, read-only inventory and evidence/dump applications. Their reusable mechanics belong in LINK; their manufacturer knowledge belongs in the owning product repository.

## LINK owns behaviour

Anything behavioural that should work the same in both products belongs in LINK unless it is generic enough to belong in Infiltratr Common.

Examples include protocol engines, transport/provider contracts, discovery state machines, ECU/module interrogation primitives, evidence/logging, safety policy, shared UI structure, platform glue, packaging helpers and common build/release behaviour.

For Discover specifically, LINK owns the reusable scanner, OpenPort/J2534 integration, standard OBD inventory, generic ISO-TP/UDS read-only machinery, result/evidence model and deny-by-default safety policy.

For the main diagnostic applications, LINK also owns the common **OBD**
workspace and diagnostic-generation model. That workspace presents the same
standards-backed VIN, responder, PID, readiness, freeze-frame and
stored/pending/permanent fault inventory in every product face. Manufacturer
repositories may contribute evidence that a legacy diagnostic protocol answered,
but they must not replace the common OBD page with a copied product-specific
implementation.

## Product repositories own identity and vehicle specificity

A product repository may own:

- its name, executable names and bundle/application identifiers;
- its icon/emblem and other brand assets;
- its README/version/release identity;
- multiple branded application targets that share the same manufacturer lifecycle;
- Mercedes-only or Jaguar-only network/module definitions and diagnostic behaviour;
- evidence-backed manufacturer-specific read-only ECU/module probes and decoders;
- minimal compatibility facades needed while old product-prefixed APIs are migrated.

A product repository must not own a copied generic LINK implementation.

The fact that MBLINK Discover and JAGLINK Discover can expose different modules, identifiers or views does not justify separate generic scanner implementations: those differences are manufacturer data/behaviour layered over the same engine.

## Repository boundary rule

Create a new repository only when something is genuinely an independent product with an independent ownership/release lifecycle and substantial implementation that does not naturally belong to the manufacturer family.

Do not create a separate repository merely because a manufacturer product has a second executable or application target.

Therefore the intended automotive repository set remains:

```text
LINK
MBLINK
JAGLINK
```

not:

```text
LINK
MBLINK
MBLINK-Reader
JAGLINK
JAGLINK-Reader
```

Discover already fills the specialist reader/dumper role inside MBLINK and JAGLINK and should evolve there.

## Face-only rule

Two product files that differ only because one says MBLINK and the other says JAGLINK are candidates to become one LINK file with face parameters.

A product-specific file is justified only when changing it for one manufacturer/vehicle family would genuinely be wrong for the other.

Branding and manufacturer knowledge are data/definitions. Shared behaviour is code. Branding differences must not create behavioural forks.

## Build rule

LINK should expose constructors/helpers that accept product identity and build the common implementation under the requested product name. Product CMake files should select/configure the face rather than duplicate targets or source files.

For example, the Windows Discover scanner is sourced from LINK's shared implementation; `mblink-discover.exe` and `jaglink-discover.exe` are branded outputs of that one implementation with manufacturer-specific knowledge supplied by their product layers.

The same principle should be followed if Discover gains additional Linux or Apple-specific specialist targets: share the mechanics in LINK, keep the branded target in the existing manufacturer repository.

## Dependency rule

LINK reuses Infiltratr Common wherever possible. If logic is broadly reusable outside the vehicle-diagnostics family, it should move downward into Common rather than be duplicated in LINK.

The intended dependency graph is therefore:

```text
MBLINK -> LINK -> Infiltratr Common
JAGLINK -> LINK -> Infiltratr Common
```

Application targets inside MBLINK/JAGLINK do not introduce new dependency roots; they consume the same pinned LINK tree.

## Regression test

When reviewing a change, ask:

1. Would both MBLINK and JAGLINK want this behaviour? If yes, it belongs in LINK.
2. Would unrelated projects want this primitive? If yes, it probably belongs in Common.
3. Is the only difference name/icon/metadata? If yes, make it a product-face parameter.
4. Is it genuinely Mercedes-only or Jaguar-only? If yes, leave it in the product repository.
5. Is this merely another application target for the same manufacturer product? If yes, keep it in the existing product repository rather than creating another repo.

Any reintroduction of duplicated generic application source into MBLINK and JAGLINK, or creation of parallel Reader repos that merely duplicate Discover, should be treated as an architectural regression.



## Linux/GTK face contract

The Linux connection shell follows the same rule as the Apple face: LINK owns
geometry and product repositories own appearance and manufacturer content.

`platform/linux/link-gtk-shell.c` owns the adapter row, selector expansion,
toolbar/action sizing, status strip, shared card padding/radii, navigation
spacing and other common shell dimensions. A product face may supply colours,
fonts, logos and manufacturer-specific page content, but it must not override
shared `.link-*` dimensions merely to make one brand wider, taller or more
compact than another.

In particular, adapter-selector width is deliberately not a
`LinkGtkShellDescriptor` option. MBLINK, JAGLINK, FORDLINK, AUDILINK,
BMWLINK and future LINK-family products consume the same expanding selector
and the same common control geometry. If a shell-size change is appropriate
for every product, make it in LINK. If it is only a colour/font/identity
change, keep it in the product theme.

## Apple/iPhone face contract

The Apple/iPhone product face is a LINK-owned presentation contract, not a
manufacturer-owned copy. `platform/apple/LinkDiagnosticUI.swift` owns the
shared screen geometry and information architecture: command-centre shell,
header behaviour, connection/progress placement, primary diagnostic grid,
secondary tools, panel/tile shapes, corner radii, padding and spacing.

Manufacturer products inject a `LinkDiagnosticTheme` plus their logo,
wording and manufacturer-specific content. They may add Mercedes-only or
Jaguar-only diagnostic screens and fields, but they must not fork the shared
layout primitives merely to change colours, fonts or branding.

An iPhone layout change intended for every vehicle product must therefore be
made in LINK first and consumed by each pinned product face. This makes visual
and structural drift between MBLINK, JAGLINK and future LINK products an
explicit architectural regression rather than normal parallel development.
