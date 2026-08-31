# OBDex generic PID snapshot

Upstream commit: `bc58b0eb7273226a1aabae98e956b70b8362bda1`
Data license: `CC0-1.0`

- Mode 01 definitions: **119**
- Mode 09 definitions: **13**
- Total definitions: **132**

LINK vendors the independently maintained OBDex data snapshot and generates
transport-neutral metadata from it. The standards engine preserves bitmap,
encoded, DTC, ASCII and other structured payloads rather than forcing every
PID into one scalar. Formula-backed entries are decoded by LINK's portable C11
core; manufacturer-specific data remains outside this catalogue.

SAE J1979 and its Digital Annex are copyrighted publications. This vendored
CC0 snapshot is not represented as a copy of those publications.
