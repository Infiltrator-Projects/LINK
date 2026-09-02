<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Diagnostic generations in LINK

LINK supports a family of vehicle diagnostic generations rather than assuming
that every vehicle is full OBD-II.

## Legacy / OBD-I-era

Pre-OBD-II diagnostics are mainly manufacturer-specific. LINK owns the common
capability model, presentation and evidence rules. The manufacturer product owns
the evidence-backed legacy protocol knowledge.

A legacy tier is selected only after an actual legacy diagnostic exchange is
positively identified. Age, connector shape and brand are not enough.

## Transitional / "OBD1.5"

"OBD1.5" is an informal label for vehicles that sit between fully
manufacturer-specific legacy diagnostics and the later standardized OBD-II/EOBD
experience. It is not a universal SAE or ISO standard.

LINK classifies a session as transitional only when both of these are observed:

1. a valid standards-shaped OBD response; and
2. a positively identified legacy diagnostic response supplied by the
   manufacturer layer.

The 1999 AU Falcon is a project regression/example target for this category.
FORDLINK supplies Ford-specific legacy knowledge; LINK supplies the common
classification and user interface.

## Standard OBD-II / EOBD

When a standards-shaped OBD surface answers and no legacy surface has been
positively identified, LINK presents the standard OBD-II/EOBD tier. This is a
functional observation rather than a regulatory-conformance claim.

The LINK-owned standard surface includes supported-PID discovery, live Mode 01
data, readiness, Mode 02 freeze-frame context, stored/pending/permanent DTCs,
Mode 09 vehicle information, responder attribution and the OBDonUDS foundation.

## Ownership and presentation

The diagnostic-generation model belongs to LINK, but diagnostic generation is
not a top-level operator destination. OBD-II, OBDonUDS, UDS and legacy
manufacturer protocols feed the shared task-oriented screens:

- Vehicle receives identity, protocol, network and module inventory.
- Errors receives standard and manufacturer DTCs.
- Table, Dashboard and Graph receive standard and manufacturer live parameters.
- Tests receives readiness, monitor results and supported self-tests.
- Services receives only explicitly supported procedures and remains
  deny-by-default for unsafe or unknown operations.
- Log receives the chronological evidence trail.

Manufacturer repositories own only manufacturer-specific protocol knowledge and
capabilities layered into those LINK-owned tasks.
