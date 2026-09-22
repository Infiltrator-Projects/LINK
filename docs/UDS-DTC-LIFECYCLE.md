# UDS DTC lifecycle reference engine

LINK separates two jobs that are easy to confuse:

1. **UDS ReadDTCInformation framing** — how services such as 0x19/0x14
   serialize a DTC fault-detection counter.
2. **Diagnostic monitor lifecycle** — how an ECU decides that a monitored
   condition is prefailed, failed, confirmed, healed or aged.

The first is protocol behaviour. The second is product/vehicle diagnostic
policy. ISO 14229 exposes the status and scaled counter but does not prescribe
one universal OEM debounce algorithm. LINK therefore provides a configurable,
allocation-free reference lifecycle instead of pretending that one manufacturer's
monitor policy is universal.

## Reference state

`LinkUdsDtcLifecycleDefinition` supplies application metadata and policy:

- 24-bit DTC;
- FunctionalGroupIdentifier;
- severity and functional unit;
- failed/pass counter step sizes;
- failed-operation-cycle count required for confirmation;
- passed-operation-cycle count required for aging.

`LinkUdsDtcLifecycleState` owns the changing state:

- the standard DTC status byte;
- signed fault-detection counter;
- aging counter;
- consecutive failed-cycle counter;
- whether the monitor was tested, fully failed or fully passed in the current
  operation cycle.

## Fault-detection counter

The reference engine uses the ISO-scaled signed range:

- `0` at the beginning of an operation cycle;
- `+1 .. +126` means positive/pre-failed progress and is reportable through
  `reportDTCFaultDetectionCounter (0x19/0x14)`;
- `+127` means the monitor reached failed and is **not** emitted as a
  pre-failed FDC record;
- `-1 .. -127` is pass-direction progress;
- `-128` means the monitor reached fully passed.

The counter is reset to zero by
`link_uds_dtc_lifecycle_begin_operation_cycle()`. This prevents an internal
debounce accumulator from being incorrectly carried across diagnostic operation
cycles.

The increment/decrement steps are policy, not protocol. A target can choose
different values or replace the reference lifecycle entirely while continuing
to use LINK's UDS codecs.

## Status transitions

A partial positive FDC does not yet assert `testFailed`.

When the counter reaches +127, the reference engine sets:

- `testFailed`;
- `testFailedThisOperationCycle`;
- `pendingDTC`;
- `testFailedSinceLastClear`.

At the operation-cycle boundary a fully failed cycle increments the
confirmation counter. Once the configured number of failed cycles has completed,
`confirmedDTC` is asserted.

A monitor that reaches -128 in a later operation cycle is a fully passed cycle.
Passed cycles clear pending state and increment the aging counter. Once the
configured aging threshold is reached, `confirmedDTC` is cleared.

A cycle in which the monitor never reaches fully failed or fully passed does
not confirm or age the DTC.

The portable definitions contain all eight ISO status bits, but the STM32F103
reference lifecycle itself owns bits 0 through 6 only. It does not model a
warning-indicator request, so that application advertises a DTC status
availability mask of `0x7F`, not `0xFF`. A product that actually owns a
warning indicator can advertise bit 7 through its own DTC store.

## ClearDiagnosticInformation

`link_uds_dtc_lifecycle_clear()` resets:

- FDC to zero;
- aging counter to zero;
- confirmation-cycle counter to zero;
- fault/pending/confirmed state.

It leaves the status byte at:

`testNotCompletedSinceLastClear | testNotCompletedThisOperationCycle`

which is `0x50`.

That is why `19 01 FF` is not a valid assertion that a successful clear must
produce zero DTC definitions: an 0xFF status mask also selects the two
not-completed bits. A fault-oriented mask such as 0x0D is the appropriate
reference check for testFailed/pending/confirmed state.

## STM32F103 reference integration

The STM32F103 ECU example uses the shared lifecycle engine rather than manually
inventing status transitions.

Its supported-DTC catalogue contains the 66 definitions supplied with LINK
#42's diagnostic workbook, replacing the old three synthetic placeholders.
The workbook-derived U300614/U300615 records use FunctionalGroupIdentifier
0x33; the C1006xx safety/chassis records use 0xD0. Severity and functional-unit
metadata are retained from the workbook-derived table. The reference monitor
uses ±64 steps, two failed operation cycles for confirmation, and three fully
passed operation cycles for aging.

Monitor samples are **RAM-local**. LINK does not erase/program the STM32 flash
journal every time a diagnostic monitor runs. The reference ECU persists the
completed-cycle state once at the operation-cycle boundary, plus explicit
state-changing diagnostic operations such as ClearDiagnosticInformation.

The flash journal is slot-based and crash-consistent. A slot consumes
`ceil(sizeof(LinkStm32F103PersistentState) / page_size)` reserved pages; the
page ring must contain at least two complete slots. LINK erases/programs the
next complete slot, reads it back and validates the full-state CRC before
publishing it as the active generation. A torn or corrupt multi-page
generation is therefore rejected on reboot and the previous valid slot remains
recoverable. The two-page journal remains a compatibility case when the whole
state fits in one physical page.

For demonstration, extended-data record 1 contains four application-defined
bytes:

1. aging counter;
2. failed-operation-cycle counter;
3. raw signed FDC byte;
4. FunctionalGroupIdentifier.

This four-byte layout belongs to the STM32F103 reference application; it is not
presented as a universal ISO or Mercedes extended-data layout.
