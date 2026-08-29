<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Mercedes me Adapter interoperability evidence

This note preserves interoperability facts recovered from an archived official
**Mercedes me Adapter 4.7.61** Android application before those facts are
translated into LINK behaviour.

It is an evidence record, not imported application source. LINK does not copy
Daimler/T-Systems implementation code from the APK. Protocol identifiers,
device-name rules, transport characteristics, observed state names and backend
resource paths are recorded as interoperability facts and reimplemented
independently in C11.

## Evidence set

The analysed package identifies itself as:

- Android package: `com.daimler.mbfa.android`
- application version: `4.7.61`
- version code visible in the archived build: `4076103`
- framework namespace carrying the adapter implementation:
  `com.tsystems.cc.aftermarket.app.android...`

The locally analysed extracted files have these SHA-256 values:

| File | SHA-256 |
| --- | --- |
| `AndroidManifest.xml` | `cd047871724cb85bbb95aefa118e1b4bc6763c807d074626e4fd5944eb4c23b3` |
| `classes.dex` | `dbf7612636602e32f0ac9ce51d345d1298d4782d759938b12554dde9baa1cc46` |
| `classes2.dex` | `cc66c2f220d5ba1f5ae638b926418ac082248be7924ed73f3eda9b109b678a56` |
| `classes3.dex` | `83cd980cac55e517926469f165cdd83f55eddec7c45e1590c45b6686c5685ae0` |
| `classes4.dex` | `93df1e037e6b7eefa3e2b490ba1b0631e055f92cf4d35d500d6ea4de7f81b720` |
| `classes5.dex` | `482449473a2034bd3607eb3a4ade72fd1dd05c900c28d4f2bc7efc4a75eb8369` |
| `build-data.properties` | `bf1f5c8d3216ce1eddcc6ee8920234b2bb92a24d73bc964787ab36cb8fe2bdc9` |

Most transport evidence below is present in `classes3.dex`.

## Device-name families

`BackendAppApiConfiguration` contains the following default adapter-name
patterns as literal static values:

| Purpose in official app | Default pattern |
| --- | --- |
| BLE OBD adapter | `^MB-[189].*` |
| first-generation OBD adapter | `^MB-[2346].*` |
| second-generation OBD adapter | `^MB-[57].*` |
| adapter intended for other apps | `^VAN-.*` |

The runtime configuration keys corresponding to those defaults are
`bleObdAdapterPattern`, `firstGenerationObdAdapterPattern`,
`secondGenerationObdAdapterPattern` and `otherAppsObdAdapterPattern`.

A further literal `^MB-[02-46].*` exists in the DEX. Its owning semantic role
has not yet been proved strongly enough to promote into LINK behaviour, so it
is retained here as an unresolved observation rather than guessed.

`BluetoothLowEnergyObdAdapterNameMatcher.isPatternMatching` directly
references the BLE default `^MB-[189].*`.

## Bluetooth Classic transport

The official `BluetoothObdAdapterDevice` contains the standard Bluetooth
Serial Port Profile UUID:

`00001101-0000-1000-8000-00805F9B34FB`

and uses Android's
`createInsecureRfcommSocketToServiceRecord` path. This is direct evidence
that the Classic transport is RFCOMM/SPP; LINK should not search an unrelated
public-browse service before SPP for these adapters.

Static values recovered from that class are:

- Bluetooth connect timeout: **44,000 ms**
- minimum connection duration: **6,000 ms**

These timings are preserved as reference behaviour. LINK may use different
bounded platform timing where necessary, but any divergence should be
intentional and documented.

## Bluetooth Low Energy transport

The official `BleObdAdapterDevice` uses the Nordic UART Service identifiers:

- service: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E`
- RX/write characteristic: `6E400002-B5A3-F393-E0A9-E50E24DCCA9E`
- TX/notify characteristic: `6E400003-B5A3-F393-E0A9-E50E24DCCA9E`
- Client Characteristic Configuration descriptor:
  `00002902-0000-1000-8000-00805F9B34FB`

The same class has `REQUESTED_MTU = 512`. The value is evidence about the
official Android implementation; it does not imply that every LINK platform
can or should force MTU 512.

For an `MB-[189]...` device, these exact UUIDs are stronger evidence than
LINK's generic “find any write+notify pair” BLE heuristic and should be tried
first.

## Toshiba SPP-over-BLE identifiers

The official transport formatters also contain a second named channel:

- `TOSHIBA_SPP_OVER_BLE_SERVICE_UUID`:
  `e079c6a0-aa8b-11e3-a903-0002a5d5c51b`
- `TOSHIBA_SPP_OVER_BLE_CHARACTERISTIC_UUID`:
  `b38312c0-aa89-11e3-9cef-0002a5d5c51b`

These constants appear in both `BleFormatter` and `BluetoothFormatter`.
They are useful fallback/protocol evidence, but the current evidence does not
prove that every Mercedes me Adapter generation exposes this channel. LINK
must therefore prefer the generation-specific NUS/SPP path and treat Toshiba
SPP-over-BLE as a corroborated fallback rather than a universal requirement.

## Adapter lifecycle and security/session evidence

The APK contains the adapter lifecycle operations:

- `setupObdAdapter`
- `readObdAdapterData`
- `deactivateSppServerMode`
- `detectAndCheckObdAdapter`
- separate `createClassicObdAdapterDevice` and
  `createBleObdAdapterDevice` paths.

It also contains a Session Master Key subsystem with types and states including:

- `SessionMasterKey`
- `AbstractSessionMasterKeyProvider`
- `SessionMasterKeyProviderFactory`
- `loadSessionMasterKey`
- `SMK_INVALID_PASSKEY`
- `SMK_NOT_AVAILABLE`
- `SMK_BLOCK_TEMPORARY`
- backend error names such as `FW_CORE_BE_SRV_SMK_INVALID_PASSKEY`.

A backend resource literal is present:

`POST tenants/{tenantId}/obdAdapters/{obdAdapterId}/v2/smk`

and the adapter resource itself appears as:

`GET tenants/{tenantId}/obdAdapters/{obdAdapterId}`

This is strong evidence that a commissioned adapter session can depend on
backend-provided/keyed state rather than being a completely unauthenticated
serial telemetry stream. It is **not** evidence for an SMK algorithm or packet
format. LINK must not invent one. Until the local wire framing is independently
recovered, native Mercedes me operation remains read/passive or limited to
requests whose bytes and security semantics are proven.

The package also contains communication outcomes including
`DEVICE_NO_SPP`, consistent with SPP availability being an explicit part of
the adapter state model.

## Cryptographic dependency evidence

The extracted `build-data.properties` identifies the build target
`tink-android-unshaded.jar`, showing that Google's Tink Android library is
present in the archived application dependency set. The separate SMK classes
prove that the application has session-key handling. The current evidence does
**not** yet prove that Tink is the implementation used for the Mercedes me SMK
wire protocol, so LINK must keep those two facts separate until call-level
evidence connects them.

## Implementation consequences for LINK

The evidence supports these changes without guessing:

1. classify Mercedes adapter families from the official default name rules;
2. distinguish BLE `MB-[189]` from first/second-generation Classic families;
3. use standard RFCOMM/SPP for Classic Mercedes me adapters;
4. prefer the exact Nordic UART service/characteristics for BLE Mercedes me
   adapters;
5. retain Toshiba SPP-over-BLE as a known fallback candidate;
6. never send ELM `ATI`, `ATZ` or other ELM setup traffic to a native
   Mercedes me adapter;
7. preserve raw native RX evidence until packet framing is proved;
8. keep SMK/setup/read command bytes evidence-gated.

## Evidence still required

The APK has not yet yielded, with sufficient confidence, all of the following:

- native application packet framing;
- checksum/length/escaping rules;
- exact request bytes for `setupObdAdapter` and `readObdAdapterData`;
- SMK derivation/use on the local Bluetooth channel;
- mapping from native payload fields to fuel level, odometer, trip and other
  vehicle values;
- whether the Toshiba SPP-over-BLE channel applies to a specific hardware
  generation or is only a formatter compatibility path.

Those items should be added here as they become reproducible. A real adapter
capture remains the final check that archived-app behaviour matches the
specific A2138203202 hardware under test.
