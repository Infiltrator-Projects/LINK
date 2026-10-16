<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Link Product Face Contract

MBLink, JAGLink, BMWLink, AUDILink and FORDLink are branded vehicle products built from the same Link application engine. Each manufacturer repository may expose more than one branded application target, but those targets remain part of the same manufacturer product family rather than becoming separate repositories by default.

The architectural target is:

```text
Libraries
        |
       Link
        |
        +-- MBLink
        |    +-- main app
        |    +-- Discover
        +-- JAGLink
        |    +-- main app
        |    +-- Discover
        +-- BMWLink
        |    +-- main app
        |    +-- Discover
        +-- AUDILink
        |    +-- main app
        |    +-- Discover
        +-- FORDLink
             +-- main app
             +-- Discover
```

The current specialist second target is Discover. MBLink Discover, JAGLink Discover, BMWLink Discover, AUDILink Discover and FORDLink Discover are manufacturer-branded ECU/module discovery, identification, read-only inventory and evidence/dump applications built from Link's shared Discover machinery. Their reusable mechanics belong in Link; their manufacturer knowledge belongs in the owning product repository.

## Link owns behaviour

Anything behavioural that should work the same across Link-family products belongs in Link unless it is generic enough to belong in Libraries.

Examples include protocol engines, transport/provider contracts, discovery state machines, ECU/module interrogation primitives, evidence/logging, safety policy, shared UI structure, platform glue, packaging helpers and common build/release behaviour.

For Discover specifically, Link owns the reusable scanner, OpenPort/J2534 integration, standard OBD inventory, generic ISO-TP/UDS read-only machinery, result/evidence model and deny-by-default safety policy.

For the main diagnostic applications, Link owns the **operator-task information
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

Language selection and measurement conversion are shared capabilities in Link.
Manufacturer-rich applications own the composition and appearance of their
Settings page and call those shared capabilities where appropriate. Standard
faces use Link's standard Settings composition and supply only product identity
and theme.

About is a shared Link capability and, unlike Settings, is a complete standard
surface. Link owns the Linux, Windows and iPhone About renderers and their
behaviour. Product repositories supply only identity metadata such as product
name, version, description, release date, author/authors, copyright, website,
licence, credits and the product emblem.

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

Link owns the neutral high-resolution icon base at
`assets/branding/link-obd-icon-base.png`. It contains the shared dark
rounded-square panel and the front-facing 16-pin OBD-II connector, with the
upper emblem area intentionally empty.

Manufacturer product icons derive from that base by adding only the
manufacturer emblem in the reserved upper area. The base panel and connector
are the visual invariant; each branded derivative remains owned and packaged
by its product repository. MBLink, JAGLink, FORDLink, BMWLink and AUDILink all
use this composition, which is also the rule for future product faces.

A product repository must not own a copied generic Link implementation.

The fact that the five Discover product faces can expose different modules, identifiers or views does not justify separate generic scanner implementations: those differences are manufacturer data/behaviour layered over the same engine.

## Repository boundary rule

Create a new repository only when something is genuinely an independent product with an independent ownership/release lifecycle and substantial implementation that does not naturally belong to the manufacturer family.

Do not create a separate repository merely because a manufacturer product has a second executable or application target.

Therefore the intended automotive repository set remains:

```text
Link
MBLink
JAGLink
BMWLink
AUDILink
FORDLink
```

not:

```text
Link
MBLink
MBLink-Reader
JAGLink
JAGLink-Reader
```

Discover already fills the specialist reader/dumper role inside the Link-family product repositories and should evolve there.

## Face-only rule

Product files that differ only by product name, icon, colours or metadata are candidates to become one Link file with face parameters.

A product-specific file is justified only when changing it for one manufacturer/vehicle family would genuinely be wrong for the other.

Branding and manufacturer knowledge are data/definitions. Shared behaviour is code. Branding differences must not create behavioural forks.

## Build rule

Link should expose constructors/helpers that accept product identity and build the common implementation under the requested product name. Product CMake files should select/configure the face rather than duplicate targets or source files.

For example, the Windows Discover scanner is sourced from Link's shared implementation; `mblink-discover.exe` and `jaglink-discover.exe` are branded outputs of that one implementation with manufacturer-specific knowledge supplied by their product layers.

The same principle should be followed if Discover gains additional Linux or Apple-specific specialist targets: share the mechanics in Link, keep the branded target in the existing manufacturer repository.

## Dependency rule

Link reuses Libraries wherever possible. If logic is broadly reusable outside the vehicle-diagnostics family, it should move downward into Common rather than be duplicated in Link.

The intended dependency graph is therefore:

```text
MBLink   -> Link -> Libraries
JAGLink  -> Link -> Libraries
BMWLink  -> Link -> Libraries
AUDILink -> Link -> Libraries
FORDLink -> Link -> Libraries
```

Application targets inside MBLink, JAGLink, BMWLink, AUDILink and FORDLink do not introduce new dependency roots; they consume the same pinned Link tree.

## Regression test

When reviewing a change, ask:

1. Would multiple Link-family products want this behaviour? If yes, it belongs in Link.
2. Would unrelated projects want this primitive? If yes, it probably belongs in Common.
3. Is the only difference name/icon/metadata? If yes, make it a product-face parameter.
4. Is it genuinely manufacturer-specific to Mercedes, Jaguar, BMW, Audi or Ford? If yes, leave it in the product repository.
5. Is this merely another application target for the same manufacturer product? If yes, keep it in the existing product repository rather than creating another repo.

Any reintroduction of duplicated generic application source into MBLink, JAGLink, BMWLink, AUDILink or FORDLink, or creation of parallel Reader repos that merely duplicate Discover, should be treated as an architectural regression.



## Linux/GTK face contract

The Linux connection shell follows the same rule as the Apple face: Link owns
geometry and product repositories own appearance and manufacturer content.

`platform/linux/link-gtk-shell.c` owns the adapter row, selector expansion,
toolbar/action sizing, status strip, shared card padding/radii, navigation
spacing and other common shell dimensions. A product face may supply colours,
fonts, logos and manufacturer-specific page content, but it must not override
shared `.link-*` dimensions merely to make one brand wider, taller or more
compact than another.

In particular, adapter-selector width is deliberately not a
`LinkGtkShellDescriptor` option. MBLink, JAGLink, FORDLink, AUDILink,
BMWLink and future Link-family products consume the same expanding selector
and the same common control geometry. If a shell-size change is appropriate
for every product, make it in Link. If it is only a colour/font/identity
change, keep it in the product theme.

## Apple/iPhone face contract

The Apple/iPhone product face is a Link-owned presentation contract, not a
manufacturer-owned copy. `platform/apple/LinkDiagnosticUI.swift` owns the
shared screen geometry and information architecture: command-centre shell,
header behaviour, connection/progress placement, primary diagnostic grid,
secondary tools, panel/tile shapes, corner radii, padding and spacing.

The same file also owns `LinkStandardProductViewModel`, the complete standard
application model used by product faces that do not yet add a manufacturer
extension, and `LinkStandardProductContentView`, the matching complete standard
task surface. `LinkStandardProductAppearance` is the face parameter containing
the product's name, emblem, theme and About metadata.
`LinkProductDiagnosticsController` owns the corresponding Objective-C
forwarding boundary. A standard-only product supplies identity, version,
simulation identity, appearance and its typed delegate; it must not copy the
connection/profile/PID/dashboard/evidence orchestration or standard task views
into its repository. Manufacturer-rich products may retain a specialised view
model and task screens, but must use the Link connection presenter and shared
controller façade rather than copying those generic mechanisms.

Manufacturer products inject a `LinkDiagnosticTheme` plus their logo,
wording and manufacturer-specific content. They may add manufacturer-specific
diagnostic screens and fields, but they must not fork the shared
layout primitives merely to change colours, fonts or branding.

An iPhone layout change intended for every vehicle product must therefore be
made in Link first and consumed by each pinned product face. This makes visual
and structural drift between MBLink, JAGLink, BMWLink, AUDILink, FORDLink and future Link products an
explicit architectural regression rather than normal parallel development.

### Shared Dashboard presentation

Dashboard presentation is Link behaviour. Link exposes the three persistent
operator modes `numbers`, `dials` and `combined` and owns gauge range/fraction
semantics. Native faces draw the same instrument design using their platform
toolkit; product repositories provide colours/fonts and manufacturer-specific
parameter metadata only. A structured/text state without a meaningful numeric
range remains a text tile even when Dials or Combined is selected.

The existing Linux circular cockpit instrument is the visual reference: a
270-degree sweep, dark radial face, restrained accent arc, exact live value in
the centre and the parameter title beneath. Apple reproduces that geometry in
SwiftUI. Windows and Android main diagnostic faces must consume the same core
mode/range contract when those faces are present; they must not invent another
mode enum or independent scaling rules.


### Apple language and units boundary

Link supplies the common language catalogue/selection API and the two
measurement systems proven in MBLink: Metric and US customary. Manufacturer
applications decide how their Settings page is laid out and which
manufacturer-specific settings appear there. Link does not own
unavailable-value policy, adapter rows, or a complete Settings screen.
About is deliberately separate from Settings and is Link-owned.


## Shared About contract

About is application infrastructure, not manufacturer diagnostic behaviour.
Link owns the standard About presentation on every supported face:

- Linux uses `link_gtk_show_about()` with `LinkAboutInfo`;
- Windows uses `link_windows_show_about()` with `LinkAboutInfo`;
- iPhone uses `LinkDiagnosticAboutView` with `LinkDiagnosticAboutInfo`.

Product repositories supply facts and branding only. They must not fork the
layout merely to change a name, version, logo, author list, date, description,
website, licence or credits.

The Linux `show_about` callback remains temporarily for source compatibility
with product faces that already own an About dialog. Supplying
`LinkGtkShellDescriptor.about` selects the Link renderer; the compatibility
callback can be removed as each product is migrated. New product faces should
not introduce another custom About implementation.
