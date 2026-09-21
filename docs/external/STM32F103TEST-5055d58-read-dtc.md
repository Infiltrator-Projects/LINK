<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# STM32F103TEST ReadDTCInformation forensic repair map

This note records the LINK issue #31 review of the reporter's current external
project:

- repository: `leoembeddeder/STM32F103TEST`
- audited commit: `5055d58f4f6240488271d85cb177f71337e0cdeb`
- primary file: `Library/StoredDataTransmissionFunctionalUnit.c`

LINK cannot push to that repository through the connected GitHub installation
(`push: false`). This document therefore fixes the ambiguity in the issue by
giving an exact migration target and by locking the malformed external response
families against LINK's tested server implementation. The clean replacement
path for STM32F103 is the complete ECU/server reference in
`examples/stm32-bxcan/issue-32/`.

## Confirmed external defects

### 0x17 reportUserDefMemoryDTCByStatusMask

The external dispatch currently calls:

`NumberOfDTCByStatusMask(..., userDefMemory = true)`

That is the wrong response family. 0x17 is a DTC-list report, not a count
report. Its positive response must carry, after `59 17`, the requested
MemorySelection echo, DTCStatusAvailabilityMask, then zero or more
DTC(3)+status records.

LINK's equivalent implementation is
`LINK_UDS_DTC_REPORT_USER_MEMORY_BY_STATUS_MASK` in
`link_uds_server_dtc_handler()`.

### 0x18 reportUserDefMemoryDTCSnapshotRecordByDTCNumber

The external function reads `receiveBuffer[6]` as MemorySelection for lookup,
but its positive response starts `59 18` and then writes DTC/status directly.
The mandatory MemorySelection response echo is missing.

Correct fixed prefix after the report echo:

`MemorySelection, DTC[3], statusOfDTC`

followed by the snapshot record payload.

### 0x19 reportUserDefMemoryDTCExtDataRecordByDTCNumber

The same defect exists in the external extended-data path. MemorySelection is
used for lookup but not emitted in the positive response.

Correct fixed prefix after the report echo:

`MemorySelection, DTC[3], statusOfDTC`

followed by the extended-data record payload.

### 0x12 / 0x13 emissions reports

At the audited commit both functions return SubfunctionNotSupported. They are
part of the requested 2013-era surface and therefore cannot be described as
implemented by that external project. LINK retains them as withdrawn-in-2020
compatibility reports and tests both families.

### 0x14 reportDTCFaultDetectionCounter

The external handler returns SubfunctionNotSupported. A positive response is
`59 14` followed by zero or more `DTC[3], faultDetectionCounter` records.

Do not confuse ReadDTCInformation subfunction 0x14 with service SID 0x14
ClearDiagnosticInformation.

### 0x15 reportDTCWithPermanentStatus

The external handler returns SubfunctionNotSupported. A positive response is
`59 15, DTCStatusAvailabilityMask` followed by zero or more
`DTC[3], statusOfDTC` records for permanent-status DTCs.

### 0x42 reportWWHOBDDTCByMaskRecord

The external implementation emits:

`59 42, functionalGroupIdentifier, DTCStatusAvailabilityMask, ...records`

and then severity / functional-group / DTC / status records.

The fixed response envelope is incomplete. The LINK 2013 compatibility
contract requires after `59 42`:

`functionalGroupIdentifier, DTCStatusAvailabilityMask,
DTCSeverityAvailabilityMask, DTCFormatIdentifier`

followed by the WWH severity records defined by the selected standard
generation.

### User-defined-memory storage

The external write path still contains a TODO indicating that writing DTCs into
user-defined memory has not been implemented. Even after repairing 0x17/0x18/
0x19 response bytes, those reports require real backing data before they can be
meaningfully exercised.

## Tested replacement path

LINK's `link_uds_server_dtc_handler()` has a single bounded implementation for
all 27 requested ReadDTCInformation report types:

`0x01..0x19, 0x42, 0x55`.

The host regression suite covers every request and response family, malformed
fixed envelopes and request-specific echoes. The STM32 transport regression
covers segmented 0x19 traffic, FlowControl, physical/functional addressing and
deferred request races.

For the reporter's STM32F103 hardware, the issue-32 reference ECU additionally
provides persistent rich DTC records and routes its service 0x19 directly to
that shared handler. Moving the external project to that handler removes the
parallel, divergent 0x19 implementation rather than continuing to patch 27
nearly-duplicate functions.

## External-project completion condition

LINK issue #31 can be closed only after one of these is true:

1. the external owner applies equivalent repairs/replaces the local 0x19 server
   with LINK and provides a successful physical-board retest; or
2. repository write permission is granted and the correction can be committed
   and qualified there.

Until then LINK can prove the correct implementation and provide the migration,
but it cannot truthfully claim the independently owned repository or board is
fixed.
