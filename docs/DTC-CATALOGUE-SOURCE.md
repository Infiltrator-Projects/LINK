<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# SAE generic DTC catalogue source

LINK owns the shared generic diagnostic trouble-code catalogue used by product faces. The catalogue is standards-oriented data; manufacturer-specific definitions remain in MBLINK, JAGLINK or another owning product.

The complete-table import is normalized from the GPLv3 PiOBDII ISO/SAE DTC dataset by Jason Birch. That project explicitly notes that its descriptions were gathered and reformatted from multiple sources, so LINK preserves provenance and treats the imported text as a reviewed data snapshot rather than claiming it is a verbatim publication of SAE J2012.

Import rules:

- retain only syntactically valid five-character DTC records;
- preserve explicit ISO/SAE reserved records as reserved, not known faults;
- keep manufacturer-specific ranges out of the generic definition table;
- reject duplicate codes with conflicting descriptions;
- generate the compiled lookup deterministically from the vendored normalized snapshot; and
- test representative Powertrain, Body, Chassis and Network definitions plus reserved/unknown behavior.

Source: `BirchJD/PiOBDII`, `DATA/TroubleCodes-ISO-SAE.txt`, GPL v3 or later project terms. Imported snapshot source blob: `4c695c631bf867f0255782a81258c89aa9888305`.
