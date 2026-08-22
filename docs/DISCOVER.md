<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK Discover

LINK Discover is the single shared discovery and evidence subsystem used by every LINK-family product.

There is not an MBLINK implementation of Discover and a separate JAGLINK implementation of Discover. There is one implementation in LINK, then each product applies its own face.

```text
LINK Discover engine
    + MBLINK face
    = MBLINK Discover

LINK Discover engine
    + JAGLINK face
    = JAGLINK Discover
```

The same rule applies to the Windows OpenPort 2.0 / SAE J2534 scanner:

```text
platform/windows/link-discover.c
    + MBLINK identity
    = mblink-discover.exe

platform/windows/link-discover.c
    + JAGLINK identity
    = jaglink-discover.exe
```

Both executables must therefore have identical diagnostic, capture, safety and evidence behaviour. Their permitted differences are branding only.

## What LINK owns

LINK owns all Discover behaviour shared by the products:

- passive 500 kbit/s CAN capture;
- bounded standard OBD inventory;
- OpenPort 2.0 / J2534 discovery and connection logic;
- deny-by-default safety classification;
- JSON Lines evidence recording and operator annotations;
- shared discovery UI behaviour and control flow;
- shared tests and Windows build machinery.

The portable public API is `include/link/discover.h`. The implementation lives under `src/discover/` and the Windows front end lives at `platform/windows/link-discover.c`.

## What a product face may change

A product face may supply only identity and presentation values such as:

- product display name (`MBLINK` or `JAGLINK`);
- executable/product slug (`mblink` or `jaglink`);
- native window class name;
- product icon or emblem resource;
- product-specific application metadata;
- product-specific evidence filename prefix;
- genuinely vehicle-specific definitions supplied by the product layer.

A product face must not fork or copy the scanner, safety classifier, evidence writer, OBD inventory logic, capture loop, or J2534 implementation.

If a behavioural change makes sense for both products, it is a LINK change.

## Windows branded targets

Products create their Windows scanner by calling LINK's CMake constructor:

```cmake
link_add_windows_discover(mblink-discover
    PRODUCT_NAME "MBLINK"
    PRODUCT_SLUG "mblink"
    WINDOW_CLASS "MBLINKDiscoverWindow")

link_add_windows_discover(jaglink-discover
    PRODUCT_NAME "JAGLINK"
    PRODUCT_SLUG "jaglink"
    WINDOW_CLASS "JAGLINKDiscoverWindow")
```

Both calls compile the exact same `platform/windows/link-discover.c` source and link the same `LINK::Core` target. No product repository should contain its own Windows scanner source file.

LINK CI also builds both reference faces on Windows. This is an architectural test: if one face stops building, the shared contract has regressed.

## Discover compatibility façades

During migration, product repositories may expose tiny product-named compatibility headers such as `mblink/discover.h` or `jaglink/discover.h`. Those headers may alias product-prefixed names to the `link_*` API so existing callers do not require a flag-day rename.

Such headers are product-face compatibility files only. They contain no Discover implementation.

## Safety model

Passive capture has no transmit path. The bounded OBD inventory reaches the J2534 transmit call only after LINK's safety classifier admits the request as explicitly read-only.

Unknown services and write/control operations remain deny-by-default. This policy belongs to LINK and therefore cannot drift between MBLINK and JAGLINK.

## Evidence model

Both products use LINK's JSONL evidence writer. Frame and annotation schema, escaping, timestamps and export behaviour are therefore identical. Only the product-facing filename/presentation identity may differ.

## Source-of-truth rule

If a future MBLINK or JAGLINK change introduces a second Discover implementation or a second Windows scanner source, that is an architectural regression. Move the behaviour into LINK and keep only the face-specific identity in the product repository.
