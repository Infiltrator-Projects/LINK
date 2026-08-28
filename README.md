<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK

[![LINK CI](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml)

LINK is the shared C11 vehicle-diagnostics and application engine used by MBLINK and JAGLINK.

**Current source version:** see [`VERSION`](VERSION)  
**Shared foundation:** exact Infiltratr Common release/commit pinned by `src/infiltratr-common` and `CMakeLists.txt`  
**Platforms:** Linux, Windows, macOS/iOS-facing portable core, bare-metal STM32  
**Licence:** GPL-3.0-or-later

## Role in the project family

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns portable facilities useful outside vehicle diagnostics. LINK owns product-neutral automotive functionality. MBLINK and JAGLINK own branding, metadata and genuinely manufacturer-specific behaviour.

Each manufacturer repository may build multiple branded application targets without creating a new repository. Today that includes the main MBLINK/JAGLINK applications and the specialist MBLINK Discover/JAGLINK Discover targets. Discover is the ECU/module discovery, identification, read-only inventory and evidence/dump application; its generic mechanics live in LINK while Mercedes/Jaguar knowledge remains in the appropriate product repository.

## Capabilities

LINK currently owns:

- platform-neutral byte-stream transport ABI;
- ELM327 framing, parsing, initialization and adapter/protocol probing;
- ELM327-managed ISO 15765 CAN channels and transport-backed sessions;
- standard OBD-II requests, PID/VIN/readiness/DTC decoding;
- a complete pinned generic OBD-II DTC catalogue containing 9,533 definitions across the seven standardized generic families (`P0`, `P2`, standardized `P3`, `B0`, `C0`, `U0`, `U3`), with normalized classification, independently authored CC0 titles/categories, explicit unknown handling and ISO 14229 status translation;
- ISO 14229 UDS request/response, DID and client-state handling;
- a compiled 27-service ISO 14229 service catalogue and bounded request/response codecs;
- read-only UDS `ReadDTCInformation` helpers;
- Classical CAN and CAN-FD ISO-TP, including CAN-FD payloads through 64 bytes and extended First Frame lengths for PDUs above 4095 bytes;
- bare-metal STM32 CAN/FDCAN edge with a bounded interrupt queue and direct ISO-TP/UDS orchestration;
- parameter definitions, store/history, scheduler and telemetry/CSV;
- diagnostic workspace model;
- portable diagnostic-flow controller state machine;
- Discover safety classification and evidence writing;
- common ECU/module discovery, identification and raw-response acquisition primitives; and
- shared native Linux diagnostic-adapter layer for tty/RFCOMM, BlueZ BLE/GATT, BlueZ Classic/SPP and direct-libUSB Tactrix OpenPort 2.0;
- shared native Windows OpenPort 2.0/J2534 Discover scanner shell.

Codec support does not grant transmit permission. Discover remains independently deny-by-default; adding a UDS codec cannot silently broaden its request allowlist.

The DTC knowledge API is presentation-neutral and deliberately preserves the raw ECU code. Generic definitions come from a reproducible pinned OBDex CC0 data snapshot. SAE J2012 itself is not vendored or reproduced. A valid reserved or otherwise unmapped generic code remains explicit, and manufacturer-specific descriptions remain in the owning product repository rather than leaking into LINK.

The catalogue is generated deterministically by `scripts/import-obdex-dtcs.py`. The generator validates exact family counts and a total of 9,533 unique generic definitions before producing the vendored normalized snapshot and compiled C lookup. The resolver uses that compiled table directly, so product builds require no network access or runtime data files.

The shared diagnostic-flow controller owns the product-neutral sequence and supports exactly one manufacturer-extension insertion point per session:

```text
ELM init
  → standard OBD-II PID discovery
  → standard VIN
  → [optional early manufacturer extension + optional ELM restore]
  → stored / pending / permanent DTC inventory
  → [optional late manufacturer extension + optional ELM restore]
  → live-data scheduler
  → live PID decode
```

A configuration that enables both extension positions is rejected rather than silently running manufacturer discovery twice. MBLINK deliberately uses the late hook so standard fault evidence completes before a potentially long Mercedes module scan. JAGLINK currently skips the manufacturer hook. Manufacturer logic therefore stays above LINK while generic sequencing remains shared.

An optional stored, pending or permanent DTC mode may return ELM `NO DATA` or
an ISO-style `7F <service> <NRC>` response on a vehicle that does not make that
inventory available. LINK records that outcome as unavailable and continues
the bounded flow; malformed or unrelated traffic remains a hard protocol error.

## Discover application model

Discover is not a separate product repository. It is a specialist branded application target inside each manufacturer repository:

```text
LINK shared Discover engine
       ↓                 ↓
MBLINK Discover     JAGLINK Discover
 Mercedes face       Jaguar face
```

The current implementation provides passive CAN capture, bounded standard OBD inventory and a shared deep read-only discovery-plan interface that product repositories can populate with evidence-backed manufacturer targets. MBLINK already uses that interface for its explicit Mercedes FULL SWEEP; JAGLINK intentionally remains at the bounded/passive stage until Jaguar-specific routes and requests are corroborated. ECU identification, structured evidence/dump export and all generic discovery mechanics remain single-source in LINK.

That evolution must not fork the generic scanner. Mercedes-specific module topology, identifiers and probes belong in MBLINK; Jaguar-specific equivalents belong in JAGLINK. The reusable state machine, transports, safety classifier, evidence model and platform shell stay here.

See `docs/DISCOVER.md` and `docs/PRODUCT_FACES.md` for the repository/application boundary.

## Architecture

Portable diagnostic behaviour is C11. C++ is used only where it materially improves a design. Platform-required languages remain narrow presentation or interop edges and must not become alternate protocol implementations.

Shared protocol state machines, diagnostic sequencing, safety policy, generic diagnostic knowledge and transport-independent decisions belong in LINK rather than Swift, Objective-C, GTK callbacks or Win32 message handlers.

Compatibility aliases/wrappers preserve product-prefixed public APIs while the underlying ELM327, OBD-II, UDS, DTC knowledge and diagnostic-flow algorithms remain single-source in LINK.

The 27-service UDS implementation is compiled into `LINK::Core`; `include/link/uds_services.h` contains the public contract rather than per-consumer private implementations.

Bare-metal STM32 support keeps STM32Cube HAL outside the portable core: the host-tested queue/transaction glue lives in `platform/stm32`, while concrete Cube integration stays in `examples/stm32c092`. See `docs/STM32.md`.

## Build and test

```bash
git clone --recurse-submodules https://github.com/The-First-Infiltrator/LINK.git
cd LINK
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

Sanitizer build:

```bash
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DLINK_ENABLE_SANITIZERS=ON
cmake --build build-sanitized --parallel
ctest --test-dir build-sanitized --output-on-failure
```

GitHub Actions builds and tests the strict portable core on Linux, macOS and Windows, runs ASan+UBSan on Linux, and independently compiles LINK's native BlueZ/direct-libUSB adapter providers in LINK CI. The OpenPort regression suite rejects J2534 start-of-message, loopback and TX-done records as completed vehicle responses. Product Linux builds provide a second integration check; physical USB handshake validation remains a hardware test rather than something CI can simulate. The DTC knowledge suite enforces the exact 9,533-definition catalogue size and pinned upstream snapshot, samples all seven generic families, checks generic/manufacturer range boundaries, preserves lowercase normalization and malformed-code rejection, and verifies shared UDS status semantics. The ISO-TP suite covers preserved Classical CAN behaviour as well as CAN-FD single-frame, multi-frame and extended-length traffic. The STM32 regression suite host-simulates the bounded interrupt queue, wrapping HAL-style millisecond clock and a complete multi-frame UDS F190 transaction through the same LINK ISO-TP/UDS core. The Windows configuration proves that the same shared Discover implementation can produce both MBLINK and JAGLINK product faces.

CI also installs LINK and its Common dependency to a clean prefix, rediscovers the exported `LINK::Core` package with `find_package`, and builds an external consumer that exercises the 27-service catalogue and 64-byte CAN-FD contract.

## Installed CMake package

A configured build can be installed with:

```bash
cmake --install build --prefix /desired/prefix
```

Consumers then use `find_package(LINK CONFIG REQUIRED)` and link `LINK::Core`. The installed LINK package resolves its `InfiltratrCommon` dependency rather than requiring consumers to enumerate LINK or Common source files.

## Release assets

A numbered LINK release publishes:

| File | Purpose |
| --- | --- |
| `LINK-<version>-source.zip` | Exact tested source archive, including the pinned dependency tree. |
| `SHA256SUMS.txt` | SHA-256 checksum for the source archive. |

LINK is a shared engine rather than an end-user application, so product installers remain in MBLINK and JAGLINK.

## Repository and release policy

This repository uses `main` as its working branch. Development changes are made directly on `main`; the normal project workflow does not depend on PR, feature or release branches.

Every push to `main` runs LINK CI. Ordinary commits do not publish. A commit is release-eligible only when its subject begins with the exact source version as `Release <version>` and the complete LINK CI run succeeds.

The publisher checks out the exact tested commit, verifies it is still current `main`, builds the source release and checksum, then creates the version tag and GitHub release. Existing version tags and published releases are immutable and are never moved, replaced or edited in place.

Manually runnable build/test helpers, where present, are diagnostic tools only and are not release-approval mechanisms.

## Engineering rules

- Broadly reusable non-automotive primitives belong in Infiltratr Common.
- Shared automotive behaviour and generic diagnostic knowledge belong in LINK.
- Manufacturer-specific definitions and behaviour remain in the product repositories.
- Multiple executables for one manufacturer do not require multiple repositories; keep the product family together and share mechanics through LINK.
- Public APIs document ownership, lifetime, failure behaviour and invariants.
- Source comments explain rationale and non-obvious state-machine constraints rather than narrating syntax.
- Unknown DTCs remain explicit and evidence-preserving; unknown or write-capable diagnostic actions remain denied unless an owning product explicitly introduces and validates them.

## Roadmap

The next diagnostic completion work is to connect the existing OBD freeze-frame/readiness primitives to the same resolved fault-record path and continue broadening the shared deep-reader mechanics only where a product can supply evidence-backed targets. LINK now provides prompt-safe ELM resynchronisation after an interrupted manufacturer extension and header-aware broad CAN route discovery for native OpenPort 2.0; product repositories decide when those mechanisms are appropriate for a verified vehicle family. The generic DTC catalogue is complete for the pinned OBDex snapshot and should be refreshed reproducibly when its upstream source changes. Remaining consolidation work includes packaging/CI helper convergence without pulling product branding into LINK.

## Licence

LINK is free software licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`). The vendored OBDex generic DTC data snapshot is CC0-1.0; its provenance is documented in `docs/DTC-CATALOGUE-SOURCE.md` and `third_party/obdex/SOURCE.md`.
