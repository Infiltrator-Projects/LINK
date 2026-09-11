# Apple provider boundary

LINK owns the reusable Apple transport/controller and the Apple portable-core build glue consumed by branded product faces.

The LINK-owned Apple amalgamation entry points are:

- `platform/apple/LinkPortableCore.c`
- `platform/apple/LinkPortableObd2.c`
- `platform/apple/LinkPortableUds.c`

Product repositories should consume these LINK files rather than creating product-owned translation units that directly include generic LINK implementation `.c` files.

## Manual Bluetooth device list

Connect opens the device picker before any adapter connection or vehicle diagnostic request. The nearby list displays every peripheral delivered by iOS discovery, including unfamiliar names, unnamed devices and the saved adapter when it is discovered. A saved identifier alone is not evidence that the device is nearby. Device-name recognition must not filter this manual list.

Scan Again clears the previous discovery results and restarts discovery. Repeated advertisements update the same device row by identifier, allowing a later advertisement to supply its name. The user selects which device to connect to; appearing in the list does not certify diagnostic compatibility. Automatic adapter selection can use compatibility hints separately. This list contains devices exposed by the iOS discovery API, not a copy of the system Bluetooth Settings list.

## Adapter backends are not vehicle-brand policy

Mercedes me native-adapter support is a LINK adapter/transport capability. The name identifies the hardware family that the backend speaks to; it does not restrict that hardware to Mercedes-Benz vehicles. If the adapter can carry the CAN/ISO-TP traffic required by a Ford, BMW, Audi, Jaguar or another supported vehicle family, the corresponding LINK product may use it.

`LINK_ENABLE_MERCEDES_ME_NATIVE` therefore defaults to `1`, including for non-Mercedes product faces. A product target may define it as `0` only when that target intentionally does not support the hardware backend for a technical or product-support reason. Vehicle manufacturer alone is not such a reason.

When the backend is intentionally disabled, the public adapter types remain available to the shared Apple controller while provider discovery/parser hooks resolve to inert inline stubs, allowing the Mercedes me implementation translation units to be omitted from that particular binary.

This keeps adapter support in LINK, keeps manufacturer-specific vehicle knowledge in the owning product repository, and allows every LINK-family product to use any LINK-supported adapter that is technically compatible with the vehicle and diagnostic traffic.

## Controller modularity rule

`LinkDiagnosticsController` is a public facade, not the permanent home for every Apple-side concern. New behaviour should live in the smallest component that owns the relevant state and lifecycle, with the controller delegating to it. The controller may coordinate components, expose stable public API, and translate component events for product faces, but it should not duplicate transport, scheduling, telemetry, persistence, localisation, or recovery state machines.

Refactors of the Apple controller must be behaviour-preserving and incremental. Each extraction must keep the public API stable unless an intentional API change is separately documented, keep one authoritative owner for each mutable state value, and add or retain regression coverage for the extracted behaviour before the old implementation is removed. Avoid large rewrites: one concern should be extracted and proven at a time.

The target ownership boundaries are:

- `LinkBLETransport`: Bluetooth discovery, peripheral identity, connection and byte transport.
- portable LINK core: ELM327 session parsing, diagnostic flow, scheduler, OBD/UDS/ISO-TP semantics and protocol-neutral telemetry structures.
- Apple session runner: command dispatch, timers and bridging portable flow actions/events to the active Apple transport.
- Apple polling coordinator: user PID enablement policy, scheduler application and live-polling restart/resume semantics.
- Apple telemetry recorder: session CSV ownership and recording/export plumbing.
- Apple settings store: language and measurement persistence/resolution.
- `LinkDiagnosticsController`: thin facade and coordinator over the above pieces.
- branded product repositories: manufacturer-specific vehicle knowledge and product presentation.

Current Apple source ownership also keeps vehicle/profile persistence in
`LinkVehicleProfileStore.h` / `LinkVehicleProfileStore.inc` and preference
persistence in `LinkAppleSettings.h` / `LinkAppleSettings.inc`. The `.inc`
implementation files are intentionally compiled through the existing controller
translation unit so pinned product Xcode projects do not need a source-list
migration merely to consume this behaviour-preserving refactor.

A change to one responsibility should not require unrelated components to know its internal state. In particular, branded products should be able to set generic PID selections without needing to understand when the LINK scheduler is constructed or when live polling needs to be kicked; LINK must guarantee that lifecycle internally.
