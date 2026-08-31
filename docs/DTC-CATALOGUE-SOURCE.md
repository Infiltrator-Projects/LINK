# Generic OBD-II DTC catalogue provenance

LINK's generic diagnostic-trouble-code knowledge is generated from OBDex, an
open machine-readable DTC database whose data is dedicated to the public domain
under CC0-1.0.

Pinned upstream snapshot:
`foerbsnavi/OBDex@bc58b0eb7273226a1aabae98e956b70b8362bda1`.

At the 2026-09 audit this commit is also the head of OBDex `main`, so LINK is
not knowingly pinned behind its lawful source.

The pinned snapshot contains **9,533 assigned generic definitions** across the
seven SAE/ISO-controlled families represented by the source:

- P0: 3,705
- P2: 3,495
- P34xx assigned standardized portion: 155
- B0: 323
- C0: 626
- U0: 1,055
- U3: 174

LINK imports only the five-character code, broad category and independently
authored English title. Manufacturer-specific definitions are never imported
into LINK and remain owned by product repositories such as MBLINK and JAGLINK.

## Range model audit

The ownership/range model was re-audited against **SAE J2012_202509** (current
main revision at audit time) and the **J2012DA_202607** Digital Annex revision
metadata. Publicly accessible ISO 15031-6 range information was used as an
independent cross-check.

LINK does **not** copy SAE definition text. The current range model is:

- B0 and C0: standard-controlled; B1/B2 and C1/C2: manufacturer-specific;
  B3/C3: reserved by the standards document.
- P0 and P2: standard-controlled; P1: manufacturer-specific.
- P3000-P33FF: manufacturer-specific; P3400-P3FFF: standard-controlled.
- U0 and U3: standard-controlled; U1/U2: manufacturer-specific.

A number merely being inside a standard-controlled range does not prove that a
generic definition has been assigned. LINK therefore distinguishes:

- `STANDARD_GENERIC`: the pinned open catalogue contains an assigned
  definition;
- `STANDARD_CONTROLLED`: the number is in a standards-controlled range but no
  open definition is present;
- `MANUFACTURER_SPECIFIC`: the number belongs to an OEM-controlled range;
- `DOCUMENT_RESERVED`: B3/C3 ranges reserved by the standards document.

This prevents both failure modes that matter to a generic diagnostic library:
using an OEM definition for a standardized number, and inventing a generic
meaning merely because a number sits inside an SAE/ISO-controlled range.

## Reproducibility and qualification

`third_party/obdex/generic-dtcs.tsv` is the normalized vendored snapshot.
`src/obd2/dtc_catalogue.inc` is generated from the same input.
`scripts/import-obdex-dtcs.py` validates exact family counts, syntax, titles
and duplicate codes before generating either file.

Runtime tests additionally enumerate **every one of the 9,533 compiled
definitions**, require strict sort/uniqueness, verify source/title/category,
resolve every entry back through the public lookup, reproduce all seven family
counts, and exercise every J2012 family-boundary transition listed above.

SAE J2012 and its Digital Annex are copyrighted publications. They are revision
and range-audit references only. LINK's compiled English definitions come from
the CC0 OBDex snapshot, not from copied SAE text.
