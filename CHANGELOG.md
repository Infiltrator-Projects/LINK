# Changelog

This file records user-visible, compatibility, diagnostic-knowledge and validation changes for LINK.

## Unreleased

- No unreleased changes.

## 0.15.26 — 2026-09-19

- Upgraded the exact nested Infiltratr Common dependency to 1.19.4.
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
