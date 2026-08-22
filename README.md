# LINK

LINK is the shared C11 vehicle-diagnostics/application engine used by MBLINK and JAGLINK.

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns portable facilities useful outside vehicle diagnostics. LINK owns automotive functionality shared by the products. Product repositories own branding, metadata and genuinely manufacturer-specific behaviour.

## LINK 0.9.1 shared implementation

LINK 0.9.1 pins Infiltratr Common 1.11.0 and fixes the public OBD-II unit contract so `link/obd2.h`, `link/parameter.h` and the shared diagnostic-flow API can be included together without duplicate C enumerators.

LINK owns:

- platform-neutral byte-stream transport ABI;
- complete ELM327 command/parser/init engine;
- ELM327 adapter/protocol probing;
- ELM327-managed ISO 15765 CAN channel;
- transport-backed ELM327 command session;
- standard OBD-II request, PID, VIN, readiness and DTC decoding;
- ISO 14229 UDS request/response, DID and client-state handling;
- read-only UDS `ReadDTCInformation` helpers;
- diagnostic workspace model;
- Classical-CAN ISO-TP;
- parameter definitions/store/history, scheduler and telemetry/CSV;
- shared portable diagnostic-flow controller state machine;
- Discover safety/evidence;
- Windows OpenPort 2.0/J2534 Discover scanner and shared native Windows shell.

The diagnostic-flow controller owns the normal product-neutral sequence:

```text
ELM init
  → standard OBD-II PID discovery
  → optional manufacturer extension hook
  → optional ELM restore
  → stored / pending / permanent DTC inventory
  → live-data scheduler
  → live PID decode
```

MBLINK uses the extension hook for its Mercedes-Benz ECU probe. JAGLINK currently skips it. This keeps manufacturer logic above LINK while preventing Apple, Linux or Windows front ends from reimplementing the generic diagnostic state machine.

The ELM327, OBD-II and UDS migrations preserve the existing product-prefixed public APIs through thin compatibility aliases/wrappers while removing duplicated algorithms from MBLINK and JAGLINK.

## Remaining migration

1. move reusable BLE recovery/discovery policy behind a platform-neutral C transport coordinator;
2. consolidate genuinely shared Linux application structure;
3. consolidate genuinely shared iPhone application structure while keeping Swift/Objective-C at the presentation and platform edges;
4. consolidate packaging/CI/release helpers and common assertions.

## Engineering rules

C is the default implementation language; C++ is used where it materially improves a design. Platform-required languages remain narrow UI/interop edges. Shared state machines, protocol decisions, safety policy and diagnostic sequencing belong in C/C++ in LINK, not in Swift, Objective-C, GTK callbacks or Win32 message handlers. Public APIs document ownership, lifetime, failure behaviour and invariants; source comments explain rationale and non-obvious state-machine constraints without narrating obvious syntax.

SPDX-License-Identifier: GPL-3.0-or-later
