<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Link Discover

Link Discover is the single shared discovery, ECU/module reading and evidence subsystem used by every Link-family product.

There is not a separate Discover implementation for each manufacturer product. There is one reusable implementation in Link, then MBLink, JAGLink, BMWLink, AUDILink and FORDLink apply their own manufacturer face and vehicle knowledge.

```text
Link Discover engine
    + Mercedes definitions / MBLink identity
    = MBLink Discover

Link Discover engine
    + Jaguar definitions / JAGLink identity
    = JAGLink Discover

Link Discover engine
    + BMW definitions / BMWLink identity
    = BMWLink Discover

Link Discover engine
    + Audi definitions / AUDILink identity
    = AUDILink Discover

Link Discover engine
    + Ford definitions / FORDLink identity
    = FORDLink Discover
```

Discover is not a proposed extra repository or a future separate `Reader` product. It is already the specialist second application target in each manufacturer repository. The main manufacturer application is the normal diagnostic experience; Discover is the deeper engineering-oriented ECU/module discovery, identification, read-only inventory and evidence/dump application.

The same ownership rule applies to OpenPort 2.0 on both supported desktop paths. Windows Discover uses the installed SAE J2534 FunctionLibrary, while Linux Link can drive the Tactrix directly through its native libusb provider:

```text
platform/windows/link-discover.c
    + MBLink identity / Mercedes definitions
    = mblink-discover.exe

platform/linux/link-linux-openport2.c
    + shared Link diagnostic flow
    + manufacturer definitions from the owning product face
    = the same product diagnostics over native OpenPort USB
```

The generic transport, scanner, safety and evidence behaviour belongs to Link. Manufacturer-specific results and available read-only probes may differ because Mercedes, Jaguar, BMW, Audi and Ford expose different networks, ECUs, modules, identifiers and documented diagnostic data.

## Product role

Discover has two layers of responsibility.

The shared Link layer provides the mechanics required to locate, interrogate and record control modules safely. The product layer supplies manufacturer knowledge: known network topology, module identities, supported addresses/endpoints, documented identifiers and evidence-backed read-only requests.

This lets the same Discover engine operate professionally across very different vehicles without reducing it to a lowest-common-denominator OBD scanner and without forking the implementation for each manufacturer.

The intended progression is:

```text
passive network observation
    -> bounded standards-based inventory
    -> manufacturer-aware module discovery
    -> ECU/module identification
    -> documented read-only data acquisition
    -> structured raw/evidence dump
```

Current functionality spans more than the first two steps. Link supplies passive capture, bounded standard OBD inventory, generic read-only interrogation/evidence machinery and a reusable deep discovery-plan contract. Link also now owns a portable Vehicle Research session state and typed evidence timeline: passive capture, standards inventory, manufacturer sweep, operator event markers and export summaries remain distinguishable in the raw evidence instead of being flattened into UI log text. The shared Windows Discover face consumes that state directly. MBLink already supplies a Mercedes FULL SWEEP plan through the deep-discovery contract; JAGLink deliberately does not add Jaguar-specific active depth until reproducible evidence supports the routes and requests; BMWLink, AUDILink and FORDLink use the same shared machinery while manufacturer-specific depth remains evidence-gated. Further depth expands the same Discover applications rather than creating new product-specific Reader repositories or duplicate scanner programs.

## What Link owns

Link owns all Discover behaviour shared by the products:

- passive CAN/CAN-FD observation where the selected transport supports it;
- bounded standard OBD inventory;
- OpenPort 2.0 / J2534 discovery and connection logic;
- generic ISO-TP/UDS read-only interrogation machinery;
- common ECU/module discovery state and result model;
- generic identification and raw-response capture primitives;
- deny-by-default safety classification;
- structured evidence/dump recording and operator annotations;
- shared discovery UI behaviour and control flow;
- shared tests and platform build machinery.

The portable public API begins at `include/link/discover.h`. Shared implementation lives under `src/discover/`; platform shells such as the Windows front end live under `platform/`.

If a capability is useful across multiple manufacturer products, it belongs here rather than being copied into product repositories.

## What a product face owns

A product face may supply:

- product display name (`MBLink`, `JAGLink`, `BMWLink`, `AUDILink` or `FORDLink`);
- executable/product slug (`mblink`, `jaglink`, `bmwlink`, `audilink` or `fordlink`);
- native window/bundle identity;
- product icon or emblem resource;
- product-specific application metadata;
- product-specific evidence filename prefix;
- manufacturer network topology and module catalogue;
- evidence-backed manufacturer ECU/module identities;
- documented or reproducibly verified manufacturer-specific read-only probes, identifiers and decoders;
- presentation appropriate to the manufacturer's module hierarchy.

Those differences are not generic scanner forks. They are vehicle knowledge supplied to the shared Discover engine.

A product face must not fork or copy the J2534 transport, generic scanner state machine, safety classifier, evidence writer, standard OBD inventory, ISO-TP/UDS machinery or shared platform shell.

## Windows branded targets

Products create their Windows Discover application by calling Link's CMake constructor:

```cmake
link_add_windows_discover(mblink-discover
    PRODUCT_NAME "MBLink"
    PRODUCT_SLUG "mblink"
    WINDOW_CLASS "MBLINKDiscoverWindow")

link_add_windows_discover(jaglink-discover
    PRODUCT_NAME "JAGLink"
    PRODUCT_SLUG "jaglink"
    WINDOW_CLASS "JAGLINKDiscoverWindow")

link_add_windows_discover(bmwlink-discover
    PRODUCT_NAME "BMWLink"
    PRODUCT_SLUG "bmwlink"
    WINDOW_CLASS "BMWLINKDiscoverWindow")

link_add_windows_discover(audilink-discover
    PRODUCT_NAME "AUDILink"
    PRODUCT_SLUG "audilink"
    WINDOW_CLASS "AUDILINKDiscoverWindow")

link_add_windows_discover(fordlink-discover
    PRODUCT_NAME "FORDLink"
    PRODUCT_SLUG "fordlink"
    WINDOW_CLASS "FORDLINKDiscoverWindow")
```

All five calls compile the same Link Windows Discover implementation and link the same `Link::Core` target. Manufacturer-specific knowledge remains supplied by the owning product layer.

Link CI builds the established reference faces on Windows, while BMWLink, AUDILink and FORDLink consume the same constructor in their own repositories. This is an architectural contract: a shared Discover change must not silently depend on one manufacturer face.

## Discover compatibility facades

Product repositories may expose tiny product-named compatibility headers such as `mblink/discover.h`, `jaglink/discover.h`, `bmwlink/discover.h`, `audilink/discover.h` or `fordlink/discover.h`. Those headers may alias product-prefixed names to the `link_*` API so existing callers do not require a flag-day rename.

Such headers are product-face compatibility files only. They contain no independent Discover implementation.

## Safety model

Discover is read-oriented by design. Passive capture has no transmit path. Active inventory and identification requests reach a transport transmit call only after Link's safety classifier admits them as explicitly read-only.

Unknown services and write/control operations remain deny-by-default. Module reset, security access, routines, DTC clearing, coding, programming and firmware-write operations are not implicitly enabled merely because a codec or transport exists.

Manufacturer-specific read requests must be evidence-backed and bounded before a product face can expose them through Discover.

## Evidence and dump model

"Dump" in Discover means a structured read-only acquisition of available diagnostic information and raw responses, not an unrestricted write/programming path.

All Link-family products use Link's evidence writer. Frame/result schema, escaping, timestamps, annotations and export behaviour remain shared. Product/manufacturer identity and manufacturer-specific decoded fields may differ while the raw evidence is preserved.

A useful Discover export should make it possible to answer:

- which networks/endpoints were examined;
- which modules responded;
- which requests were attempted or blocked;
- each raw request/response pair;
- any evidence-backed decoded identity;
- timing/result status and operator annotations;
- the exact product/profile used for interpretation.

## Repository rule

The professional repository model is therefore:

```text
Libraries
        |
       Link
        |
        +-- MBLink   -> MBLink Discover
        +-- JAGLink  -> JAGLink Discover
        +-- BMWLink  -> BMWLink Discover
        +-- AUDILink -> AUDILink Discover
        +-- FORDLink -> FORDLink Discover
```

There are no separate manufacturer Reader repositories in the intended architecture. Discover evolves in place as the specialist branded reader/dumper target within each product repository, while reusable mechanics continue to move downward into Link.

If a future Link-family product change introduces a second generic Discover implementation, a second Windows scanner source, or a separate repository solely to duplicate this application, treat that as an architectural regression unless there is a demonstrable independent product lifecycle that cannot be represented as a product target.
