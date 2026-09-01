# OBD standards coverage

This is LINK's canonical generic OBD implementation boundary. It distinguishes
native implementation, adapter-managed support, raw-preserving interoperability,
and named/scaled catalogue coverage.

LINK does not vendor SAE or ISO standards text. Unknown layouts stay raw rather
than being guessed.

| Standard / surface | Status | Boundary |
| --- | --- | --- |
| SAE J1979 services | Implemented service model | Modes 01 through 0A are modelled; read-only requests are available for 01/05/06/09, freeze frame for 02, DTC inventory for 03/07/0A, and 04/08 remain explicitly gated as state-changing. |
| J1979 Mode 01 | Complete byte-wide namespace | 256 identifier slots, 220 assigned and schedulable under J1979DA_202608. |
| J1979 Mode 05 | Complete addressability, raw-preserving | Every byte-wide test identifier can be requested and an ECU-advertised payload retained even when no verified semantic layout is available. |
| J1979 Mode 06 | Complete addressability, raw-preserving | Every byte-wide monitor identifier can be requested and retained; support-page discovery is shared with other parameterized modes. |
| J1979 Mode 09 | Named catalogue plus raw fallback | Known information types are decoded; additional ECU-advertised types remain available as raw bytes. |
| SAE J2012/J2012DA | Complete numeric namespace classification | All 65,536 OBD DTC values are classified; the open generic description catalogue contains 9,533 audited definitions. |
| SAE J1978-1 first-generation protocol access | Implemented through ELM | Protocols 1..9 are explicitly modelled/selectable: J1850 PWM, J1850 VPW, ISO 9141-2, ISO 14230-4 slow/fast and ISO 15765-4 CAN 11/29-bit at 250/500 kbit/s. ATDP/ATDPN remains authoritative for actual hardware. |
| SAE J1850 | Adapter-managed | Both regulated ELM variants are selectable/reportable. LINK does not mislabel an ELM-managed electrical bus as a native J1850 transceiver. |
| ISO 9141-2 | Adapter-managed | ELM protocol 3 is explicit and classified as 5-baud init. |
| ISO 14230-4 | Adapter-managed plus application helpers | ELM protocols 4/5 cover OBD slow/fast init; LINK also owns portable read-only KWP2000 application helpers. |
| ISO 15765-2/-4 | Native and adapter-managed | LINK owns ISO-TP over Classical CAN/CAN-FD plus ELM/J2534 paths and 11/29-bit addressing. |
| SAE J1979-2 OBDonUDS | Foundation audited to J1979-2_202604 | F400/F500 DID mapping, UDS/ISO-TP and portable DoIP diagnostic framing are in LINK. |
| ISO 13400 DoIP | Portable codec | Generic header, routing-activation request, alive-check response and diagnostic-message framing/decoding are in Core; socket discovery/TCP connection remains platform-provider work. |
| ISO 27145 WWH-OBD | Transport/application foundation | DoCAN and DoIP foundations plus UDS are present. Full regulatory conformance is not claimed without a conformance suite. |
| SAE J1962 | Physical connector | Not a software protocol implementation. |
| SAE J1939 | Outside classic J1979 scope | ELM protocol A is represented for honest adapter reporting; LINK does not claim a complete J1939 stack. |

CI regression requirements: ELM protocols 1..9 must remain selectable and
correctly classified; Mode 05/06 unknown semantics must remain lossless; J1979
Mode 01 and J2012 audits must remain pinned; J1979-2 revision reporting must
remain current; DoIP framing must round-trip and reject malformed/truncated
frames.
