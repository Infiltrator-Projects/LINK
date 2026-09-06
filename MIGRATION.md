# LINK migration status

Dependency hierarchy: `Infiltratr Common -> LINK -> MBLINK / JAGLINK / BMWLINK / AUDILINK / FORDLINK`.

LINK's initial extraction from the manufacturer repositories is complete. The current work is consolidation: shared automotive behaviour remains single-source in LINK, broadly reusable non-automotive primitives move downward into Infiltratr Common, and manufacturer knowledge stays in the owning product repository.

## Current LINK ownership

LINK is the shared vehicle-diagnostics and application engine for the complete LINK product family. Shared ownership includes:

- the operator-task workspace and information architecture used by the main diagnostic applications;
- Classical-CAN and CAN-FD ISO-TP;
- the byte-stream transport ABI and shared adapter/provider contracts;
- ELM327 framing, parsing, initialisation, protocol probing, managed CAN channels and command sessions;
- standard SAE OBD-II/J1979 request/response, PID, readiness, VIN, freeze-frame and DTC behaviour;
- generic DTC knowledge and classification;
- ISO 14229 UDS request/response, DID, client-state and standard service codecs;
- shared KWP2000 and DoIP foundations where product-neutral;
- parameter definitions, storage/history, scheduling and telemetry/CSV;
- portable product-neutral diagnostic-flow orchestration, including the manufacturer-extension boundary;
- diagnostic-generation classification;
- Discover scanner mechanics, safety classification, evidence writing, ECU/module interrogation primitives and shared platform shell;
- shared native Linux diagnostic-adapter support and common Linux application-shell behaviour;
- shared Apple/iPhone presentation contracts and reusable diagnostic controller behaviour while Swift/Objective-C remain presentation/platform edges;
- shared Windows Discover infrastructure;
- shared language selection, measurement conversion and standard About behaviour;
- shared Dashboard presentation modes and other application behaviour that must remain consistent across manufacturer faces; and
- common build, packaging and release helpers where the behaviour is genuinely product-neutral.

Product-prefixed files may remain as compatibility aliases or thin adaptors while older public APIs are retired. They must not contain a second implementation of LINK-owned behaviour.

## Ongoing consolidation

Remaining migration work is not a second architectural phase. It is the continuing removal of duplication and historical compatibility layers as the common engine evolves. Typical candidates are:

1. product-prefixed wrappers whose only purpose is to expose an older name for a LINK implementation;
2. repeated platform-shell mechanics that can become one LINK implementation without carrying manufacturer branding or knowledge with them;
3. common CI, packaging and release assertions that are still duplicated between product repositories; and
4. generic automotive behaviour discovered while implementing one manufacturer product that is equally valid for the rest of the LINK family.

A consolidation is complete only when LINK is the source of truth, affected LINK-family products consume the shared implementation, duplicate behaviour is removed, regression tests pass, and documentation records the ownership accurately.

## Product repository boundary

Manufacturer repositories own identity and genuinely manufacturer-specific content. Mercedes, Jaguar, BMW, Audi and Ford definitions, topology, diagnostic endpoints, proprietary identifiers, manufacturer DTC knowledge, evidence-backed probes and decoders, branding, product metadata and manufacturer-specific workflows remain in their product repositories.

Product applications also own the composition and appearance of their Settings pages. LINK supplies shared capabilities such as language selection and measurement conversion but does not own favourites policy, unavailable-value policy, adapter/About rows inside Settings, or a complete Settings screen. About is a separate shared LINK capability.

Protocols such as OBD-II, OBDonUDS, UDS and manufacturer legacy diagnostics are data sources beneath the shared operator-task interface; they are not competing top-level navigation destinations. Module inventory belongs within the shared task model rather than reintroducing a separate generic Modules workspace.

## Placement rule

When reviewing a change:

1. If unrelated software could reuse the primitive, it belongs in Infiltratr Common.
2. If multiple LINK-family products should behave the same way, it belongs in LINK.
3. If the only differences are name, icon, colours or metadata, parameterise a LINK-owned behaviour rather than copy it.
4. If the knowledge or behaviour is genuinely manufacturer-specific, keep it in the product repository.

SPDX-License-Identifier: GPL-3.0-or-later
