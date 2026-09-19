# Roadmap

This roadmap describes technical direction; the source, tests and evidence documents define current support.

## Current foundation

- maintain the shared OBD, UDS, ISO-TP, DoIP, KWP, adapter and diagnostic-flow foundations
- keep the generic DTC/standards catalogue provenance explicit
- support product faces through one installed/shared CMake contract

## Near-term priorities

- consolidate product-neutral application behaviour that is still duplicated above LINK
- expand protocol/adapter coverage only with explicit safety and evidence contracts
- continue strengthening native providers and embedded reuse

## Longer-term direction

- support additional diagnostic generations and transports without weakening existing Classical CAN paths
- grow shared application capability while preserving thin manufacturer product faces

## Admission rule

New diagnostic knowledge or capability requires clear ownership, evidence provenance, a safety classification and a realistic validation path.

## Completion rule

A roadmap item is complete only when implementation, safety policy, tests, platform integration and documentation agree. Raw discovery evidence is not automatically a supported decoded feature.
