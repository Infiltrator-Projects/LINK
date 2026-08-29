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


## Native GDK / DiagLogic architecture recovered from the DEX

The Java framework does not contain the complete adapter state machine. It
crosses into native code through JNA/JNI.

`HwDeviceLibraryProvider.getInstance()` loads the native library name
`gdk` through `NativeLibraryLoader.load("gdk", IHwDeviceLibrary.class)`.
The Java interface exposes:

- `checkStateOrdinal()`
- `deactivateSppServerMode()`
- `getQosStateOrdinal()`
- `getReasonOrdinal()`
- `interruptDeadManThead()`
- `registerStateListenerCallback(...)`
- `removeAllBoundedDevices()`
- `removeNotConnectedBoundedDevices()`
- `setReasonOrdinal(int)`
- `start(adapterName, SessionMasterKeyProviderCallback, boolean)`
- `stop()`

The diagnostic engine similarly loads native library name `diaglogic`.
`IDiagLogicLibrary` exposes `diagLogicConfigure`,
`diagLogicGetActiveImplementationName`, `diagLogicRequestRepeatCycleStep`,
`diagLogicReset` and `diagLogicSelectImplementation`. A separate
`DiagLogicJni` class exposes native `preview()` and `runCycleStep()`
methods returning byte arrays.

On a normal Android installation these library names conventionally resolve to
ABI-specific shared objects such as `libgdk.so` and `libdiaglogic.so`.
Those native objects were not among the extracted files supplied for this
analysis, so the lower command generator, SMK application and vehicle decoder
remain outside the present evidence set.

## Java/native Bluetooth bridge and stream contract

The native `gdk` engine reaches Bluetooth through
`BtConnectionGdkImp`. Its `BtOpenComm(byte[])` converts the adapter name to
a Java string, creates either the Classic or BLE device, connects it, creates a
blocking queue and starts dedicated read/write threads. `BtWriteComm(byte[])`
queues native-generated bytes; `BtReadThread.doRx(byte[])` is itself native,
so complete received records are returned to the native engine.

This bridge gives LINK the first defensible byte-stream framing rules:

- outbound commands are rejected unless their final byte is **0x0D (CR)**;
- inbound **0x0D (CR)** terminates a record and is included in the byte array
  delivered to native `doRx`;
- inbound **0x07 (BEL)** is handled as a special **NACK terminator**, is
  included in the delivered record, and closes that record immediately;
- the read accumulator is **700 bytes**;
- at position **698** an unterminated record is treated as overflow and the
  accumulator is cleared;
- the read-thread static wait value is **5** and the static NACK/CR values are
  7 and 13 respectively.

This proves a command/record-oriented transport boundary rather than an
arbitrary opaque binary stream. It does **not** prove that every payload byte
between delimiters is printable ASCII, nor does it reveal the native command
vocabulary.

The official write-thread shutdown sentinel is a one-byte CR command. LINK does
not need to reproduce that Java thread-control mechanism; it is recorded only
to distinguish transport lifecycle behavior from an adapter command.

## Native state and reason ordinals

The Java wrappers index enum arrays directly with integer ordinals returned by
`gdk`, so these numeric values are part of the observed native ABI.

### QoS state

| Ordinal | State |
| ---: | --- |
| 0 | `QOS_STOPPED` |
| 1 | `QOS_GOOD` |
| 2 | `QOS_WEAK` |
| 3 | `QOS_BAD` |

### Execution state

| Ordinal | State |
| ---: | --- |
| 0 | `SUCCESS` |
| 1 | `STATE_ERROR` |
| 2 | `COMMUNICATION_ERROR` |
| 3 | `ADAPTER_ERROR` |

### Reason

| Ordinal | Reason |
| ---: | --- |
| 0 | `R_STOPPED` |
| 1 | `R_STARTED` |
| 2 | `R_NAME_NOT_FOUND` |
| 3 | `R_NO_PING` |
| 4 | `R_WRONG_PARTMU_VERSION` |
| 5 | `R_BT_NOT_AKTIVE` |
| 6 | `R_DEVICE_DISAPPEARED` |
| 7 | `R_DEVICE_NO_SPP` |
| 8 | `R_NOT_CONNECTABLE` |
| 9 | `R_ILLEGAL_STATE_BT_DISCOVERY` |
| 10 | `R_CONNECT_TIMEOUT` |
| 11 | `R_SMK_NOT_AVAILABLE` |
| 12 | `R_SMK_INVALID_PASSKEY` |
| 13 | `R_SMK_BLOCK_TEMPORARY` |
| 14 | `R_ADAPTER_ERROR_NO_AUTHENTICATION_01` |
| 15 | `R_ADAPTER_ERROR_AUTHENTICATION_FAILED_02` |
| 16 | `R_ADAPTER_ERROR_ADC_SELFTEST_ERROR_03` |
| 17 | `R_ADAPTER_ERROR_MAC_ERROR_04` |
| 18 | `R_ADAPTER_ERROR_SELFTEST_ERROR_05` |
| 19 | `R_ADAPTER_ERROR_INVALID_MAC_MAPPING_11` |
| 20 | `R_ADAPTER_ERROR_BT_RX_OVERFLOW_18` |
| 21 | `R_ADAPTER_ERROR_CAN_OFF_21` |
| 22 | `R_ADAPTER_ERROR_BT_TX_OVERFLOW_30` |
| 23 | `R_ADAPTER_ERROR_CAN_RX_OVERFLOW_31` |
| 24 | `R_ADAPTER_ERROR_CAN_TX_OVERFLOW_32` |
| 25 | `R_BUS_ERROR_35` |
| 26 | `R_GATT_FAILURE` |

The separate Java Bluetooth bridge has observed communication result ordinals:
`STARTED=1`, `NAME_NOT_FOUND=2`, `BT_NOT_AKTIVE=5`,
`DEVICE_NO_SPP=7`, `NOT_CONNECTABLE=8`,
`ILLEGAL_STATE_BT_DISCOVERY=9`, `CONNECT_TIMEOUT=10` and
`GATT_FAILURE=11`.

## DiagLogic protobuf output schema

A complete embedded `Values.proto` schema is present in `classes3.dex`
under package `diaglogic.api`. The important structures are:

- `Dtc`: required trouble-code string, required sporadic/static flag,
  optional display text and optional responded-device ID;
- `Value`: typed boolean, double, long, string or byte-array value;
- `MeasuredItem`: required data ID, optional unit/value, optional responding
  device address and timestamp;
- `MeasuredItemCollection`: required requested-device ID plus repeated
  measured items;
- `DtcCollection`: required requested-device ID plus repeated DTCs;
- `VehicleConfiguration`: version, variant and timestamp;
- `VehicleStatus`: assigned VIN, error code/message, DTC collections,
  measured items, vehicle configuration and OBD-adapter software version;
- `DiagPreview`: cycle-completed flag, pending-action token and repeatable
  flag.

The distinction between **requested device ID** and **responding device
address** is explicit in this schema and should be preserved when LINK/MBLINK
eventually ingest native DiagLogic output.

The archived application also exposes 115 exact diagnostic/live-data IDs.
They are preserved separately in
[`MERCEDES-ME-DATA-IDS.md`](MERCEDES-ME-DATA-IDS.md) so the catalogue can be
used as an implementation checklist without bloating the transport notes.

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
8. expose the proved CR/NACK record framing and native state ordinals in portable code;
9. keep SMK/setup/read command bytes evidence-gated.


## Additional native callback, error-code and logging vocabulary

A deeper pass over the Java/native boundary exposed one native callback sentinel
that must **not** be mistaken for a `Reason` enum ordinal. In
`StateListenerCallback.notifyReasonChanged(int)`, integer **4711** causes
`notifySleepCommandSent()` and returns immediately. Every other value follows
the normal `Reason.values()[ordinal]` path. LINK therefore records 4711 as a
separate sleep-command callback sentinel.

The application-layer `ConnectionProblem` enum contains 29 ordered entries
with explicit user-facing/error-telemetry codes:

| Ordinal | Connection problem | Error code |
| ---: | --- | ---: |
| 0 | `NO_PROBLEM` | 0 |
| 1 | `DEVICE_NOT_PAIRED` | 600 |
| 2 | `NO_PING` | 601 |
| 3 | `WRONG_PARTMU_VERSION` | 602 |
| 4 | `BLUETOOTH_DISABLED` | 603 |
| 5 | `DEVICE_DISAPPEARED` | 604 |
| 6 | `DEVICE_NO_SPP` | 605 |
| 7 | `NOT_CONNECTABLE` | 606 |
| 8 | `ILLEGAL_STATE_BT_DISCOVERY` | 607 |
| 9 | `CONNECT_TIMEOUT` | 608 |
| 10 | `NO_BLUETOOTH_AVAILABLE` | 609 |
| 11 | `UNKNOWN` | 610 |
| 12 | `SMK_NOT_AVAILABLE` | 611 |
| 13 | `SMK_INVALID_PASSKEY` | 612 |
| 14 | `SMK_BLOCK_TEMPORARY` | 613 |
| 15 | `ADAPTER_ERROR_BT_RX_OVERFLOW_18` | 618 |
| 16 | `ADAPTER_ERROR_CAN_OFF_21` | 621 |
| 17 | `ADAPTER_ERROR_BT_TX_OVERFLOW_30` | 630 |
| 18 | `ADAPTER_ERROR_CAN_RX_OVERFLOW_31` | 631 |
| 19 | `ADAPTER_ERROR_CAN_TX_OVERFLOW_32` | 632 |
| 20 | `BUS_ERROR_35` | 635 |
| 21 | `ADAPTER_ERROR_NO_AUTHENTICATION_01` | 641 |
| 22 | `ADAPTER_ERROR_AUTHENTICATION_FAILED_02` | 642 |
| 23 | `ADAPTER_ERROR_ADC_SELFTEST_ERROR_03` | 643 |
| 24 | `ADAPTER_ERROR_MAC_ERROR_04` | 644 |
| 25 | `ADAPTER_ERROR_SELFTEST_ERROR_05` | 645 |
| 26 | `ADAPTER_ERROR_INVALID_MAC_MAPPING_11` | 646 |
| 27 | `PARALLEL_OBD_ADAPTER_CONNECTED` | 647 |
| 28 | `GATT_FAILURE` | 650 |

These are distinct from the lower GDK `Reason` ordinals and should remain
distinct in LINK.

Useful official logging identifiers recovered from the same DEX are:

- `GdkConnectionLog`: `connection attempt`, `connection end`,
  `connection start`, `connection trigger`, and ` time to connect: `;
- domain events: `ConnectionAttempted`, `ConnectionEnded`,
  `ConnectionStarted`, `ConnectionTriggered`, with `|` as the observed
  separator;
- log tags: `carla-ble-----------`, `carla-fw-bluetooth--`,
  `carla-connection----`, `carla-gdkjava-------` and
  `carla-fw-status-----`.

These strings are useful when correlating archived app logs with native
captures. They are evidence labels, not commands to the adapter.

## Evidence still required

The APK has not yet yielded, with sufficient confidence, all of the following:

- the command vocabulary and payload syntax inside the now-proved CR/NACK record framing;
- checksum/length/escaping rules;
- exact request bytes for `setupObdAdapter` and `readObdAdapterData`;
- SMK derivation/use on the local Bluetooth channel;
- mapping from native command/DiagLogic payload bytes to the now-known data IDs such as fuel level, odometer, trip and other vehicle values;
- whether the Toshiba SPP-over-BLE channel applies to a specific hardware
  generation or is only a formatter compatibility path.

Those items should be added here as they become reproducible. A real adapter
capture remains the final check that archived-app behaviour matches the
specific A2138203202 hardware under test.
