# Changelog

## 0.15.65 - 2026-09-24

- Align the shared Linux vehicle titlebar with the 44 px publisher desktop chrome height.
- Preserve manufacturer accents, diagnostics, transport behaviour, dependencies and platform architecture unchanged.

## 0.15.64 - 2026-09-24

- Align shared Linux vehicle navigation rows with the suite-wide 6 px compact corner radius.
- Preserve manufacturer accents, diagnostics, transport behaviour, dependencies and platform architecture unchanged.

## 0.15.63 — 2026-09-24

- Add the publisher-wide 3 px selected-navigation edge to the shared Linux GTK shell while preserving LINK's automotive palette and layout.
- Keep diagnostic, transport, safety, protocol and dependency behaviour unchanged.


## 0.15.62 — 2026-09-23

- Harden LINK #47's STM32 UDS server receive path against a delayed or missing Cube RX callback by opportunistically draining the controller from `link_stm32_uds_server_poll()` as well as from the normal interrupt path.
- Preserve the interrupt as the low-latency producer while relying on the existing re-entrancy guard so IRQ and main-loop drains cannot enter the HAL receive callback concurrently.
- Add an end-to-end regression reproducing the reporter's permanent-silence shape: a valid 0x7E0 DiagnosticSessionControl request is present in the controller-facing RX source, no RX callback fires, and LINK must still transmit the 0x7E8 response.
- Retain the earlier STM32F103 single-bank-flash startup fix: flash-backed ECU recovery still completes before bxCAN is started in the canonical issue-32 integration.
- Reconcile changelog ordering so the 0.15.61 Common 1.19.24 release is recorded in chronological position instead of appearing in a duplicated lower section.

## 0.15.61 — 2026-09-23

- Advance the exact nested Infiltratr Common dependency from 1.19.23 to 1.19.24 at `748e089ae175329471d4cf375522c44081371bd5`.
- Inherit Common's API-compatible graphics hardening for clipped signed coordinates, alias-safe in-place surface operations and overflow-safe nearest-neighbour scaling without changing LINK's diagnostic or transport contracts.
- Keep LINK as the sole Common authority for manufacturer products; downstream products continue to pin an exact released LINK revision rather than selecting Common independently.

## 0.15.60 — 2026-09-22

- Complete LINK #45's STM32F103 Diagnostic-workbook Freeze Frame Snapshot Record 0x01 with the exact six Sheet 7 identifiers: DF00 supply voltage, DF01 vehicle speed, DF02 occurrence counter, DF03 first-malfunction odometer, DF04 last-malfunction odometer and DD00 malfunction timestamp.
- Encode the six workbook DIDs as a 31-byte snapshot payload and prove the exact 39-byte 0x19/0x04 positive response, 0x19/0x03 identification and request-out-of-range behavior when a target reports no captured snapshot.
- Keep real freeze-frame capture and persistence target-owned through a bounded callback instead of fabricating vehicle history in LINK; the reference fallback remains deterministic bench data only.
- Preserve LINK #47's STM32F103 startup/flash fix by keeping the persistent journal within one 2 KiB page and storing the expanded 31-byte snapshot representation outside the flash journal; add a regression that guards that footprint.
- Retain 0.15.59's generic multi-snapshot-record server model and qualify the exact workbook integration across strict Linux/Windows/macOS, ASan/UBSan, native Linux providers and STM32 Cortex-M cross-compile before release.

## 0.15.59 — 2026-09-22

- Generalise LINK's DTC server model so a single DTC can expose multiple snapshot records instead of being limited to one snapshot record number/payload.
- Keep the existing singular snapshot fields as a source-compatible fallback while adding the allocation-free `LinkUdsServerDtcSnapshotRecord` view for real multi-record applications.
- Make ReadDTCInformation 0x19/0x03 enumerate every configured snapshot record and make 0x19/0x04 return one requested record or all records for record selector 0xFF.
- Add strict regression coverage for three records on one DTC, including record identification, exact record selection, all-record response and unsupported-record rejection.
- Repair all strict positional initialisers exposed by the ABI extension and qualify the resulting head across the complete LINK CI matrix before release.

## 0.15.58 — 2026-09-22

- Complete LINK #43's STM32F103 two-level SecurityAccess reference with the requested deterministic 4-byte level-1 seed/key path and a 16-byte AES-CMAC-128 level-2 path backed by target-supplied key material.
- Complete LINK #44's seven workbook DIDs (F181, F182, F183, F184, F185, F186 and F18A), including a live ActiveDiagnosticSession value and exact regression vectors.
- Complete LINK #46's routines 0x0202 CheckMemory, 0xFF00 EraseMemory and 0xFF01 CheckProgrammingDependencies. Qualification exposed and fixed a stale-RAM-CRC defect by validating the active persisted journal generation instead.
- Harden LINK #47's STM32F103 Cube startup ordering so flash journal recovery/schema migration completes before bxCAN is started, avoiding a single-bank flash stall overlapping an already-live CAN receive/response path.
- Carry forward the 0.15.56/0.15.57 multi-page persistence, corrected 0x14/0x19 policies, status availability mask and exact 66-DTC workbook catalogue.
- Full LINK CI is green on the qualified pre-release head across STM32 Cortex-M cross-compile, strict Linux/Windows/macOS, ASan/UBSan and native Linux adapter-provider coverage.

## 0.15.57 — 2026-09-22

- Resolve LINK #42 by replacing the three synthetic STM32F103 DTC placeholders with the exact 66-entry catalogue supplied in the reporter's Diagnostic workbook, cross-checked against an independent implementation of the same sheet.
- Preserve the workbook-derived DTC identities from U300614/U300615 through C100616..C100679, with the corresponding severity/functional-unit metadata and 0x33/0xD0 functional-group mapping rather than inventing additional definitions.
- Raise the allocation-free DTC decode record capacity from 64 to 128 so LINK can consume its own 66-entry supported-DTC response without truncation.
- Bump the STM32F103 persistent-state schema because the DTC arrays now contain 66 entries; old three-entry journal generations are rejected cleanly and a fresh state is published.
- Extend regression coverage to prove the 66-entry 0x19/0x0A response, exact first/last DTC identities and catalogue size, while retaining lifecycle, clear/status-mask and multi-page crash-recovery tests.

## 0.15.56 — 2026-09-22

- Resolve LINK #39's remaining storage-capacity defect by changing the STM32F103 journal from one-state-per-page rotation to crash-consistent multi-page generation slots; large persistent state can span as many reserved pages as required, with at least two complete slots retained for rollback.
- Add forced multi-page regression coverage that writes across two physical pages, reboots successfully, corrupts the newest slot and proves recovery to the previous CRC-valid generation while retaining the existing N-page wear-distribution test.
- Resolve LINK #40's portable-reference policy mismatch: ClearDiagnosticInformation (0x14) and ReadDTCInformation (0x19) are exercised across Default, Programming, Extended and SafetySystemDiagnostic sessions without an invented SecurityAccess prerequisite; 0x14 is also proven usable after SecurityAccess raises the active level.
- Resolve LINK #41's STM32F103 status-advertisement mismatch by reporting DTCStatusAvailabilityMask 0x7F because the reference lifecycle owns bits 0..6 but does not model warningIndicatorRequested; generic 0xFF request-mask support is unchanged.
- Correct the STM32 documentation and issue #37 interpretation carried forward from 0.15.51: a default-session all-group clear now returns positive 0x54 in the portable reference, while the post-clear 0x50 status still means `19 01 FF` can count definitions and a fault-oriented mask such as 0x0D is the correct cleared-fault check.
- Full pre-release qualification passed on Linux strict C11, ASan/UBSan, Windows strict C11, macOS strict C11, STM32 Cortex-M cross-compile and native Linux adapter-provider coverage before this release metadata was cut.

## 0.15.55 — 2026-09-22

- Correct the Common 1.19.23 dependency metadata exposed by the 0.15.54 matrix: the submodule was already pinned to `a9cf2957cffeefe6001830916b8a32c2ef58a551`, while LINK's CMake invariant still named Common 1.19.22 / the previous commit.
- Carry forward the fully qualified DTC lifecycle engine, STM32 tester/server clarification, clear/session regression, exact issue-27 Cube-main qualification and release-race fix.
- Keep the dependency invariant strict: configuration fails if either the Common version or exact git revision disagrees with LINK's declared pin.

## 0.15.54 — 2026-09-22

- Pin LINK to Infiltratr Common 1.19.23 at a9cf2957cffe, preserving the fully qualified 0.15.53 DTC lifecycle and STM32 fixes on the newest shared foundation.
- No LINK protocol semantics are changed by this release; it is the canonical dependency-qualified head for downstream vehicle products.

## 0.15.53 — 2026-09-22

- Publish the fully qualified follow-up to 0.15.52 after correcting the DTC lifecycle regression expectations and MSVC status-mask warnings found by the full cross-platform matrix.
- Retain the new configurable DTC lifecycle engine requested in issue #38: functional-group metadata, ISO-scaled fault-detection counters, confirmation-cycle tracking, aging counters, clear/reset semantics and STM32F103 persistence/integration.
- Retain the issue #36 tester/server role split, issue #37 clear/session regression and exact STM32C092 issue-27 Cube-main cross-compile qualification.
- The complete LINK CI matrix is green on the exact pre-release head across Ubuntu, macOS, Windows, ASan/UBSan, STM32/Cortex-M and native Linux provider coverage.

## 0.15.52 — 2026-09-22

- Implement LINK issue #38's missing DTC lifecycle layer as a portable allocation-free reference engine instead of leaving 0x19 as response framing over manually injected status bytes.
- Add FunctionalGroupIdentifier/severity/functional-unit definition metadata, signed ISO-scaled fault-detection-counter progression, per-operation-cycle FDC reset, prefailed (+1..+126) reporting, +127 failed-state transition, failed-cycle confirmation and configurable passed-cycle aging.
- Keep the OEM boundary explicit: ISO-visible state is shared, while counter step sizes, confirmation thresholds and aging policy remain configurable product policy rather than being falsely labelled universal or Mercedes-specific.
- Integrate the shared lifecycle into the STM32F103 ECU/server reference, bump its persisted-state schema, expose begin/report/end operation-cycle APIs and make ClearDiagnosticInformation reset lifecycle/aging state atomically.
- Avoid flash-endurance damage by keeping monitor samples in RAM and committing the reference journal at operation-cycle completion rather than erasing/programming flash for every test execution.
- Expose the reference lifecycle through application-defined extended-data record 1 (aging count, failed-cycle count, signed FDC byte, FunctionalGroupIdentifier) and add end-to-end 0x19/0x14, confirmation, aging, persistence-boundary and clear regressions.
- Document the distinction between UDS DTC report framing and manufacturer/product diagnostic-monitor policy in `docs/UDS-DTC-LIFECYCLE.md`.

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
