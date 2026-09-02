# OBD standards coverage

## Diagnostic generations and capability detection

LINK classifies diagnostic generation from observed communication, not from a
vehicle's model year or connector shape.

- **Legacy / OBD-I-era**: a positively identified legacy diagnostic surface
  answers and no standard OBD surface answers.
- **Transitional / OBD1.5**: both a standards-shaped OBD surface and a legacy
  diagnostic surface answer. "OBD1.5" is an informal project/UI label rather
  than a universal SAE or ISO standard.
- **Standard OBD-II / EOBD**: a standards-shaped OBD surface answers and no
  legacy surface has been positively identified.
- **Unknown / probing**: evidence is still incomplete.
- **No supported diagnostics detected**: the bounded probe has completed
  without either supported surface.

The classification reports software-visible capability. It is not a legal
emissions-compliance certification.

The 1999 AU Falcon is retained as a project regression/example target for the
transitional category: FORDLINK contributes Ford-specific legacy evidence while
LINK owns the common classification and presentation.


This is LINK's canonical generic OBD implementation boundary. Protocol support,
current-standard targeting, semantic-registry coverage and regulatory conformance
are deliberately separate claims.

Current standards targets:
- SAE J1979_202505;
- SAE J1979DA_202607;
- SAE J1978-1_202604; and
- SAE J1979-2_202604.

The current J1979 Digital Annex is a licensed SAE data product and is not
redistributed by LINK. `link/j1979da.h` therefore reports that current target
separately from the compiled public semantic baseline
`J1979DA_201110+verified-public-updates`. Unknown current-annex rows are retained
as raw data; LINK never assigns guessed names, lengths, units or scaling.

| Standard / surface | LINK status | Exact boundary |
| --- | --- | --- |
| SAE J1979 service model | Implemented | Services 01 through 0A are represented; state-changing 04/08 remain authorization-gated. |
| Mode 01 current data | Complete byte-wide addressability; extensive public semantic catalogue | 256 slots are classified and 220 currently-known assigned slots are schedulable. Publicly verified layouts are decoded; other assigned layouts stay raw. The current J1979DA_202607 target is not represented as a claim that every licensed DA row is redistributed. |
| Mode 02 freeze frame | Implemented | Correct PID + freeze-frame-number request and standard response handling. |
| Mode 03/07/0A DTC inventory | Implemented | Stored, pending and permanent standardized DTC inventories. |
| Mode 04 clear | Implemented but gated | Requires explicit authorization and acknowledgement of readiness reset. |
| Mode 05 oxygen-sensor monitoring | Correct legacy message format + verified semantic baseline | Requests are `05 TID O2S`, not the invalid two-byte `05 TID` shortcut. Standard TIDs 01-0A and verified legacy scaling bands are represented; 45/TID/O2S result records are decoded. |
| Mode 06 non-continuous monitors | Structured CAN decoder + verified registry baseline | Positive 46 responses are decoded as repeated 9-byte OBDMID/TID/UASID/value/min/max records. Public standardized monitor IDs and UASID scaling are compiled. Public change-history assignments such as later MIDs 11-14/51-54, later UASIDs 45/46/AB/B2/B3 and Mode 09 InfoTypes 12-29/79 are classified as standard even when their licensed semantic row is unavailable; such rows deliberately have no fabricated definition. Other post-baseline holes are UNVERIFIED, not falsely labelled reserved. |
| Mode 09 vehicle information | Named catalogue + raw fallback | VIN, CALID, CVN, IUPT, ECU name, ESN and other verified InfoTypes are represented. ESN is corrected to 17 characters and InfoType 0C is its message-count item. |
| SAE J2012/J2012DA | Complete numeric namespace classification | All 65,536 OBD DTC values are classified; the open generic description catalogue contains 9,533 audited definitions. |
| SAE J1978-1 protocol access | Implemented through adapter/provider layer | ELM protocol selection covers J1850 PWM/VPW, ISO 9141-2, ISO 14230-4 slow/fast and ISO 15765-4 CAN variants. Runtime protocol reporting remains authoritative for actual hardware. |
| SAE J1850 | Adapter-managed | LINK selects and reports PWM/VPW through capable hardware; it does not claim to be a bit-level J1850 transceiver. |
| ISO 9141-2 | Adapter-managed | Legacy K-line protocol selection is explicit. |
| ISO 14230-4 | Adapter-managed plus KWP helpers | Slow/fast-init protocol selection plus portable read-only KWP application helpers. |
| ISO 15765-2/-4 | Native and adapter-managed | LINK owns ISO-TP over Classical CAN/CAN-FD plus ELM/J2534 paths and 11/29-bit addressing. |
| SAE J1979-2 OBDonUDS | Implemented foundation | F400/F500 DID mapping, UDS/ISO-TP and DoIP diagnostic framing target J1979-2_202604. |
| ISO 13400 DoIP | Portable codec | Header, routing activation, alive-check and diagnostic-message framing/decoding are in Core; socket discovery/TCP connection remains provider-owned. |
| ISO 27145 WWH-OBD | Transport/application foundation | DoCAN, DoIP and UDS foundations are present. Full regulatory conformance is not claimed without a dedicated conformance suite. |
| SAE J1962 | Physical connector | Not a software protocol implementation. |
| SAE J1939 | Outside classic J1979 scope | ELM protocol A is represented for honest adapter reporting; no complete J1939 diagnostics stack is claimed. |

CI regression requirements:
1. revision strings must distinguish current target documents from compiled semantic data;
2. Service 05 must never regress to a two-byte `05xx` request;
3. Service 06 must parse 9-byte records and apply signed/unsigned UASID scaling;
4. the Mode 09 ESN count/value pair must remain structurally correct;
5. unknown current-annex semantics must remain explicit/raw rather than fabricated; and
6. transport support must never be presented as semantic or regulatory conformance.
