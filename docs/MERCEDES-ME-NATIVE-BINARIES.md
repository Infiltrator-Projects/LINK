<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Mercedes me Adapter native-library evidence

This note preserves interoperability facts recovered from the native Android libraries supplied from the archived official Mercedes me Adapter 4.7.61 application. It is intended for the current `Infiltrator-Projects/LINK` repository.

Authentication call-flow analysis continued after this inventory was written. [`MERCEDES-ME-AUTH-FORENSICS.md`](MERCEDES-ME-AUTH-FORENSICS.md) is authoritative for the random-value direction, GetSeed/SetKey ordering and Session Master Key boundary; this note remains authoritative for the binary inventory, framing, builders, limits and DiagLogic/Whisper evidence.

## Evidence set

All analysed vehicle-stack libraries are 32-bit little-endian ARM ELF shared objects for Android 21. The main stack was built with Android NDK r24; `libSAL.so` reports NDK r21.

| File | SHA-256 | Observed role |
| --- | --- | --- |
| `libgdk.so` | `39d8fa5a5cb207e10449a37314e8e2147197409cb583a7701e82354b71ec6502` | adapter protocol, CAN/ISO-TP, authentication/session and X-mode control |
| `libdiaglogic.so` | `e86879fa50cef63a0b34d2ce993cfdc6c3c9ea7e5cf8bd7a1f2fe1f406de0478` | higher-level diagnostic workflow/vehicle data logic |
| `libwhisper.so` | `c0f3e0a4cf17113e25a29f6e09efb618c51cc23022b0c92c64aa2983aedab0bc` | configuration-driven diagnostic/device/channel execution |
| `libstatemachine.so` | `2c37df7a9bcc089421fa2f35de888f68cd507b2ad2b1a912f1ed128731e6049b` | generic state-machine support |
| `libcommon.so` | `a3fdc76b42ec22baaa23890336f86cd050da07bf63044a7dd32268d31207a894` | shared AES-256, Base64, SHA-256 and utility layer |
| `libtsivehicleinterface.so` | `3e3de7618928168cc1bbfd8e1351082e9f7dfd230fc575e6e75efab2f9457e2b` | JNI/vehicle-interface bridge into GDK/Whisper |
| `libSAL.so` | `5fe49a5a0b95f6c6b2bd86e4abf3e7a90e8e1d918275d38ba72bd59cc26b703f` | low-level support abstraction |
| `libc++_shared.so` | `a51e2ead03805f4d3a230f8f5802471c090441e7e6100d7a06ac82fdb207f87c` | Android C++ runtime |
| `libjnidispatch.so` | `1c6776ee60c3698c784eed83ccc8910e1b5be6ca85553d5edd97f7177cc45f01` | JNA support |
| `libsqlcipher.so` | `6b14a4def0246a5ee63f650c4efad0bc61cbef1a7f1951da1923a78b5d0800b7` | encrypted database support |
| `libjniPdfium.so` | `eca0f44c753770ba33d091ca7ea34be8183728b1f54a0e39b6f765a9e70c446e` | PDF support |
| `libmodpdfium.so` | `be44e26142d2cd8f8e607c0dac00ea1df1528813a49a4af1f4cad05452428b0e` | PDF support |
| `libmodft2.so` | `44ce4ea58531fe07328b0121ebddbc139dbdc577e5bcede9df7eb689d5687c28` | font/PDF support |
| `libmodpng.so` | `5599b9e871f38e90d9d9a636978e085c7f38ed70cd9eeef36eb7eb4d1672f3ed` | image/PDF support |

## Native dependency graph

ELF `DT_NEEDED` relationships confirm the architecture:

- `libgdk.so` depends on `libstatemachine.so` and `libcommon.so`.
- `libwhisper.so` depends on `libgdk.so` and `libcommon.so`.
- `libdiaglogic.so` depends on `libwhisper.so`, `libgdk.so`, `libstatemachine.so` and `libcommon.so`.
- `libtsivehicleinterface.so` depends on `libwhisper.so`, `libgdk.so` and `libcommon.so`.

This matches the DEX bridge: GDK owns the adapter session, Whisper owns configuration-driven communication, and DiagLogic owns higher-level diagnostic cycles.

## GDK adapter command surface

`libgdk.so` retains descriptive exported C++ symbols for:

- `SlCommandCanOpen` / `SlCommandCanClose`
- `SlCommandRawTx` / `SlCommandRawRx`
- `SlCommandIsoTpConfig`
- `SlCommandIsoTpConfigOBD`
- `SlCommandIsoTpTRx`
- `SlCommandIsoTpStop`
- `SlCommandSetBaudrate`
- `SlCommandGetSeed`
- `SlCommandSetKey`
- `SlCommandGetPasskey`
- generic `GetX` / `SetX` interfaces

`XCommander` exposes adapter VIN get/set, Bluetooth link-key retrieval, delayed sleep, SPP-server deactivation, app-launch mode, bounded-device removal and ignition-off voltage-threshold configuration.

This proves the genuine adapter stack contains raw CAN and ISO-TP capabilities, not merely a closed telemetry API.

The command-class vocabulary and single-character command family are consistent with an SLCAN-derived protocol, but exact command strings should be promoted only from the individual `buildCommandString()` implementations or a physical capture.

## Session-key evidence

`SessionMasterKey` constants in `libgdk.so` prove:

- Session Master Key: 32 bytes
- random-D: 16 bytes
- random-A: 16 bytes

`SessionMasterKey::deriveSessionKey()` uses `Sha256ContextFactory::createContext()`. Thumb disassembly validates both random inputs as 16 bytes, then updates the digest with the stored Session Master Key followed by the first and second random arguments before obtaining the digest and constructing a `SessionKey`.

The later call-flow reconstruction assigns the two arguments:

`SessionKey = SHA-256(SMK || device_random || app_random)`

`libgdk.so` also imports `Aes256Utils::encodeMessage/decodeMessage`, raw `Aes256Utils::encode/decode`, and Base64 encode/decode from `libcommon.so`. `libcommon.so` exports AES-256 ECB primitives plus the higher-level message helpers.

This proves AES-256/Base64 are part of the secure native stack. The active command envelope and its bounds are reconstructed later in this note and covered by regression tests.

## Whisper configuration-driven vehicle protocol

`libwhisper.so` exposes configuration vocabulary for:

- device-provider types `OBD`, `PDU`, `OBD_TRANSCEIVE`, `PDU_TRANSCEIVE`, `STREAMING_OBD_TRANSCEIVE`
- action types `RAW_CAN_TRANSMIT`, `OBD_TRANSMIT`
- channel `baudrate`, `p2star`, `p3`
- data/request/result IDs
- OBD request service ID and parameters
- PDU request bytes, timeout, matching response
- CAN result filtering, offsets, byte/bit masks, timeout and throttle
- response-selection strategies
- result extraction, field length, encoding and presentation
- formulas

Literal resource/configuration names include:

- `config.properties`
- `_configs`
- `deviceproviders`
- `actionproviders`
- `activeconfiguration`
- `vinmapping`

The strongest architectural implication is that the actual Mercedes ECU addresses, services, PDU bytes, extraction rules and formulas are likely data consumed by Whisper, rather than all being hard-coded in the native binaries.

Those APK configuration assets are therefore the next highest-value evidence source.

## Cross-transport consequence

The evidence supports a clean architecture:

1. Mercedes-me-specific Bluetooth/session/security remains in the genuine-adapter provider.
2. Generic CAN/ISO-TP remains in LINK.
3. Mercedes ECU/request/result/formula knowledge belongs in MBLINK's Mercedes knowledge layer.
4. Once a Mercedes request is proven, the same vehicle-side definition can be executed through Mercedes me, Tactrix/OpenPort, STM32 CAN or an ELM transport where supported.

## Secure native message format

A deeper Thumb-disassembly pass over `libcommon.so` and `libgdk.so` now
establishes the secure message format byte-for-byte.

`libcommon.so` exports `crc16_ccitt`, `pmu_encrypt`, `pmu_decrypt`,
AES-256 ECB primitives and Base64 helpers. The CRC implementation is the
CRC-16/CCITT-XMODEM parameter set: polynomial `0x1021`, initial value
`0x0000`, non-reflected processing and no observed final XOR. The standard
`123456789` check value is therefore `0x31C3`.

Before AES encryption, the plaintext is represented as:

```text
+0  uint16  plaintext length, big endian
+2  uint16  CRC-16/CCITT-XMODEM over plaintext, big endian
+4  uint16  reserved, zero
+6  bytes   plaintext
...         zero padding
```

The padded inner-frame length is:

`(plaintext_length + 22) & ~15`

which is the next 16-byte multiple strictly above the six-byte header plus
payload. Ciphertext is capped at 512 bytes; consequently the largest plaintext
that can be represented by the observed implementation is **505 bytes**.

The frame is encrypted block-by-block with **AES-256 ECB** using the 32-byte
session key. `AesCommandProcessor::onTransmit()` then Base64-encodes the
ciphertext and wraps it as:

```text
a<Base64(ciphertext)>\r
```

The receive path requires the `a` identifier for encrypted records, strips
the wrapper, Base64-decodes, AES-256-decrypts, validates the embedded length
and CRC, and reconstructs the plaintext command. The two reserved bytes are
observed as zero on transmit; no stronger receive-side semantic meaning is
claimed.

LINK independently implements this bounded envelope in
`mercedes_me_native_protocol.c`. It does not use or redistribute the
proprietary Android binary.

## Session-key derivation

The native constants are now tied together:

- Session Master Key: **32 bytes**
- device/adapter random: **16 bytes**
- application random: **16 bytes**
- SHA-256 digest / SessionKey: **32 bytes**

`SessionMasterKey::deriveSessionKey()` updates one SHA-256 context with the
stored SMK and then the two random arguments in call order. The independent
interoperability formula is therefore:

```text
SessionKey = SHA-256(SMK || device_random || app_random)
```

The secure path sends the application random with GetSeed, receives the
device random, authenticates that value with SetKey, and then derives the
session key in the order above. The legacy path omits the application-random
payload and performs the device-seed challenge/SetKey exchange only. The
supporting call-flow evidence is recorded in `MERCEDES-ME-AUTH-FORENSICS.md`.

## Proved GDK command vocabulary and builders

Native command classes and their concrete builders establish these identifiers:

| Purpose | Identifier / observed builder |
| --- | --- |
| CAN close | `C` |
| CAN open | `O` |
| baud-rate ordinal | `S` |
| CAN filter code | `M` |
| CAN filter mask | `m` |
| timestamp control | `Z` |
| echo control | `E` |
| raw CAN family | `t` |
| ISO-TP configuration | `I` |
| ISO-TP transaction | `i` |
| GetSeed | `y` |
| SetKey | `Y` |
| GetPasskey | `p` |
| adapter status | `V` |
| hardware information | `N` |
| X-mode family | `X` |
| secure wrapper | `a` |

The following complete builders are now proved:

- CAN open: `O\r`
- CAN close: `C\r`
- adapter status: `V\r`
- hardware information: `N\r`
- GetPasskey: `p\r`
- GetSeed with no payload: `y\r`
- GetSeed with payload: `y<Base64(payload)>\r`
- SetKey: `Y<Base64(payload)>\r`
- baud ordinal: `S0\r` through `S8\r`
- X read: `X%02X\r`
- X write: `X%02X<payload>\r`
- raw CAN: `t<can-id:3HEX><payload-length:1HEX><payload:HEX>\r`
- ISO-TP configure: `I01<request-id:4HEX><response-id:4HEX><flags:2HEX><padding:2HEX>\r`
- ISO-TP transact: `i01<request-id:4HEX><Base64(payload)>\r`

The raw-CAN payload-length field is one uppercase hexadecimal digit and the
payload is emitted as uppercase hexadecimal bytes. The native bounds are
**0..8 bytes** and CAN IDs **0..0x7FF**. For example, CAN ID `0x7E0`
with payload `22 F1 90` constructs `t7E0322F190\r`.

The ISO-TP command version is exactly **`01`**. Request/response CAN IDs
are emitted as four uppercase hexadecimal digits. ISO-TP transmit payloads are
**0..100 bytes** and are Base64-encoded; `0x7E0` plus `22 F1 90`
constructs `i0107E0IvGQ\r`.

For ISO-TP configuration, bit 0 of the flags byte denotes padding enabled.
Bit 7 is set when raw-CAN responses are **not** allowed. Padding accepts
**-1..255**; `-1` disables padding, clears bit 0 and places the native
sentinel byte `AA` in the wire field. Thus request `0x7E0`, response
`0x7E8`, raw responses allowed and padding `00` constructs
`I0107E007E80100\r`; with raw responses disallowed and padding disabled it
constructs `I0107E007E880AA\r`.

The official initialization path uses baud ordinal **6**, which constructs
`S6\r`. The native binary does not by that fact alone prove the physical
bit-rate meaning of ordinal 6, so LINK intentionally does not relabel it using
generic SLCAN convention.

The CAN-throttle extension also exposes `F10` run state, `F11` message
limit, `F12` filter and `F13` reset, with corresponding `F10-E-` through
`F13-E-` error prefixes.

These builders are exposed as **pure bounded functions only**. LINK does not
automatically transmit them to a real adapter.

## X-mode identifiers

`CommandString::COMMAND_ID_X(mode)` formats an uppercase two-digit hex mode
after `X`. The observed valid mode range is `0x01..0x29`. Named modes
currently proved are:

| Mode | Native meaning |
| ---: | --- |
| `0x01` | Bluetooth MAC reset |
| `0x10` | set/get data |
| `0x19` | get Bluetooth link key |
| `0x20` | enter sleep |
| `0x22` | set adapter sleep period |
| `0x23` | set CAN repeat count |
| `0x24` | set CAN repeat wait time |
| `0x25` | set ignition-off voltage threshold |
| `0x28` | set app-launch mode |
| `0x29` | set adapter SPP mode |

The observed maximum value for the enter-sleep setting is 99. VIN-oriented
XCommander interfaces accept one through 17 characters for adapter VIN.

## GDK protocol bounds and watchdog policy

Exact native constants now preserved in portable LINK include:

- synchronous command timeout: **6900 ms**
- standard CAN-ID range: **0..0x7FF**
- block-all CAN filter value: **0x7FF**
- maximum CAN IDs in one filter operation: **15**
- raw CAN TX payload maximum: **8 bytes**
- ISO-TP TX payload maximum: **100 bytes**
- default raw-RX timeout: **400 ms**, observed range **200..10000 ms**
- ISO-TP P2* maximum: **10000 ms**
- ISO-TP P3 maximum: **5000 ms**
- legislated OBD request ID: **0x7DF**
- QoS loop queue size: **10**
- QoS loop age: **3000 ms**
- QoS-state maximum age: **90000 ms**
- dead-man default adapter-stop delay: **30 s**
- dead-man startup timeout: **3000 ms**
- dead-man startup attempts: **3**
- `ObdAdapterInit::USE_NO_RESPONSE_MODE = true`

These constants describe the archived implementation. They are not permission
to change adapter state or vehicle state.

## DiagLogic acquisition and sanity policy

`libdiaglogic.so` contains exact acquisition defaults that are independent of
the Bluetooth transport. LINK exposes them as a read-only reference-policy
structure so every transport can use the same evidence without duplicating
magic numbers.

| Native policy | Value |
| --- | ---: |
| live-data stream read timeout | 1500 ms |
| live-data availability timeout | 5000 ms |
| minimum ignition read delay | 10000 ms |
| maximum ignition-read speed | 10 |
| minimum mileage read delay | 30000 ms |
| maximum mileage read delay | 300000 ms |
| maximum mileage-read speed | 20 |
| minimum fuel-read distance | 5 |
| minimum negative mileage difference | -2 |
| codes-cleared/measured-mileage time difference | 30000 ms |
| maximum speed age for ignition | 10000 ms |
| ignition-off threshold minimum | 12.2 V |
| ignition-off threshold default | 13.2 V |
| ignition-off threshold maximum | 13.2 V |
| margin below observed max battery voltage | 0.2 V |
| allowed live-status age | 60000 ms |
| allowed run-cycle-status age | 90000 ms |
| invalid trip-start mileage sentinel | -1.0 |

Where the native symbol name does not prove a physical unit (notably the
speed/distance thresholds), LINK preserves the numeric value without inventing
one.

The higher-level native pipeline also names
`UnplausibleFuelVolumeFilter`,
`MileageAdjustingVehicleStatusUpdater`,
`TripAverageSpeedVehicleStatusUpdater`,
`IrregularObdResponseVehicleStatusUpdater` and
`TripWarningLampVehicleStatusUpdater`. It separately tracks concepts such as
`actualFuelFlow`, `fuelFlowSinceReset`, `fuelFlowSinceStart`,
`fuelFlowValues`, `fuelVolumeStatistics`, `mileageOfLastFuelRead` and
`fuelRefreshRequired`.

The architectural consequence is important: raw ECU samples and
validated/derived vehicle state are distinct layers and should remain so in
MBLINK regardless of which LINK transport supplied the raw measurement.

## VIN cascade vocabulary

DiagLogic contains separate VIN paths including `vinCascade`,
`miniVinCascade`, `MSAVinCascadeVin`, `modelSpecificVin`, `vinPart3`,
`vinPart3_0x071` and `vinPart3_0x204`. The suffixes are retained as native
evidence labels only; LINK does not assume they are CAN addresses without
call/configuration evidence.

## Expanded Whisper configuration schema

Whisper exposes considerably more than a generic PDU/formula pair. Its
configuration vocabulary includes data points, throttle data points, data
type/unit, read interval, always-available flags, dependencies/children,
device provider, link/request/service identifiers, retry interval, PDU bytes,
timeouts, matching responses, TX/RX addressing, baud rate, P2*/P3 timing,
padding, CAN filter offset/byte mask/bit mask, result extraction, field length,
encoding and formulas.

Observed response-selection strategies are:

- `SELECT_FIRST`
- `SELECT_LOWEST_CANID_CACHED`
- `SELECT_MAXIMUM`
- `MERGE_ELIMINATE_DUPLICATES`

Observed DTC presentation types are:

- `SAEDTC_KWP_DAI`
- `SAEDTC_KWP_VW`
- `SAEDTC_UDS_DAI`
- `SAEDTC_UDS_VW`
- `SAEDTC_OBD`

LINK preserves these exact names in a small portable Whisper vocabulary module.
This is particularly relevant to multi-ECU OBD responses: the official stack
has explicit select/merge policies rather than assuming every response is one
anonymous value.

Whisper also contains Poco ZIP decompression support, a native
`DecompressHandler`, configuration revision/cache/blacklist handling,
`activeconfiguration` and `vinmapping`. The diagnostic definition set may
therefore be supplied as a compressed configuration bundle rather than a
plain directory of obvious `.properties` files.

## Implementation status

LINK now independently implements and regression-tests:

- session-key derivation from caller-supplied SMK/random values;
- CRC-16/CCITT-XMODEM;
- the bounded six-byte secure inner frame;
- AES-256 ECB encryption/decryption for that frame;
- Base64 `a...CR` secure wrapping/unwrapping;
- the proved simple/seed/key/baud/X command builders;
- the exact raw-CAN `t...`, ISO-TP configuration `I...` and ISO-TP
  transaction `i...` builders;
- observed GDK protocol limits and X-mode constants;
- the DiagLogic acquisition-policy constants;
- Whisper response-selection and DTC-presentation vocabularies.

No proprietary shared object, decompiled function body, embedded fixed
challenge key or APK configuration bundle is copied into LINK.

## Still evidence-gated

- acquisition of an adapter-specific SMK from the retired external provider
  or a valid existing cache on a fresh installation;
- the exact commissioning and backend-provisioning lifecycle that associates
  an adapter ID/passkey with that SMK;
- payload formats for X modes whose identifier is known but structure is not;
- the actual compressed/raw APK configuration assets;
- Mercedes ECU TX/RX CAN addresses and service/DID/PDU mappings from those
  assets;
- result extraction/scaling formulas and units where not already explicit;
- physical validation against the A2138203202 adapter.
