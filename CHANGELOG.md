# Changelog

This file records user-visible, compatibility, diagnostic-knowledge and validation changes for LINK.

## Unreleased

- No unreleased changes.

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
