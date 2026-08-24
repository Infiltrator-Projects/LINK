<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK

[![LINK CI](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml)

LINK is the shared C11 vehicle-diagnostics and application engine used by MBLINK and JAGLINK.

**Current source version:** see [`VERSION`](VERSION)  
**Shared foundation:** exact Infiltratr Common release/commit pinned by `src/infiltratr-common` and `CMakeLists.txt`  
**Platforms:** Linux, Windows, macOS/iOS-facing portable core  
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
- parameter definitions, store/history, scheduler and telemetry/CSV;
- diagnostic workspace model;
- portable diagnostic-flow controller state machine;
- Discover safety classification and evidence writing;
- common ECU/module discovery, identification and raw-response acquisition primitives; and
- shared native Windows OpenPort 2.0/J2534 Discover scanner shell.

Codec support does not grant transmit permission. Discover remains independently deny-by-default; adding a UDS codec cannot silently broaden its request allowlist.

The DTC knowledge API is presentation-neutral and deliberately preserves the raw ECU code. Generic definitions come from a reproducible pinned OBDex CC0 data snapshot. SAE J2012 itself is not vendored or reproduced. A valid reserved or otherwise unmapped generic code remains explicit, and manufacturer-specific descriptions remain in the owning product repository rather than leaking into LINK.

The catalogue is generated deterministically by `scripts/import-obdex-dtcs.py`. The generator validates exact family counts and a total of 9,533 unique generic definitions before producing the vendored normalized snapshot and compiled C lookup. The resolver uses that compiled table directly, so product builds require no network access or runtime data files.

The shared diagnostic-flow controller owns the normal product-neutral sequence:

```text
ELM init
  → standard OBD-II PID discovery
  → optional manufacturer extension hook
  → optional ELM restore
  → stored / pending / permanent DTC inventory
  → live-data scheduler
  → live PID decode
```

MBLINK uses the extension hook for Mercedes-specific probing. JAGLINK currently skips it. Manufacturer logic therefore stays above LINK while the generic sequencing remains shared.

## Discover application model

Discover is not a separate product repository. It is a specialist branded application target inside each manufacturer repository:

```text
LINK shared Discover engine
       ↓                 ↓
MBLINK Discover     JAGLINK Discover
 Mercedes face       Jaguar face
```

The current implementation provides passive CAN capture plus a bounded read-only standard OBD inventory. Its intended evolution is deeper manufacturer-aware module discovery, ECU identification, documented read-only acquisition and structured evidence/dump export using the same shared engine.

That evolution must not fork the generic scanner. Mercedes-specific module topology, identifiers and probes belong in MBLINK; Jaguar-specific equivalents belong in JAGLINK. The reusable state machine, transports, safety classifier, evidence model and platform shell stay here.

See `docs/DISCOVER.md` and `docs/PRODUCT_FACES.md` for the repository/application boundary.

## Architecture

Portable diagnostic behaviour is C11. C++ is used only where it materially improves a design. Platform-required languages remain narrow presentation or interop edges and must not become alternate protocol implementations.

Shared protocol state machines, diagnostic sequencing, safety policy, generic diagnostic knowledge and transport-independent decisions belong in LINK rather than Swift, Objective-C, GTK callbacks or Win32 message handlers.

Compatibility aliases/wrappers preserve product-prefixed public APIs while the underlying ELM327, OBD-II, UDS, DTC knowledge and diagnostic-flow algorithms remain single-source in LINK.

The 27-service UDS implementation is compiled into `LINK::Core`; `include/link/uds_services.h` contains the public contract rather than per-consumer private implementations.

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

GitHub Actions builds and tests the strict portable core on Linux, macOS and Windows and runs ASan+UBSan on Linux. The DTC knowledge suite enforces the exact 9,533-definition catalogue size and pinned upstream snapshot, samples all seven generic families, checks generic/manufacturer range boundaries, preserves lowercase normalization and malformed-code rejection, and verifies shared UDS status semantics. The ISO-TP suite covers preserved Classical CAN behaviour as well as CAN-FD single-frame, multi-frame and extended-length traffic. The Windows configuration proves that the same shared Discover implementation can produce both MBLINK and JAGLINK product faces.

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

The next diagnostic completion work is to connect the existing OBD freeze-frame/readiness primitives to the same resolved fault-record path. In parallel, Discover should evolve from its present passive CAN/bounded OBD baseline into the shared deep read-only ECU/module reader: manufacturer-aware module inventory, identity acquisition and structured evidence/dump export, while keeping all generic mechanics single-source in LINK. The generic DTC catalogue is complete for the pinned OBDex snapshot and should be refreshed reproducibly when its upstream source changes. Remaining consolidation work includes reusable BLE transport coordination and further packaging/CI helper consolidation without pulling product branding into LINK.

## Licence

LINK is free software licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`). The vendored OBDex generic DTC data snapshot is CC0-1.0; its provenance is documented in `docs/DTC-CATALOGUE-SOURCE.md` and `third_party/obdex/SOURCE.md`.
