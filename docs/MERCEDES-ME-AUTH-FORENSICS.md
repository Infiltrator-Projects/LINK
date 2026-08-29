<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Mercedes me Adapter authentication forensics

This note records additional interoperability facts recovered from the archived official Mercedes me Adapter 4.7.61 native stack. It supplements `MERCEDES-ME-NATIVE-BINARIES.md` without changing the evidence boundary: only behaviour supported by the supplied binaries is stated as proved, and no backend credential, cached master key, or proprietary binary is redistributed here.

## Evidence boundary

The relevant native classes in `libgdk.so` are:

- `SeedKeyAction`
- `ConfigureSecureModeAction`
- `RetrieveSmkAction`
- `SessionMasterKey`
- `SessionMasterKeyCache`
- `ISessionMasterKeyProvider`
- `JniSessionMasterKeyProvider`
- `AesCommandProcessor`

The reconstruction below comes from their retained symbols, relocation/import relationships, constant sizes and Thumb call flow. The portable primitives already implemented in LINK remain clean-room interoperability code.

## Two distinct login paths

The archived stack has a legacy seed/key login and a secure-session login. They share the adapter challenge transform but are not the same operation.

### Legacy `SeedKeyAction`

The proved sequence is:

1. Execute GetSeed without an application-random payload (`y\r`).
2. Receive and Base64-decode the adapter/device seed.
3. Require the decoded seed to be exactly 16 bytes.
4. Transform that 16-byte device seed with the fixed 32-byte adapter-authentication AES-256 key embedded in the archived GDK.
5. Send the resulting 16-byte authentication response with SetKey (`Y<Base64(response)>\r`).

This path does not construct an `AesCommandProcessor` and does not derive the encrypted-session key described below.

LINK already implements and regression-tests the legacy seed builder, seed parser, challenge response and SetKey builder. The fixed adapter-authentication key remains an interoperability implementation detail in the clean-room source rather than documentation material.

## Secure `ConfigureSecureModeAction`

The secure path proves the previously unresolved direction and ordering of the two random values.

1. Obtain the 32-byte Session Master Key (SMK) from state/context.
2. Generate a 16-byte **application random** value.
3. Execute GetSeed with the application random as its payload:

   `y<Base64(app_random)>\r`

4. Receive and decode the 16-byte **device random** returned by the adapter.
5. Authenticate the device random using the same fixed AES-256 challenge transform as the legacy login.
6. Send that authentication response through SetKey.
7. Derive the live 32-byte session key in this exact order:

   `SessionKey = SHA-256(SMK || device_random || app_random)`

8. Construct `AesCommandProcessor(SessionKey)` and apply it to the adapter command path.
9. Subsequent secure records use the independently reconstructed `a<Base64(AES-256-ECB(inner-frame))>\r` envelope documented in `MERCEDES-ME-NATIVE-BINARIES.md`.

This resolves the earlier A/D ambiguity: the first random supplied to `SessionMasterKey::deriveSessionKey()` is the device/adapter random, and the second is the application random.

The retired Android implementation generates its application random through a `System::getCurrentTimeMillis()` / `srand()` / `rand()` path. LINK intentionally does **not** reproduce that weak generator; callers must supply cryptographically strong random bytes.

## Where the Session Master Key comes from

`RetrieveSmkAction` proves that GetPasskey does **not** directly return the SMK.

The native sequence is:

1. Read the current adapter ID (`KEY_OBD_ADAPTER_ID`).
2. Obtain the `SessionMasterKeyCache` and look up an SMK by that adapter ID.
3. On a cache hit, reuse the cached SMK.
4. On a cache miss, execute GetPasskey (`p\r`) against the adapter.
5. Pass **adapter ID plus returned passkey** to the configured `ISessionMasterKeyProvider`.
6. The provider returns textual key material.
7. Reject an empty provider response and the observed `#`-prefixed error/sentinel form.
8. Base64-decode the successful provider response.
9. Construct a `SessionMasterKey`, which requires the decoded key material to satisfy the native 32-byte SMK contract.
10. Store that SMK in the per-adapter cache.

The JNI implementation exposes the provider as `JniSessionMasterKeyProvider::loadSessionMasterKey(...)`; the native callback surface accepts two string arguments corresponding to adapter ID and passkey.

### Practical consequence

A fresh secure session therefore requires one of two things:

- a valid SMK returned by the original/compatible Session Master Key provider for that adapter ID and passkey, or
- a valid previously cached SMK for that adapter.

Static reverse engineering of these shared objects alone cannot manufacture an adapter-specific SMK that was supplied externally. This is now the principal evidence boundary for reproducing a complete fresh secure login.

## Session Master Key cache

The native cache is per adapter ID and has explicit read/write/remove handling plus named cache-directory and filename constants. Its key-protection path first asks `System::getEncryptionPassword()` for encryption material and contains a fallback path when that value is absent. The cache also hashes/encodes password-derived material before use.

This is enough to establish that an archived application-data directory may contain valuable interoperability evidence. It is **not** enough to claim that an SMK can be recovered without the corresponding application/cache data and protection context. LINK does not embed or document the native cache fallback secret.

If an archived Mercedes me Adapter application-data/cache set becomes available, it should be treated as a separate forensic evidence source and validated against the adapter ID before any recovered SMK is considered usable.

## Diagnostic concept vocabulary recovered from DiagLogic

`libdiaglogic.so` exposes a substantially richer set of vehicle concepts than the current vehicle-specific DID mappings prove. These names describe data the official application expected to acquire, validate, derive or present; they do **not** by themselves prove a CAN ID, ECU address, service, DID, byte layout or scaling formula.

High-value source-corroborated concepts include:

- vehicle speed, engine RPM and calculated engine load;
- throttle position and **relative accelerator pedal position** as distinct concepts;
- engine coolant temperature, engine oil temperature and intake-air temperature;
- intake-manifold pressure, absolute barometric pressure and boost pressure;
- actual engine torque and engine reference torque;
- fuel pressure, fuel volume, engine fuel rate, actual fuel flow, fuel-flow-since-start, fuel-flow-since-reset, fuel-level minimum and tank range;
- AdBlue remaining-distance concepts;
- particle-filter data;
- battery voltage, maximum battery voltage and ignition-off threshold;
- measured mileage, mileage history and distance since codes were cleared;
- maintenance remaining distance/time and raw maintenance values;
- four individual tyre-pressure positions and combined tyre-pressure values;
- stored OBD DTCs and irregular OBD response state;
- model-specific VIN plus multiple VIN-cascade concepts;
- extensive exterior-lamp, brake-fluid, brake-lining, coolant-level and warning-state concepts.

Trip/derived logic also names trip average speed, trip-start time/mileage, fuel-volume statistics, acceleration/braking event counts and scoring concepts.

### What this means for MBLINK

These names are a discovery roadmap, not decoded telemetry. They justify looking for the corresponding definitions in the Mercedes configuration corpus and prioritising matching vehicle evidence, especially for fuel flow/range, AdBlue, oil temperature, particle filter, maintenance data, tyre pressures and the accelerator-pedal/throttle distinction.

They must remain `source-corroborated` until a configuration definition and/or vehicle capture proves the actual request and decoder.

## Whisper remains the mapping key

`libwhisper.so` confirms that the official stack is configuration-driven. Its schema covers device providers, requests/services, PDU bytes, TX/RX addressing, baud rate, P2*/P3 timing, padding, CAN filtering, offsets, byte/bit masks, field lengths, encodings, formulas, dependencies and response-selection strategies.

It also contains configuration revision/cache/blacklist handling, VIN mapping and ZIP decompression support. Therefore the missing ECU/request/result/formula definitions are likely to reside in external or compressed application configuration assets rather than being recoverable solely from native code symbols.

The next highest-value evidence sources are consequently:

1. archived Mercedes me Adapter APK configuration/assets and any downloaded configuration bundle;
2. archived application data containing the configuration cache/VIN mapping;
3. the Session Master Key cache for a known adapter, if available;
4. physical adapter captures to validate the reconstructed local command sequence.

## Current interoperability status

From the native binaries alone, LINK can now describe or independently implement the local portions of the authentication stack with much higher confidence:

- legacy GetSeed -> AES challenge response -> SetKey;
- secure GetSeed carrying a 16-byte application random;
- 16-byte device-random response parsing;
- identical device-random authentication step;
- exact session-key derivation `SHA-256(SMK || device_random || app_random)`;
- activation of the AES secure command processor;
- secure record framing, CRC, AES-256 ECB and Base64 wrapper.

What remains external/evidence-gated is the acquisition of an adapter-specific SMK on a fresh installation and the configuration corpus containing the actual Mercedes ECU/data-point mappings.
