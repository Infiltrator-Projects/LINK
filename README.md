# LINK

LINK is the shared C/C++ vehicle-diagnostics and application engine used by MBLINK and JAGLINK. The portable implementation is currently C11; C++ is permitted where it provides a concrete technical benefit.

```text
Infiltratr Common
        ↓
       LINK
      ↙    ↘
  MBLINK  JAGLINK
```

Infiltratr Common owns portable facilities useful outside vehicle diagnostics. LINK owns automotive functionality shared by the products. Product repositories own branding, metadata and genuinely manufacturer-specific behaviour.

## LINK 0.8.1 shared implementation

LINK now owns:

- platform-neutral byte-stream transport ABI;
- complete ELM327 command/parser/init engine;
- ELM327 adapter/protocol probing;
- ELM327-managed ISO 15765 CAN channel;
- transport-backed ELM327 command session;
- diagnostic workspace model;
- Classical-CAN ISO-TP;
- standard SAE OBD-II requests, PID discovery/decoding, readiness, VIN and DTC handling;
- ISO 14229 UDS request/response, DID and client-state handling;
- read-only UDS ReadDTCInformation decoding and DTC status helpers;
- parameter definitions/store/history, scheduler and telemetry/CSV;
- Discover safety/evidence;
- Windows OpenPort 2.0/J2534 Discover scanner.

MBLINK and JAGLINK retain product-prefixed source compatibility at their public boundaries, but LINK is the source of truth for every item above. Product repositories must not carry a second implementation of LINK-owned behaviour.

## Remaining migration

1. portable diagnostic-session orchestration currently embedded in Apple controllers;
2. platform-neutral portions of BLE adapter discovery/recovery, leaving CoreBluetooth as a thin Apple edge;
3. genuinely shared Linux application structure;
4. genuinely shared iPhone application model, leaving SwiftUI as presentation only;
5. packaging/CI/release helpers.

## Language boundary

Shared application and diagnostic behaviour belongs in C or C++. Objective-C, Swift, Win32 glue, GTK callbacks and other platform-specific code are adapters around the C/C++ engine, not alternate implementations of it. If logic can be expressed without an operating-system UI/framework type, it belongs in the C/C++ layer.

The current portable core remains strict C11. Public APIs document ownership, lifetime, failure behaviour and invariants; source comments explain rationale and non-obvious state-machine constraints without narrating obvious syntax.

SPDX-License-Identifier: GPL-3.0-or-later
