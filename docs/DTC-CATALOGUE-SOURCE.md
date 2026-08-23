# OBDex generic DTC catalogue source

LINK's complete generic OBD-II DTC catalogue is generated from OBDex, an open machine-readable generic DTC database whose data is dedicated to the public domain under CC0-1.0.

Pinned upstream snapshot: `foerbsnavi/OBDex@bc58b0eb7273226a1aabae98e956b70b8362bda1`.

The pinned snapshot contains 9,533 generic definitions across the seven SAE/ISO generic families: P0, P2, the standardized portion of P3, B0, C0, U0 and U3. LINK imports the five-character code, broad category and independently authored English title. Manufacturer-specific definitions are not imported into LINK and remain owned by manufacturer product repositories such as MBLINK and JAGLINK.

The generated `third_party/obdex/generic-dtcs.tsv` is the vendored normalized snapshot used for audit and reproducibility. `src/obd2/dtc_catalogue.inc` is the compiled lookup generated from the same input. `scripts/import-obdex-dtcs.py` validates exact family counts and rejects missing titles or duplicates before either file is generated.

SAE J2012 itself is copyrighted. LINK does not vendor or claim to reproduce the SAE publication. OBDex states that its descriptions are independently authored and licenses its data CC0-1.0.
