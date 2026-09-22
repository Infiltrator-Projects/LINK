# Changelog

## 0.15.51 — 2026-09-22

- Resolve issue #36's tester/server transplant ambiguity by renaming the bxCAN VIN request example and its public example symbols to explicitly say tester/client; the STM32F103 ECU/server remains the separate `link-stm32f103-uds-ecu.c` + `issue-32/Src-main.c` integration.
- Lock issue #37's exact screenshot sequence into regression coverage: `14 FF FF FF` in DefaultSession returns `7F 14 7F`; after the required session/security sequence the clear returns `54`, `19 01 0D` counts zero fault-state DTCs, while `19 01 FF` can still count cleared definitions via the not-completed bits.
- Close the remaining MBLINK #27 qualification gap by cross-compiling the exact retained `examples/stm32c092/issue-27/Src-main.c` under Cortex-M0+ CI with Cube-interface stubs.

## 0.15.50 — 2026-09-22

- Advance the exact nested Infiltratr Common dependency to 1.19.22 / `302c44eb7436803dee020667453a9a0681da8bbf` so every LINK consumer inherits the current bounded-text and durable-POSIX fixes from one tested dependency chain.
- Keep LINK as the sole Common authority for MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK; product repositories continue to pin LINK rather than selecting an independent Common revision.

## 0.15.49 — 2026-09-22

- Add a fail-closed UDS OTA/bootloader orchestration core for MBLINK issue #56, mapping the full programming-session -> SecurityAccess -> DTC/communication quiesce -> RequestDownload -> TransferData -> TransferExit -> CheckMemory -> staged reset flow.
- Add A/B inactive-slot selection, strict transfer sequence/size accounting, monotonic anti-rollback, separate integrity/authenticity verification gates, secure-boot candidate validation and post-boot monotonic-version commit.
- Keep programming disabled by default and require a complete target backend before arming; LINK contains no STM32 flash addresses, option-byte manipulation or OEM/HSM secrets.
- Add regression coverage for the complete successful sequence, rollback rejection, block-sequence failure, incomplete transfer rejection and authenticity-failure abort.

## 0.15.48 — 2026-09-22

- Ship an exact machine-applicable repair for LINK issue #31 against `leoembeddeder/STM32F103TEST` head `5055d58f4f6240488271d85cb177f71337e0cdeb`, covering its remaining 0x19 user-memory, emissions, fault-counter, WWH-OBD and 0x55 defects.
- Record that external repository metadata advertises push permission while actual Git and Contents writes are rejected with HTTP 403, so LINK no longer conflates a prepared external repair with a merged/physically retested one.

## 0.15.47 — 2026-09-22

- Resolve LINK issue #35 by reducing the default STM32 bxCAN integration to the one interrupt LINK actually requires: RX FIFO0 message-pending.
- Preserve real CAN transmit semantics without TX/error callbacks: LINK polls bxCAN's latched RQCP/TXOK/ALST/TERR result bits, so mailbox admission is never mistaken for wire completion and arbitration/transmit errors remain failures.
- Remove redundant abort/error forwarding state and callbacks. Projects that deliberately enable the CAN TX IRQ may still forward the three successful mailbox-complete callbacks to preserve the exact ISR completion tick; a released mailbox without successful completion evidence remains a hard failure.
- Keep the stronger LINK architecture unchanged: exact diagnostic-ID hardware filters, bounded interrupt-to-main-loop RX queuing and ISO-TP/UDS processing outside the ISR remain mandatory.
- Update the STM32F103 ECU reference and end-to-end VIN regression to use the RX-only default path, proving multi-frame ISO-TP works without TX or error callback plumbing.

## 0.15.46 — 2026-09-22

- Fix LINK issue #34's STM32F103 integration failure when a Cube project enables only the bxCAN RX0 NVIC line: the bxCAN adapter now polls latched RQCP/TXOK/ALST/TERR completion state when no TX callback has run.
- Preserve strict failure semantics: arbitration loss and transmit-error completions remain failures; a released mailbox without either callback evidence or latched hardware result remains a failure.
- Add a regression that reproduces the reporter's RX-only interrupt configuration and proves successful polled completion plus failed-error completion.
- Document TX callbacks as the preferred precise path while supporting the safe RX-only polling configuration used by simpler STM32F103 ports.

## 0.15.45 — 2026-09-22

- Correct LINK issue #24's STM32C092 server example so the bare target advertises only hardReset and softReset; keyOffOnReset and rapid-power-shutdown operations are no longer able to fall through to an MCU reset.
- Add an end-to-end fake-FDCAN regression proving 0x11/0x02 returns 7F 11 12 with no deferred reset while 0x11/0x01 still returns 51 01 and exposes the reset only after CAN transmit completion plus the response-drain interval.

## 0.15.44 — 2026-09-21

- Complete the generic portion of MBLINK issue #63 with a typed ISO 14229-1:2020 Authentication (0x29) codec covering all nine authentication tasks and strict positive-response envelope validation.
- Complete the generic variable-record gap from MBLINK issue #59 with definition-driven snapshot/stored-data DID segmentation plus typed extended-data views; unknown DID lengths return unsupported rather than being guessed.
- Preserve the security boundary: Authentication remains SECURITY-classified and Discover deny-by-default, with certificate, trust, key and OEM policy owned by the caller.

## 0.15.43 — 2026-09-21

- Complete LINK issue #32 with a bounded STM32F103 ECU/server reference covering all 27 standard LINK UDS service IDs.
- Add persistent 0x14/0x19 diagnostic state using an alternating two-page CRC-verified flash journal for the requested 512 KiB / 2 KiB-page F103 layout.
- Add a security/session-gated programming sandbox, AES-CMAC SecurityAccess reference flow, writable/dynamic DIDs, transfer sequencing and reboot-persistence regression coverage without pretending to provide a production bootloader or OEM key algorithm.
- Add a Cube/HAL integration for CAN1 PA11/PA12 at 500 kbit/s and extend Cortex-M3/M7 cross-compilation to the shared UDS server, AES-CMAC and F103 ECU core.
- Record the exact LINK #31 forensic repair map against external STM32F103TEST commit 5055d58, including the still-present 0x17 family error, missing 0x18/0x19 MemorySelection echoes, unsupported 0x12-0x15 reports and incomplete 0x42 envelope.

## 0.15.42 — 2026-09-21

- Complete LINK issue #26 with a reusable allocation-free AES-128 / RFC 4493 AES-CMAC primitive and deterministic published test vectors.
- Add an algorithm-neutral UDS 0x27 SecurityAccess server hook: target callbacks own seed generation and key verification while LINK owns requestSeed/sendKey sequencing and active security-level state.
- Add generic invalid-key attempt/delay handling with NRC 0x35, 0x36 and 0x37, preserving lockout state across ordinary diagnostic-session changes.
- Keep OEM security policy out of LINK: CMAC availability never implies that Mercedes or any other ECU uses CMAC.

## 0.15.41 — 2026-09-21

- Complete LINK issue #25 with product-neutral per-service and per-subfunction UDS server execution policy metadata.
- Add explicit Default/Programming/Extended/Safety session masks, 64 security levels, and physical/functional/both addressing policy.
- Enforce policy before application handlers with context-appropriate NRCs while preserving the existing unrestricted API when no policy table is configured.
- Pass real physical/functional request context from the STM32 UDS server into the shared dispatcher and regression-test policy precedence, validation and security-state transitions.

## 0.15.40 — 2026-09-21

- Repair the Apple portable-core amalgamation exposed by the suite-wide About rollout: include the shared selection engine now required by LINK::Core.
- Keep the Apple bridge validator and actual compiled source topology in exact agreement so product faces can consume the current LINK revision without copying core source lists.
- Preserve the 0.15.39 suite-standard About contract unchanged.


## 0.15.39 — 2026-09-21

- Complete the suite-wide About contract by supplying the canonical Build field to LINK's standard SwiftUI product face.
- Repair the Win32 Discover About metadata initializer after the shared About model gained a Build field; use designated initializers so future model growth cannot silently shift identity fields.
- Give native Windows Discover surfaces an explicit build identity while preserving the shared Website, Credits, Licence and Close structure.


This file records user-visible, compatibility, diagnostic-knowledge and validation changes for LINK.

## 0.15.38 — 2026-09-21

- Added request-aware ReadDTCInformation response validation so transaction code verifies request-specific echoes instead of accepting a merely parseable byte layout.
- Validated MemorySelection for 0x17..0x19, record-number echoes for 0x05/0x16, functional-group echoes for 0x42/0x55, and requested-DTC echoes for the applicable snapshot, extended-data and severity reports.
- Added regression vectors for malformed user-memory responses that omit MemorySelection, matching the failure class found during forensic comparison with the externally supplied STM32 server.
- Kept transmission policy unchanged: 0x19 remains read-only; ClearDiagnosticInformation 0x14 remains state-changing; Authentication 0x29 remains security-gated.

## 0.15.37 — 2026-09-21

- Standardised LINK-owned About presentation on the System Monitor contract across Linux, Windows and Apple surfaces.
- Removed forced Linux About sizing, private About CSS and product-tagline hierarchy so GTK owns the native dialog geometry.
- Added a first-class build label to the shared About model and aligned the main hierarchy to icon, product, version, description, build, website and copyright.
- Kept Credits and Licence as dedicated actions on every supported presentation surface; Windows now exposes native Credits and Licence buttons instead of flattening those details into the main text.
- Standardised the visible website label to `Website` and Australian `Licence` wording in LINK-owned custom surfaces.

## 0.15.36 — 2026-09-21

- Forensically revalidated the complete requested ReadDTCInformation (0x19) surface across all 27 report types and added an explicit implementation conformance matrix.
- Corrected response validation for 0x19/0x04, 0x06, 0x10, 0x18 and 0x19 so truncated positive responses can no longer be accepted without their mandatory DTC-and-status envelope; user-defined-memory responses also require the memory-selection echo.
- Added allocation-free typed record views for fixed DTC/status, snapshot-identification, severity, fault-detection-counter and WWH severity records while retaining implementation-defined snapshot/extended-data tails as raw bounded spans.
- Expanded ReadDTCInformation regression coverage to every requested report family, including malformed fixed-envelope cases.
- Locked the service-effect contract in tests: 0x19 remains read-only, 0x14 ClearDiagnosticInformation remains state-changing, and 0x29 Authentication remains security-gated.
- Expanded ClearDiagnosticInformation codec tests for optional memory selection and 24-bit group bounds. No Discover permission was broadened.

## 0.15.35 — 2026-09-21

- Completed a second forensic Common 1.19.20 forward-consumption pass without changing Common.
- Replaced remaining allocation-heavy GLib ASCII case-folding in the Linux Bluetooth provider with Common's deterministic ASCII comparison, substring and hexadecimal-classification contracts.
- Replaced the Windows About dialog's remaining direct LoadLibrary/GetProcAddress plumbing with Common's UTF-8-aware dynamic-library boundary.
- Reused Common's ASCII whitespace/case conversion in simulator/session normalization, Common's finite clamp in dashboard gauge normalization and Common's saturating counter increment in telemetry sequence handling.
- Deliberately retained protocol-specific narrow whitespace grammars, streaming JSON writers and GLib-native timing where the existing implementation is semantically narrower or more efficient than the available Common primitive.

## 0.15.34 — 2026-09-21

- Completed the Common 1.19.20 forward-consumption pass in the Linux GTK shell.
- Replaced the shell's GLib monotonic-clock wrapper with Common's canonical monotonic nanosecond provider.
- Moved LINK language-preference XDG path resolution, recursive directory creation and atomic persistence onto Common's POSIX contracts while retaining LINK's own language policy and INI schema.
- Replaced the language-pack parser's locale-sensitive ctype whitespace/case conversion with Common's deterministic ASCII contracts.
- Kept language-pack discovery and GTK-native UI concerns in the Linux presentation layer; no code or policy was added to Common.

## 0.15.33 — 2026-09-21

- Advanced LINK from Infiltratr Common 1.19.10 to 1.19.20 and pinned the exact released Common commit.
- Removed LINK-local ASCII case-folding and whitespace helpers now owned by Common, keeping protocol and adapter matching locale-independent.
- Replaced the Windows Discover J2534 loader's direct LoadLibrary/GetProcAddress plumbing with Common's UTF-8-aware dynamic-library boundary and atomic symbol binding.
- Reused Common's monotonic-clock adapter and saturating deadline arithmetic in the native Linux OpenPort provider.
- Linked platform UI providers to Common's full platform target only where those platform services are actually required; the portable LINK core remains on Common::Portable.

## 0.15.32 — 2026-09-20

- Published the repository-wide copyright normalization already present on main.
- No diagnostic, transport, safety, Vehicle Research, API or presentation behaviour changed from 0.15.31.

## 0.15.31 — 2026-09-20

- Added LINK's portable Vehicle Research session state with explicit passive-capture, standards-inventory, manufacturer-sweep, paused and complete phases.
- Extended the shared evidence stream with typed research-session, research-phase, event-marker and research-summary records while preserving every raw frame.
- Upgraded the shared Windows Discover face into the first Vehicle Research workspace slice: passive capture, bounded OBD inventory, product sweep, operator event markers and research export now share one deterministic session timeline.
- Kept the existing deny-by-default transmission policy unchanged; research additions do not enable reset, security, routine, clear, coding or programming operations.

## 0.15.30 — 2026-09-20

- Advanced the exact nested Infiltratr Common dependency to 1.19.10.
- Preserved LINK's protocol, transport and presentation contracts while exposing Common 1.19.10's expanded product-neutral Night/Day design palette transitively to LINK-based product faces.
- Kept automotive semantics in LINK and generic design ownership in Common; no duplicate theme wrapper or second palette source was introduced.

## 0.15.29 — 2026-09-19

- Completed a forensic Common 1.19.8 reuse pass across portable codecs and platform providers, replacing parallel endian, checked-size, allocation-growth, strict parsing, bounded-copy and array-sizing mechanics with the canonical Common APIs while retaining the existing protocol and transport contracts.
- Centralised Apple standard OBD-II value text rendering through LINK's shared preference-aware formatter so Swift, GTK and C presentation paths use the same deterministic conversion and precision policy.
- Corrected the standalone native-Linux provider smoke target to link Common explicitly now that the provider edge consumes Common primitives directly.

## 0.15.28 — 2026-09-19

- Advanced the exact Infiltratr Common dependency to 1.19.8, retaining LINK's shared formatting APIs while inheriting Common's consolidated parsing, ASCII, checked-size and POSIX numeric mechanics.

## 0.15.27 — 2026-09-19

- Centralised fuel-economy/trip value rendering in LINK so product faces share unit conversion, precision and unavailable/stationary semantics.
- Added regression coverage for metric and US-customary fuel-economy presentation.

## 0.15.26 — 2026-09-19

- Upgraded the exact nested Infiltratr Common dependency to 1.19.7.
- Reused Common's deterministic fixed-point and CSV-field encoders in shared telemetry output.
- Added shared preference-aware OBD-II value formatting so product faces no longer duplicate unit/precision policy.
- Added a bounded decoded-PID summary formatter for product-neutral standard-data presentation.
- Added regression coverage for shared display formatting and CSV control/quote handling.

## 0.15.25 — 2026-09-19

- Upgraded the exact nested Infiltratr Common dependency to 1.19.3.
- Added a protocol-neutral, vehicle-scoped parameter selection model so product faces can keep explicit choices isolated by vehicle identity.
- Generalised bounded session traces from OBD-only PID identity to LinkParameterKey while retaining the standard OBD compatibility API.
- Expanded the graph capacity to the complete 256-entry standard PID space and added safe runtime reconfiguration of the selected graph set.
- Centralised ELM327 CAN header and receive-address command formatting for reuse by manufacturer products.
- Added regression coverage for vehicle isolation, selections beyond eight channels, manufacturer-capable trace identity, and shared CAN address formatting.
- Canonical documentation baseline aligned with the Infiltrator project family.

## Policy

Record new supported diagnostic behaviour, changed safety/permission boundaries, corrected definitions, platform changes, dependency revisions that alter behaviour and material fixes. Raw research activity belongs in the relevant evidence document until it changes supported product behaviour.

## Historical identity

Git tags and GitHub Releases remain authoritative for exact historical source and dependency identity.
