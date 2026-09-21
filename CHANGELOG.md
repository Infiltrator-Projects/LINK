# Changelog

## 0.15.40 — 2026-09-21

- Repair the Apple portable-core amalgamation exposed by the suite-wide About rollout: include the shared selection engine now required by LINK::Core.
- Keep the Apple bridge validator and actual compiled source topology in exact agreement so product faces can consume the current LINK revision without copying core source lists.
- Preserve the 0.15.39 suite-standard About contract unchanged.


## 0.15.39 — 2026-09-21

- Complete the suite-wide About contract by supplying the canonical Build field to LINK's standard SwiftUI product face.
- Repair the Win32 Discover About metadata initializer after the shared About model gained a Build field; use designated initializers so future model growth cannot silently shift identity fields.
- Give native Windows Discover surfaces an explicit build identity while preserving the shared Website, Credits, Licence and Close structure.


This file records user-visible, compatibility, diagnostic-knowledge and validation changes for LINK.

## 0.15.38 — 2026-09-21

- Added request-aware ReadDTCInformation response validation so transaction code verifies request-specific echoes instead of accepting a merely parseable byte layout.
- Validated MemorySelection for 0x17..0x19, record-number echoes for 0x05/0x16, functional-group echoes for 0x42/0x55, and requested-DTC echoes for the applicable snapshot, extended-data and severity reports.
- Added regression vectors for malformed user-memory responses that omit MemorySelection, matching the failure class found during forensic comparison with the externally supplied STM32 server.
- Kept transmission policy unchanged: 0x19 remains read-only; ClearDiagnosticInformation 0x14 remains state-changing; Authentication 0x29 remains security-gated.

## 0.15.37 — 2026-09-21

- Standardised LINK-owned About presentation on the System Monitor contract across Linux, Windows and Apple surfaces.
- Removed forced Linux About sizing, private About CSS and product-tagline hierarchy so GTK owns the native dialog geometry.
- Added a first-class build label to the shared About model and aligned the main hierarchy to icon, product, version, description, build, website and copyright.
- Kept Credits and Licence as dedicated actions on every supported presentation surface; Windows now exposes native Credits and Licence buttons instead of flattening those details into the main text.
- Standardised the visible website label to `Website` and Australian `Licence` wording in LINK-owned custom surfaces.

## 0.15.36 — 2026-09-21

- Forensically revalidated the complete requested ReadDTCInformation (0x19) surface across all 27 report types and added an explicit implementation conformance matrix.
- Corrected response validation for 0x19/0x04, 0x06, 0x10, 0x18 and 0x19 so truncated positive responses can no longer be accepted without their mandatory DTC-and-status envelope; user-defined-memory responses also require the memory-selection echo.
- Added allocation-free typed record views for fixed DTC/status, snapshot-identification, severity, fault-detection-counter and WWH severity records while retaining implementation-defined snapshot/extended-data tails as raw bounded spans.
- Expanded ReadDTCInformation regression coverage to every requested report family, including malformed fixed-envelope cases.
- Locked the service-effect contract in tests: 0x19 remains read-only, 0x14 ClearDiagnosticInformation remains state-changing, and 0x29 Authentication remains security-gated.
- Expanded ClearDiagnosticInformation codec tests for optional memory selection and 24-bit group bounds. No Discover permission was broadened.

## 0.15.35 — 2026-09-21

- Completed a second forensic Common 1.19.20 forward-consumption pass without changing Common.
- Replaced remaining allocation-heavy GLib ASCII case-folding in the Linux Bluetooth provider with Common's deterministic ASCII comparison, substring and hexadecimal-classification contracts.
- Replaced the Windows About dialog's remaining direct LoadLibrary/GetProcAddress plumbing with Common's UTF-8-aware dynamic-library boundary.
- Reused Common's ASCII whitespace/case conversion in simulator/session normalization, Common's finite clamp in dashboard gauge normalization and Common's saturating counter increment in telemetry sequence handling.
- Deliberately retained protocol-specific narrow whitespace grammars, streaming JSON writers and GLib-native timing where the existing implementation is semantically narrower or more efficient than the available Common primitive.

## 0.15.34 — 2026-09-21

- Completed the Common 1.19.20 forward-consumption pass in the Linux GTK shell.
- Replaced the shell's GLib monotonic-clock wrapper with Common's canonical monotonic nanosecond provider.
- Moved LINK language-preference XDG path resolution, recursive directory creation and atomic persistence onto Common's POSIX contracts while retaining LINK's own language policy and INI schema.
- Replaced the language-pack parser's locale-sensitive ctype whitespace/case conversion with Common's deterministic ASCII contracts.
- Kept language-pack discovery and GTK-native UI concerns in the Linux presentation layer; no code or policy was added to Common.

## 0.15.33 — 2026-09-21

- Advanced LINK from Infiltratr Common 1.19.10 to 1.19.20 and pinned the exact released Common commit.
- Removed LINK-local ASCII case-folding and whitespace helpers now owned by Common, keeping protocol and adapter matching locale-independent.
- Replaced the Windows Discover J2534 loader's direct LoadLibrary/GetProcAddress plumbing with Common's UTF-8-aware dynamic-library boundary and atomic symbol binding.
- Reused Common's monotonic-clock adapter and saturating deadline arithmetic in the native Linux OpenPort provider.
- Linked platform UI providers to Common's full platform target only where those platform services are actually required; the portable LINK core remains on Common::Portable.

## 0.15.32 — 2026-09-20

- Published the repository-wide copyright normalization already present on main.
- No diagnostic, transport, safety, Vehicle Research, API or presentation behaviour changed from 0.15.31.

## 0.15.31 — 2026-09-20

- Added LINK's portable Vehicle Research session state with explicit passive-capture, standards-inventory, manufacturer-sweep, paused and complete phases.
- Extended the shared evidence stream with typed research-session, research-phase, event-marker and research-summary records while preserving every raw frame.
- Upgraded the shared Windows Discover face into the first Vehicle Research workspace slice: passive capture, bounded OBD inventory, product sweep, operator event markers and research export now share one deterministic session timeline.
- Kept the existing deny-by-default transmission policy unchanged; research additions do not enable reset, security, routine, clear, coding or programming operations.

## 0.15.30 — 2026-09-20

- Advanced the exact nested Infiltratr Common dependency to 1.19.10.
- Preserved LINK's protocol, transport and presentation contracts while exposing Common 1.19.10's expanded product-neutral Night/Day design palette transitively to LINK-based product faces.
- Kept automotive semantics in LINK and generic design ownership in Common; no duplicate theme wrapper or second palette source was introduced.

## 0.15.29 — 2026-09-19

- Completed a forensic Common 1.19.8 reuse pass across portable codecs and platform providers, replacing parallel endian, checked-size, allocation-growth, strict parsing, bounded-copy and array-sizing mechanics with the canonical Common APIs while retaining the existing protocol and transport contracts.
- Centralised Apple standard OBD-II value text rendering through LINK's shared preference-aware formatter so Swift, GTK and C presentation paths use the same deterministic conversion and precision policy.
- Corrected the standalone native-Linux provider smoke target to link Common explicitly now that the provider edge consumes Common primitives directly.

## 0.15.28 — 2026-09-19

- Advanced the exact Infiltratr Common dependency to 1.19.8, retaining LINK's shared formatting APIs while inheriting Common's consolidated parsing, ASCII, checked-size and POSIX numeric mechanics.

## 0.15.27 — 2026-09-19

- Centralised fuel-economy/trip value rendering in LINK so product faces share unit conversion, precision and unavailable/stationary semantics.
- Added regression coverage for metric and US-customary fuel-economy presentation.

## 0.15.26 — 2026-09-19

- Upgraded the exact nested Infiltratr Common dependency to 1.19.7.
- Reused Common's deterministic fixed-point and CSV-field encoders in shared telemetry output.
- Added shared preference-aware OBD-II value formatting so product faces no longer duplicate unit/precision policy.
- Added a bounded decoded-PID summary formatter for product-neutral standard-data presentation.
- Added regression coverage for shared display formatting and CSV control/quote handling.

## 0.15.25 — 2026-09-19

- Upgraded the exact nested Infiltratr Common dependency to 1.19.3.
- Added a protocol-neutral, vehicle-scoped parameter selection model so product faces can keep explicit choices isolated by vehicle identity.
- Generalised bounded session traces from OBD-only PID identity to LinkParameterKey while retaining the standard OBD compatibility API.
- Expanded the graph capacity to the complete 256-entry standard PID space and added safe runtime reconfiguration of the selected graph set.
- Centralised ELM327 CAN header and receive-address command formatting for reuse by manufacturer products.
- Added regression coverage for vehicle isolation, selections beyond eight channels, manufacturer-capable trace identity, and shared CAN address formatting.
- Canonical documentation baseline aligned with the Infiltrator project family.

## Policy

Record new supported diagnostic behaviour, changed safety/permission boundaries, corrected definitions, platform changes, dependency revisions that alter behaviour and material fixes. Raw research activity belongs in the relevant evidence document until it changes supported product behaviour.

## Historical identity

Git tags and GitHub Releases remain authoritative for exact historical source and dependency identity.
