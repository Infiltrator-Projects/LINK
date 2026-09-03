<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK Product Face Contract

MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK are branded vehicle products built from the same LINK application engine. Each manufacturer repository may expose more than one branded application target, but those targets remain part of the same manufacturer product family rather than becoming separate repositories by default.

The architectural target is:

```text
Infiltratr Common
        |
       LINK
        |
        +-- MBLINK
        |    +-- main app
        |    +-- Discover
        +-- JAGLINK
        |    +-- main app
        |    +-- Discover
        +-- BMWLINK
        |    +-- main app
        |    +-- Discover
        +-- AUDILINK
        |    +-- main app
        |    +-- Discover
        +-- FORDLINK
             +-- main app
             +-- Discover
```

The current specialist second target is Discover. MBLINK Discover, JAGLINK Discover, BMWLINK Discover, AUDILINK Discover and FORDLINK Discover are manufacturer-branded ECU/module discovery, identification, read-only inventory and evidence/dump applications built from LINK's shared Discover machinery. Their reusable mechanics belong in LINK; their manufacturer knowledge belongs in the owning product repository.

## LINK owns behaviour

Anything behavioural that should work the same across LINK-family products belongs in LINK unless it is generic enough to belong in Infiltratr Common.

Examples include protocol engines, transport/provider contracts, discovery state machines, ECU/module interrogation primitives, evidence/logging, safety policy, shared UI structure, platform glue, packaging helpers and common build/release behaviour.

For Discover specifically, LINK owns the reusable scanner, OpenPort/J2534 integration, standard OBD inventory, generic ISO-TP/UDS read-only machinery, result/evidence model and deny-by-default safety policy.

For the main diagnostic applications, LINK owns the **operator-task information
architecture** and the diagnostic-generation model. Protocols such as OBD-II,
OBDonUDS, UDS and manufacturer legacy diagnostics are data sources underneath
the interface; they are not top-level navigation destinations.

The shared task model is Vehicle, Errors, Table, Dashboard, Graph, Tests,
Services, Log and Settings. Standard and manufacturer-specific data are merged
into the appropriate task screen and labelled by source where useful. Module
inventory belongs under Vehicle; a manufacturer may offer a Scan Modules action
there, but Modules is not a competing primary task.

Manufacturer repositories may contribute evidence-backed legacy protocols,
module scans, parameters, tests and service procedures, but must not fork the
shared task structure.

Language selection and measurement conversion are shared capabilities in LINK.
Each manufacturer application owns the composition and appearance of its own
Settings page and calls those shared capabilities where appropriate.

## Product repositories own identity and vehicle specificity

A product repository may own:

- its name, executable names and bundle/application identifiers;
- its icon/emblem and other brand assets;
- its README/version/release identity;
- multiple branded application targets that share the same manufacturer lifecycle;
- manufacturer-specific Mercedes, Jaguar, BMW, Audi or Ford network/module definitions and diagnostic behaviour;
- evidence-backed manufacturer-specific read-only ECU/module probes and decoders;
- minimal compatibility facades needed while old product-prefixed APIs are migrated.

## Shared icon composition

LINK owns the neutral high-resolution icon base at
`assets/branding/link-obd-icon-base.png`. It contains the shared dark
rounded-square panel and the front-facing 16-pin OBD-II connector, with the
upper emblem area intentionally empty.

Manufacturer product icons derive from that base by adding only the
manufacturer emblem in the reserved upper area. The base panel and connector
are the visual invariant; each branded derivative remains owned and packaged
by its product repository. MBLINK, JAGLINK, FORDLINK, BMWLINK and AUDILINK all
use this composition, which is also the rule for future product faces.

A product repository must not own a copied generic LINK implementation.

The fact that the five Discover product faces can expose different modules, identifiers or views does not justify separate generic scanner implementations: those differences are manufacturer data/behaviour layered over the same engine.

## Repository boundary rule

Create a new repository only when something is genuinely an independent product with an independent ownership/release lifecycle and substantial implementation that does not naturally belong to the manufacturer family.

Do not create a separate repository merely because a manufacturer product has a second executable or application target.

Therefore the intended automotive repository set remains:

```text
LINK
MBLINK
JAGLINK
BMWLINK
AUDILINK
FORDLINK
```

not:

```text
LINK
MBLINK
MBLINK-Reader
JAGLINK
JAGLINK-Reader
```

Discover already fills the specialist reader/dumper role inside the LINK-family product repositories and should evolve there.

## Face-only rule

Product files that differ only by product name, icon, colours or metadata are candidates to become one LINK file with face parameters.

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
MBLINK   -> LINK -> Infiltratr Common
JAGLINK  -> LINK -> Infiltratr Common
BMWLINK  -> LINK -> Infiltratr Common
AUDILINK -> LINK -> Infiltratr Common
FORDLINK -> LINK -> Infiltratr Common
```

Application targets inside MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK do not introduce new dependency roots; they consume the same pinned LINK tree.

## Regression test

When reviewing a change, ask:

1. Would multiple LINK-family products want this behaviour? If yes, it belongs in LINK.
2. Would unrelated projects want this primitive? If yes, it probably belongs in Common.
3. Is the only difference name/icon/metadata? If yes, make it a product-face parameter.
4. Is it genuinely manufacturer-specific to Mercedes, Jaguar, BMW, Audi or Ford? If yes, leave it in the product repository.
5. Is this merely another application target for the same manufacturer product? If yes, keep it in the existing product repository rather than creating another repo.

Any reintroduction of duplicated generic application source into MBLINK, JAGLINK, BMWLINK, AUDILINK or FORDLINK, or creation of parallel Reader repos that merely duplicate Discover, should be treated as an architectural regression.



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
wording and manufacturer-specific content. They may add manufacturer-specific
diagnostic screens and fields, but they must not fork the shared
layout primitives merely to change colours, fonts or branding.

An iPhone layout change intended for every vehicle product must therefore be
made in LINK first and consumed by each pinned product face. This makes visual
and structural drift between MBLINK, JAGLINK, BMWLINK, AUDILINK, FORDLINK and future LINK products an
explicit architectural regression rather than normal parallel development.


### Apple language and units boundary

LINK supplies the common language catalogue/selection API and the two
measurement systems proven in MBLINK: Metric and US customary. Manufacturer
applications decide how their Settings page is laid out and which
manufacturer-specific settings appear there. LINK does not own favourites
policy, unavailable-value policy, adapter/About rows, or a complete Settings
screen.
