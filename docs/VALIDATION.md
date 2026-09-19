# Validation

## Evidence model

Vehicle diagnostics needs several evidence layers: pure protocol/unit tests, captured-traffic replay, platform/adapter integration and physical vehicle validation. These layers complement one another but are not interchangeable.

## Automated gates

- .github/workflows/ci.yml
- .github/workflows/release.yml
- .github/workflows/refresh-obdex-dtc.yml

tests/ covers diagnostics flow/request planning, Discover safety/sweep, DoIP, DTC knowledge, ECU probing, ELM327 sessions, evidence, ISO-TP/UDS, OpenPort, STM32 host simulations, Apple regressions and installed CMake consumption.

## Physical/manual evidence

Physical USB/Bluetooth/J2534 adapters and real ECU/module behaviour require hardware/vehicle evidence. Licensed standards that cannot be redistributed must remain provenance boundaries rather than guessed compiled data.

A replay proves deterministic handling of that capture. It does not prove every adapter, ECU software version or vehicle topology. A simulator build proves source/platform integration, not physical Bluetooth/USB behaviour.

## Safety validation

Regression coverage must ensure that adding a codec, DID, module or transport cannot silently broaden transmit permissions. Failed/not-scanned/scanning/clean states remain semantically distinct.

## Release criterion

The exact source/dependency tree intended for release must pass the required CI gates. Release notes and documentation must reflect the evidence actually held for that revision.
