<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# ReadDTCInformation conformance matrix

This document is LINK's implementation map for ISO 14229 ReadDTCInformation
(`0x19`). It is not a substitute for the licensed standard. It keeps the
product-neutral codec auditable: request bytes and fixed response envelopes are
validated in LINK, while ECU-defined snapshot and extended-data lengths remain
opaque until a higher layer supplies evidence.

The catalogue retains the requested 2013-era 27-report surface:
`0x01..0x19` plus WWH-OBD `0x42` and `0x55`. Reports
`0x0F..0x13` remain for older ECUs and are marked withdrawn in the 2020
standard generation by the public catalogue.

## Matrix

| Report | Request after SID/report | Fixed positive-response data after report echo | LINK handling | 2020 |
| --- | --- | --- | --- | --- |
| `01` | status mask | status availability, DTC format, 16-bit count | typed count metadata | current |
| `02` | status mask | status availability, repeated DTC(3)+status | typed DTC/status | current |
| `03` | none | repeated DTC(3)+snapshot record number | typed snapshot-identification | current |
| `04` | DTC(3), record number | DTC(3)+status, then snapshot records | fixed envelope + raw tail | current |
| `05` | record number | record-number echo, then implementation-defined records | record echo + raw tail | current |
| `06` | DTC(3), record number | DTC(3)+status, then extended-data records | fixed envelope + raw tail | current |
| `07` | severity mask, status mask | status availability, DTC format, 16-bit count | typed count metadata | current |
| `08` | severity mask, status mask | status availability, repeated severity+functional-unit+DTC(3)+status | typed severity | current |
| `09` | DTC(3) | status availability, severity+functional-unit+DTC(3)+status | typed severity | current |
| `0A` | none | status availability, repeated DTC(3)+status | typed DTC/status | current |
| `0B` | none | status availability, DTC(3)+status when present | typed DTC/status | current |
| `0C` | none | status availability, DTC(3)+status when present | typed DTC/status | current |
| `0D` | none | status availability, DTC(3)+status when present | typed DTC/status | current |
| `0E` | none | status availability, DTC(3)+status when present | typed DTC/status | current |
| `0F` | status mask | status availability, repeated DTC(3)+status | typed DTC/status | withdrawn |
| `10` | DTC(3), record number | DTC(3)+status, then extended-data records | fixed envelope + raw tail | withdrawn |
| `11` | status mask | status availability, DTC format, 16-bit count | typed count metadata | withdrawn |
| `12` | status mask | status availability, DTC format, 16-bit count | typed count metadata | withdrawn |
| `13` | status mask | status availability, repeated DTC(3)+status | typed DTC/status | withdrawn |
| `14` | none | repeated DTC(3)+fault-detection counter | typed fault counter | current |
| `15` | none | status availability, repeated DTC(3)+status | typed DTC/status | current |
| `16` | record number | record-number echo, then implementation-defined records | record echo + raw tail | current |
| `17` | status mask, memory selection | memory echo, status availability, repeated DTC(3)+status | typed DTC/status | current |
| `18` | DTC(3), record number, memory selection | memory echo, DTC(3)+status, then snapshot records | fixed envelope + raw tail | current |
| `19` | DTC(3), record number, memory selection | memory echo, DTC(3)+status, then extended-data records | fixed envelope + raw tail | current |
| `42` | functional group, status mask, severity mask | functional group, status availability, severity availability, DTC format, repeated severity+DTC(3)+status | typed WWH severity | current |
| `55` | functional group | functional group, status availability, DTC format, repeated DTC(3)+status | typed DTC/status | current |

## Generic-versus-product boundary

This matrix describes generic LINK codec capability. It does not prove that a
specific ECU implements a report type. Product layers must establish target
support independently from documentation or reproducible captures.

Codec availability is also not transmission authority. LINK Discover remains
deny-by-default. ReadDTCInformation service `0x19` is deliberately permitted
as a read-only diagnostic service; ClearDiagnosticInformation service `0x14`
is blocked as DTC clearing; Authentication service `0x29` is blocked as a
security operation. ReadDTCInformation subfunction `0x14`
(reportDTCFaultDetectionCounter) is unrelated to service SID `0x14`
(ClearDiagnosticInformation).

## Regression contract

`tests/test_uds_dtc.c` carries request vectors for all 27 report types and
positive-response coverage for every response family above. It also exercises
the typed fixed-record views and rejects malformed/truncated envelopes.

`tests/test_uds.c` locks the service-effect classifications for `0x19`,
`0x14` and `0x29`, while `tests/test_discover_safety.c` independently
proves the actual transmit policy. The two contracts stay separate so adding a
codec can never silently broaden vehicle-write authority.
