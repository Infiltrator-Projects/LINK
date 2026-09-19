# Design

## First-principles position

LINK treats standards, captures, public documentation and mature diagnostic tools as evidence. Product behaviour is implemented from explicit contracts rather than by copying another tool or assuming undocumented manufacturer behaviour.

## Goals

- define generic diagnostic semantics once
- keep transmit authority deny-by-default and independent from codec availability
- separate transport capability from manufacturer knowledge
- make external evidence provenance and licensed-standard boundaries explicit

## Non-goals

LINK is not a manufacturer database and adding a codec does not grant a product permission to transmit that service. Manufacturer-specific assumptions do not belong in the shared core.

## Safety model

Read-only and write-capable actions are intentionally distinct. Capability discovery, protocol support and operator permission are not interchangeable. Unknown or failed scan states remain different from a clean result.

## Dependency policy

Generic automotive behaviour belongs in LINK; broadly reusable non-automotive mechanics belong in Common; manufacturer-specific behaviour belongs in the product face. Exact dependency revisions are pinned so later upstream changes cannot silently redefine a reviewed product.

## Evidence rule

A human-readable interpretation must be traceable to a standard, capture, verified public source or reproducible vehicle observation. Where evidence is incomplete, preserve raw values and uncertainty rather than inventing a label.

## Change quality

Newness is not a reason to replace a proven path. A change should improve fidelity, safety, coverage, performance or maintainability and include regression evidence for the contract it changes.
