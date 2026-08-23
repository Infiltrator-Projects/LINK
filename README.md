<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK

[![LINK CI](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml/badge.svg)](https://github.com/The-First-Infiltrator/LINK/actions/workflows/ci.yml)

LINK is the shared C11 vehicle-diagnostics and application engine used by MBLINK and JAGLINK.

**Current source version:** 0.9.1 (next release: 0.10.0)  
**Shared foundation:** Infiltratr Common 1.11.0  
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

## Capabilities

LINK currently owns:

- platform-neutral byte-stream transport ABI;
- ELM327 framing, parsing, initialization and adapter/protocol probing;
- ELM327-managed ISO 15765 CAN channels and transport-backed sessions;
- standard OBD-II requests, PID/VIN/readiness/DTC decoding;
- shared generic DTC knowledge: normalized code classification, human-readable definitions for a growing standards-backed catalogue, subsystem/category metadata, explicit unknown-code handling and ISO 14229 status translation;
- ISO 14229 UDS request/response, DID and client-state handling;
- read-only UDS `ReadDTCInformation` helpers;
- Classical CAN ISO-TP;
- parameter definitions, store/history, scheduler and telemetry/CSV;
- diagnostic workspace model;
- portable diagnostic-flow controller state machine;
- Discover safety classification and evidence writing; and
- shared native Windows OpenPort 2.0/J2534 Discover scanner shell.

The DTC knowledge API is presentation-neutral and deliberately preserves the raw ECU code. A valid code that LINK does not yet know remains an explicit unmapped diagnostic result; no product face is allowed to fabricate a description. Manufacturer-specific descriptions remain in the owning product repository rather than leaking into LINK.

The initial 0.10 catalogue concentrates on high-value engine/diesel diagnostics shared by MBLINK and JAGLINK, including fuel delivery/rail pressure, injectors, boost, engine-position sensing, EGR, misfire, glow-plug/preheat, DPF/EGT/NOx aftertreatment and common vehicle-network communication faults. Structured cylinder families are generated deterministically rather than duplicated as UI strings.

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

## Architecture

Portable diagnostic behaviour is C11. C++ is used only where it materially improves a design. Platform-required languages remain narrow presentation or interop edges and must not become alternate protocol implementations.

Shared protocol state machines, diagnostic sequencing, safety policy, generic diagnostic knowledge and transport-independent decisions belong in LINK rather than Swift, Objective-C, GTK callbacks or Win32 message handlers.

Compatibility aliases/wrappers preserve product-prefixed public APIs while the underlying ELM327, OBD-II, UDS, DTC knowledge and diagnostic-flow algorithms remain single-source in LINK.

## Build and test

```bash
git clone --recurse-submodules https://github.com/The-First-Infiltrator/LINK.git
cd LINK
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build --build-config Release --output-on-failure
```

GitHub Actions builds and tests the portable core on Linux, macOS and Windows. The DTC knowledge suite checks known-code translation, structured cylinder mappings, manufacturer-specific unknown handling, network-code metadata, malformed-code rejection and shared UDS status semantics. The Windows configuration also proves that the same shared Discover implementation can produce both MBLINK and JAGLINK product faces.

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
- Public APIs document ownership, lifetime, failure behaviour and invariants.
- Source comments explain rationale and non-obvious state-machine constraints rather than narrating syntax.
- Unknown DTCs remain explicit and evidence-preserving; unknown or write-capable diagnostic actions remain denied unless an owning product explicitly introduces and validates them.

## Roadmap

The next diagnostic completion work is to connect the existing OBD freeze-frame/readiness primitives to the same resolved fault-record path, then continue expanding the standards-backed generic DTC catalogue. Remaining consolidation work also includes reusable BLE transport coordination, additional shared Linux/iPhone application structure where genuinely common, and further packaging/CI helper consolidation without pulling product branding into LINK.

## Licence

LINK is free software licensed under the GNU General Public License version 3 or, at your option, any later version (`GPL-3.0-or-later`).
