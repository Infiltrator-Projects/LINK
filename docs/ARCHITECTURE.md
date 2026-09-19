# Architecture

## Purpose

LINK is the shared product-neutral vehicle-diagnostics engine that owns transports, protocol codecs, diagnostic sequencing, evidence handling, safety policy and common application behaviour for the manufacturer product family.

## System decomposition

- portable diagnostics core
- transport and adapter providers
- OBD/UDS/KWP/DoIP protocol layers
- diagnostic sequencing and safety policy
- DTC/standards knowledge
- shared Discover/application models
- Apple/Linux/Windows provider boundaries
- STM32/embedded support and regression suites

## Ownership boundaries

Common owns broadly reusable non-automotive primitives. LINK owns product-neutral automotive behaviour. MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK own manufacturer-specific identity and knowledge.

Shared code flows downward through explicit dependencies. Product repositories should not copy shared protocol/session/application logic merely to customise manufacturer content. Conversely, manufacturer-specific evidence must not leak into LINK/Common abstractions.

## Contract boundaries

Protocol decoding, request planning, transport capability, safety permission and manufacturer interpretation are separate concerns. A decoder being able to represent a service does not imply that the product is allowed to transmit it.

## Source of truth

Code and tests define executable behaviour. Pinned gitlinks define dependency identity. Specialist evidence documents define narrower manufacturer/protocol facts and must not conflict with the ownership model above.

## Specialist documentation

- docs/PRODUCT_FACES.md
- docs/ADAPTER-CAPABILITIES.md
- docs/OBD-STANDARDS-COVERAGE.md
- docs/UDS.md
- docs/DISCOVER.md
- docs/APPLE-PROVIDER-BOUNDARY.md
- docs/STM32.md
