# Mercedes me Adapter native-library evidence

This note preserves interoperability facts recovered from the native Android libraries supplied from the archived official Mercedes me Adapter 4.7.61 application. It is intended for the current `Infiltrator-Projects/LINK` repository.

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

Current binary-supported shape:

`SessionKey = SHA-256(SMK || random-argument-1 || random-argument-2)`

`libgdk.so` also imports `Aes256Utils::encodeMessage/decodeMessage`, raw `Aes256Utils::encode/decode`, and Base64 encode/decode from `libcommon.so`. `libcommon.so` exports AES-256 ECB primitives plus the higher-level message helpers.

This proves AES-256/Base64 are part of the secure native stack. The exact active command envelope should remain evidence-gated until the relevant GDK builders/decrypt path are fully reconstructed.

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

## Still evidence-gated

- exact wire syntax for each GDK command builder
- complete active authentication/secure-envelope byte sequence
- exact X-mode identifiers where not yet tied to a specific builder
- the actual APK configuration assets
- Mercedes ECU TX/RX CAN addresses
- service/DID/PDU mappings for the known data identities
- result extraction/scaling formulas and units
- physical validation against the A2138203202 adapter
