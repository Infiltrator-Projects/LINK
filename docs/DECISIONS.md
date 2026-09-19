# Decisions

This file records durable architectural choices for LINK.

## ADR-001 — LINK owns product-neutral automotive behaviour

**Decision.** Shared transports, protocols, sequencing, safety/evidence and diagnostic application mechanics live here; manufacturer knowledge stays in product repositories.

**Rationale.** One implementation gives all product faces the same protocol and safety semantics.

**Consequence.** LINK must remain manufacturer-neutral even when development is driven by one vehicle.

## ADR-002 — Common stays below the automotive boundary

**Decision.** Broadly reusable mechanics belong in Common; automotive concepts remain in LINK.

**Rationale.** Common should not become coupled to vehicle diagnostics simply because LINK needs a utility.

**Consequence.** Shared dependency direction is Common → LINK → product.

## ADR-003 — Decode, transport and permission are independent

**Decision.** A protocol codec, an adapter's ability to carry a frame and the safety policy allowing a request are separate contracts.

**Rationale.** Protocol completeness must never silently enable new vehicle actions.

**Consequence.** Regression suites test safety/allowlists independently of codec coverage.

## ADR-004 — Standards metadata is evidence-labelled

**Decision.** Public/redistributable standards knowledge is compiled with explicit provenance; unavailable licensed current data is not guessed.

**Rationale.** A fabricated "current" catalogue would be less trustworthy than an older verified baseline plus raw unknowns.

**Consequence.** Catalogue source/version labels remain visible in code/docs.

## ADR-005 — Platform providers adapt the shared engine

**Decision.** BlueZ, CoreBluetooth, J2534, direct USB and embedded transports remain providers behind shared transport/diagnostic contracts.

**Rationale.** Platform APIs should not fork diagnostic state machines.

**Consequence.** Provider-specific quirks stay isolated and shared request/response semantics remain portable.

## ADR-006 — Evidence is retained without granting semantic authority

**Decision.** Captures, mature tools and reverse-engineered material are inputs to analysis, not runtime dependencies or automatic specifications.

**Rationale.** External behaviour can be incomplete, version-specific or wrong.

**Consequence.** Reviewed LINK code and tests define the supported product contract.