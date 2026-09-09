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
