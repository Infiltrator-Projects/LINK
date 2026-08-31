# OBD generic PID sources

## Pinned OBDex base snapshot

Upstream commit: `bc58b0eb7273226a1aabae98e956b70b8362bda1`
Data license: `CC0-1.0`

- Mode 01 definitions: **119**
- Mode 09 definitions: **13**
- Base total: **132**

LINK vendors the independently maintained OBDex data snapshot and generates
transport-neutral metadata from it. The pinned files remain unchanged; LINK
keeps later assignments and corrections outside the snapshot so its provenance
continues to be auditable.

## LINK current-standard supplement

LINK independently curates later assigned classic Mode 01 identifiers in
`src/obd2/pid_standard_supplement.inc`. Supplement v2 extends the public
assignment catalogue through PID `0xDA` and includes the `0xE0` support
page. Combined with the pinned base, LINK exposes **234** Mode 01/09
definitions.

Public assignment cross-check:

- OBD Fusion supported SAE PID index:
  https://www.obdsoftware.net/software/obdfusion/supported-sensors

Open machine-readable layout/formula cross-check:

- OBDb SAE J1979 signal set:
  https://github.com/OBDb/SAEJ1979
- OBDb data license: `CC-BY-SA-4.0`

LINK does not copy descriptive prose from either source. Assigned identifiers
whose public byte layout/scaling has not been independently corroborated are
catalogued as `LINK_OBD2_VALUE_RAW` with zero minimum payload length. Their
wire bytes remain available to products, but LINK emits no invented numeric
value. Corroborated modern layouts are decoded in the portable C11 core.

The base snapshot's PID `0x9E` metadata says `g/s`; current public data
corroborates the standard exhaust-flow unit as `kg/h`. LINK applies that
correction at integration time without altering the pinned OBDex files.

## J1979-2 / OBDonUDS identifier space

The classic Mode 01 API intentionally remains an 8-bit PID API. J1979-2 uses
UDS ReadDataByIdentifier with two-byte DIDs. LINK therefore exposes an explicit
mapping API: logical PID `0x000-0x0FF` maps to DID `F400-F4FF`, and logical
PID `0x100-0x1FF` maps to DID `F500-F5FF`. This prevents an extended
identifier such as `0x100` from ever being emitted as an invalid classic
Mode 01 request.

SAE J1979, SAE J1979-2 and their Digital Annexes are copyrighted publications.
Neither the pinned CC0 snapshot nor LINK's independently curated supplement is
represented as a copy of those publications.
