<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# LINK

[![LINK CI](https://github.com/Infiltrator-Projects/LINK/actions/workflows/ci.yml/badge.svg)](https://github.com/Infiltrator-Projects/LINK/actions/workflows/ci.yml)

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

The standards-by-standards boundary is tracked in [docs/OBD-STANDARDS-COVERAGE.md](docs/OBD-STANDARDS-COVERAGE.md); transport support, semantic catalogue coverage and regulatory conformance are deliberately treated as separate claims.

LINK currently owns:

- platform-neutral byte-stream transport ABI;
- ELM327 framing, parsing, initialization and adapter/protocol probing;
- complete ELM327 first-generation OBD protocol selection/reporting for SAE J1850 PWM/VPW, ISO 9141-2, ISO 14230-4 KWP slow/fast initialisation, and all four ISO 15765-4 CAN variants;
- ELM327-managed ISO 15765 CAN channels and transport-backed sessions;
- a shared standard OBD-II/J1979 service model for modes `01` through `0A`,
  including transport-neutral read helpers, explicit deny-by-default handling
  for control/clear operations, responder-attributed capability discovery,
  VIN/readiness/freeze-frame/DTC handling and bounded multi-responder decoding;
- correct first-generation Service 05 TID + O2-sensor framing and scaling, structured CAN Service 06 OBDMID/TID/UASID decoding, and raw-preserving handling wherever a current licensed-annex semantic row is not compiled;
- the complete classic SAE J1979 Mode 01 identifier namespace: all **256** byte-wide slots are represented, with **220 assigned definitions** and **36** explicitly reserved/unassigned under the `J1979DA_201110+verified-public-updates` audit. Every advertised assigned data PID is schedulable, including structured/encoded/raw-preserving values rather than only the scalar compatibility subset; Mode 09 vehicle information and J1979-2 OBDonUDS mapping remain shared in LINK;
- the complete **65,536-value** SAE/ISO DTC numeric namespace, with every value classified as standardized generic/controlled, manufacturer-specific or document-reserved, plus a pinned **9,533-definition** open generic catalogue audited against J2012/J2012DA;
- ISO 14229 UDS request/response, DID and client-state handling;
- transport-neutral ISO 13400 DoIP framing for routing activation, alive-check and UDS diagnostic messages;
- a compiled 27-service ISO 14229 service catalogue and bounded request/response codecs;
- read-only UDS `ReadDTCInformation` helpers;
- ISO 14230-3 KWP2000 read-only response, TesterPresent, common-identifier, ECU-identification and DTC-by-status codecs;
- target-specific Discover probe sets for mixed-protocol networks, while retaining deny-by-default transmit safety;
- Classical CAN and CAN-FD ISO-TP, including CAN-FD payloads through 64 bytes and extended First Frame lengths for PDUs above 4095 bytes;
- bare-metal STM32 CAN/FDCAN edge with a bounded interrupt queue and direct ISO-TP/UDS orchestration;
- parameter definitions, store/history, scheduler and telemetry/CSV, with a
  separate responder-attributed history that leaves the legacy one-value-per-PID
  interface intact; streaming CSV schema v2 records the responder CAN ID and
  addressing width on every attributed sample;
- diagnostic workspace model, including a LINK-owned OBD workspace shared by
  every manufacturer face;
- evidence-based diagnostic-generation classification for legacy/OBD-I-era,
  transitional/"OBD1.5" and standard OBD-II/EOBD surfaces, without inferring
  capability from model year or connector shape;
- portable diagnostic-flow controller state machine;
- Discover safety classification and evidence writing;
- common ECU/module discovery, identification and raw-response acquisition primitives; and
- shared native Linux diagnostic-adapter layer for tty/RFCOMM, BlueZ BLE/GATT, BlueZ Classic/SPP, genuine Mercedes me Adapter native Bluetooth capture and direct-libUSB Tactrix OpenPort 2.0;
- shared native Windows OpenPort 2.0/J2534 Discover scanner shell.

The genuine Mercedes me Adapter is handled as a distinct native transport rather than being made to impersonate an ELM327. Archived official Mercedes me Adapter 4.7.61 interoperability evidence, including generation-name rules and exact Bluetooth UUIDs, is preserved in [`docs/MERCEDES-ME-ADAPTER-INTEROP.md`](docs/MERCEDES-ME-ADAPTER-INTEROP.md). LINK recognises the documented `MB-xxxx` Bluetooth name, can establish the available RFCOMM/GATT byte channel and passively preserve incoming bytes, while deliberately suppressing ELM `ATI`/initialisation traffic. The archived native command builders now prove the classic 11-bit raw-CAN and ISO-TP command framing, the secure envelope and the local challenge/session-key ordering. LINK therefore exposes a native Mercedes me diagnostic bridge that can compile the same transport-neutral request used by other adapters into the adapter's proved `I`/`i` command pair. A fresh secure login remains evidence-gated by the externally provisioned, adapter-specific Session Master Key and requires validation against physical hardware.
- [`docs/MERCEDES-ME-NATIVE-BINARIES.md`](docs/MERCEDES-ME-NATIVE-BINARIES.md) records the clean-room native GDK/DiagLogic/Whisper protocol evidence, secure envelope and proved command builders.
- [`docs/MERCEDES-ME-AUTH-FORENSICS.md`](docs/MERCEDES-ME-AUTH-FORENSICS.md) is the authoritative account of authentication ordering and the remaining Session Master Key boundary.
- [`docs/ADAPTER-CAPABILITIES.md`](docs/ADAPTER-CAPABILITIES.md) records the shared capability model and how one diagnostic definition is executed through ELM/Vgate, Tactrix/OpenPort, STM32 or the retired Mercedes me Adapter.

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

See `docs/DISCOVER.md`, `docs/PRODUCT_FACES.md` and
`docs/DIAGNOSTIC-GENERATIONS.md` for the repository/application and
diagnostic-generation boundaries.

## Architecture

Portable diagnostic behaviour is C11. C++ is used only where it materially improves a design. Platform-required languages remain narrow presentation or interop edges and must not become alternate protocol implementations.

Shared protocol state machines, diagnostic sequencing, safety policy, generic diagnostic knowledge and transport-independent decisions belong in LINK rather than Swift, Objective-C, GTK callbacks or Win32 message handlers.

Standard OBD knowledge follows the same rule. Product repositories must not grow private SAE PID tables. LINK's generated Mode 01/09 catalogue and `link/j1979da.h` are the shared source of truth for compiled public metadata and formulas. LINK separately records the current targets J1979_202505, J1979DA_202607, J1978-1_202604 and J1979-2_202604. Because the current J1979 Digital Annex is licensed and is not redistributed here, the compiled semantic baseline is labelled `J1979DA_201110+verified-public-updates`; unverified current rows remain raw rather than being guessed. Manufacturer UDS/KWP definitions are layered above that shared standards engine.

Compatibility aliases/wrappers preserve product-prefixed public APIs while the underlying ELM327, OBD-II, UDS, DTC knowledge and diagnostic-flow algorithms remain single-source in LINK.

The 27-service UDS implementation is compiled into `LINK::Core`; `include/link/uds_services.h` contains the public contract rather than per-consumer private implementations.

Bare-metal STM32 support keeps STM32Cube HAL outside the portable core: the host-tested queue/transaction glue lives in `platform/stm32`, while concrete Cube integration stays in `examples/stm32c092`. See `docs/STM32.md`.

## Build and test

```bash
git clone --recurse-submodules https://github.com/Infiltrator-Projects/LINK.git
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
