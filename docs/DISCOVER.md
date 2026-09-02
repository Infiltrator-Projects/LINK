<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK Discover

LINK Discover is the single shared discovery, ECU/module reading and evidence subsystem used by every LINK-family product.

There is not a separate Discover implementation for each manufacturer product. There is one reusable implementation in LINK, then MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK apply their own manufacturer face and vehicle knowledge.

```text
LINK Discover engine
    + Mercedes definitions / MBLINK identity
    = MBLINK Discover

LINK Discover engine
    + Jaguar definitions / JAGLINK identity
    = JAGLINK Discover

LINK Discover engine
    + BMW definitions / BMWLINK identity
    = BMWLINK Discover

LINK Discover engine
    + Audi definitions / AUDILINK identity
    = AUDILINK Discover

LINK Discover engine
    + Ford definitions / FORDLINK identity
    = FORDLINK Discover
```

Discover is not a proposed extra repository or a future separate `Reader` product. It is already the specialist second application target in each manufacturer repository. The main manufacturer application is the normal diagnostic experience; Discover is the deeper engineering-oriented ECU/module discovery, identification, read-only inventory and evidence/dump application.

The same ownership rule applies to OpenPort 2.0 on both supported desktop paths. Windows Discover uses the installed SAE J2534 FunctionLibrary, while Linux LINK can drive the Tactrix directly through its native libusb provider:

```text
platform/windows/link-discover.c
    + MBLINK identity / Mercedes definitions
    = mblink-discover.exe

platform/linux/link-linux-openport2.c
    + shared LINK diagnostic flow
    + manufacturer definitions from the owning product face
    = the same product diagnostics over native OpenPort USB
```

The generic transport, scanner, safety and evidence behaviour belongs to LINK. Manufacturer-specific results and available read-only probes may differ because Mercedes, Jaguar, BMW, Audi and Ford expose different networks, ECUs, modules, identifiers and documented diagnostic data.

## Product role

Discover has two layers of responsibility.

The shared LINK layer provides the mechanics required to locate, interrogate and record control modules safely. The product layer supplies manufacturer knowledge: known network topology, module identities, supported addresses/endpoints, documented identifiers and evidence-backed read-only requests.

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

Current functionality spans more than the first two steps. LINK supplies passive capture, bounded standard OBD inventory, generic read-only interrogation/evidence machinery and a reusable deep discovery-plan contract. MBLINK already supplies a Mercedes FULL SWEEP plan through that contract; JAGLINK deliberately does not add Jaguar-specific active depth until reproducible evidence supports the routes and requests; BMWLINK, AUDILINK and FORDLINK use the same shared machinery while manufacturer-specific depth remains evidence-gated. Further depth expands the same Discover applications rather than creating new product-specific Reader repositories or duplicate scanner programs.

## What LINK owns

LINK owns all Discover behaviour shared by the products:

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

- product display name (`MBLINK`, `JAGLINK`, `BMWLINK`, `AUDILINK` or `FORDLINK`);
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

Products create their Windows Discover application by calling LINK's CMake constructor:

```cmake
link_add_windows_discover(mblink-discover
    PRODUCT_NAME "MBLINK"
    PRODUCT_SLUG "mblink"
    WINDOW_CLASS "MBLINKDiscoverWindow")

link_add_windows_discover(jaglink-discover
    PRODUCT_NAME "JAGLINK"
    PRODUCT_SLUG "jaglink"
    WINDOW_CLASS "JAGLINKDiscoverWindow")

link_add_windows_discover(bmwlink-discover
    PRODUCT_NAME "BMWLINK"
    PRODUCT_SLUG "bmwlink"
    WINDOW_CLASS "BMWLINKDiscoverWindow")

link_add_windows_discover(audilink-discover
    PRODUCT_NAME "AUDILINK"
    PRODUCT_SLUG "audilink"
    WINDOW_CLASS "AUDILINKDiscoverWindow")

link_add_windows_discover(fordlink-discover
    PRODUCT_NAME "FORDLINK"
    PRODUCT_SLUG "fordlink"
    WINDOW_CLASS "FORDLINKDiscoverWindow")
```

All five calls compile the same LINK Windows Discover implementation and link the same `LINK::Core` target. Manufacturer-specific knowledge remains supplied by the owning product layer.

LINK CI builds the established reference faces on Windows, while BMWLINK, AUDILINK and FORDLINK consume the same constructor in their own repositories. This is an architectural contract: a shared Discover change must not silently depend on one manufacturer face.

## Discover compatibility facades

Product repositories may expose tiny product-named compatibility headers such as `mblink/discover.h`, `jaglink/discover.h`, `bmwlink/discover.h`, `audilink/discover.h` or `fordlink/discover.h`. Those headers may alias product-prefixed names to the `link_*` API so existing callers do not require a flag-day rename.

Such headers are product-face compatibility files only. They contain no independent Discover implementation.

## Safety model

Discover is read-oriented by design. Passive capture has no transmit path. Active inventory and identification requests reach a transport transmit call only after LINK's safety classifier admits them as explicitly read-only.

Unknown services and write/control operations remain deny-by-default. Module reset, security access, routines, DTC clearing, coding, programming and firmware-write operations are not implicitly enabled merely because a codec or transport exists.

Manufacturer-specific read requests must be evidence-backed and bounded before a product face can expose them through Discover.

## Evidence and dump model

"Dump" in Discover means a structured read-only acquisition of available diagnostic information and raw responses, not an unrestricted write/programming path.

All LINK-family products use LINK's evidence writer. Frame/result schema, escaping, timestamps, annotations and export behaviour remain shared. Product/manufacturer identity and manufacturer-specific decoded fields may differ while the raw evidence is preserved.

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
Infiltratr Common
        |
       LINK
        |
        +-- MBLINK   -> MBLINK Discover
        +-- JAGLINK  -> JAGLINK Discover
        +-- BMWLINK  -> BMWLINK Discover
        +-- AUDILINK -> AUDILINK Discover
        +-- FORDLINK -> FORDLINK Discover
```

There are no separate manufacturer Reader repositories in the intended architecture. Discover evolves in place as the specialist branded reader/dumper target within each product repository, while reusable mechanics continue to move downward into LINK.

If a future LINK-family product change introduces a second generic Discover implementation, a second Windows scanner source, or a separate repository solely to duplicate this application, treat that as an architectural regression unless there is a demonstrable independent product lifecycle that cannot be represented as a product target.
