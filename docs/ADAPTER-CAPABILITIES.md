<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Adapter capability model

Link treats an adapter as a transport implementation, not as the source of
vehicle knowledge. A Mercedes, Jaguar or generic request is described once as
CAN endpoint + payload + timing + response policy. The provider then chooses
the strongest proved execution path for the connected hardware.

## Current capability profiles

| Adapter kind | ELM command surface | Native raw CAN | Native ISO-TP | 11-bit | 29-bit | Multi-response | Secure native session |
| --- | --- | --- | --- | --- | --- | --- | --- |
| ELM327 / Vgate-class | yes | not assumed | through ELM command surface | yes where runtime probe confirms | yes where runtime probe confirms | supported by Link parser/channel policy | no |
| Tactrix OpenPort 2.0 | compatibility surface in Link | provider/J2534 layer | J2534 ISO15765 | yes | yes | yes | no |
| STM32 Link target | no | yes | Link ISO-TP | yes | yes | yes | no |
| Mercedes me Adapter | no | **yes, proved** | **yes, proved** | **yes** | not yet claimed | **yes, native symbols prove it** | **yes, recovered envelope** |

The ELM row describes what Link knows how to request. Individual ELM clones
remain runtime-probed because a clone may omit or misimplement commands.

The Mercedes row is deliberately conservative. The archived 4.7.61 GDK proves
standard CAN IDs through `0x7FF`, raw payloads through 8 bytes, ISO-TP
transmit payloads through 100 bytes, up to 15 CAN filter IDs and multi-response
ISO-TP operation. Link does not advertise a 29-bit native Mercedes command
encoding until the binary or physical adapter proves one.

## One request, several adapters

`LinkDiagnosticRequestDefinition` is the common contract. It carries:

- request and optional known response CAN IDs;
- 11/29-bit addressing selection;
- diagnostic PDU bytes;
- timeout/retry/P2*/P3 policy;
- optional padding; and
- response-selection policy.

The shared execution selector currently chooses:

- **ELM command surface** for Vgate/ELM and the OpenPort compatibility path;
- **native ISO-TP** for the STM32 target; and
- **native ISO-TP** for the Mercedes me Adapter.

This prevents MBLink from embedding four copies of the same Mercedes request.

## Multi-ECU response handling

The response engine implements the exact strategy vocabulary recovered from
Mercedes Whisper:

- `SELECT_FIRST`;
- `SELECT_LOWEST_CANID_CACHED`;
- `SELECT_MAXIMUM`; and
- `MERGE_ELIMINATE_DUPLICATES`.

The policies are product-neutral once recovered. A numeric maximum requires a
caller-supplied decoder; Link never interprets arbitrary bytes as a number.

This is particularly useful with functional OBD requests or manufacturer
requests that can legitimately produce more than one ECU response.

## Retired Mercedes me Adapter

The genuine Mercedes me Adapter is now a first-class native diagnostic target,
not an ELM impersonator.

For the proved 11-bit path, Link translates one common request to:

```text
I01<request:4HEX><response:4HEX><flags:2HEX><padding:2HEX>\r
i01<request:4HEX><Base64(PDU)>\r
```

For example, a physical `0x7E0 -> 0x7E8` ReadDataByIdentifier request
`22 F1 90`, padding disabled and raw responses allowed becomes:

```text
I0107E007E800AA\r
i0107E0IvGQ\r
```

If the established adapter session requires secure command transport, either
plaintext command can be wrapped by the recovered:

```text
a<Base64(AES-256-ECB(inner-frame, SessionKey))>\r
```

The remaining blocker for autonomous physical use is not CAN/ISO-TP framing or
local authentication ordering. The secure path is now proved as application
random in GetSeed, device random in the response, device challenge through
SetKey, then `SHA-256(SMK || device_random || app_random)`. What remains
external is acquisition of the backend-provisioned SMK for a specific
commissioned adapter, plus validation against physical hardware. Link keeps
that boundary explicit rather than guessing and risking lockout; see
[`MERCEDES-ME-AUTH-FORENSICS.md`](MERCEDES-ME-AUTH-FORENSICS.md).

## Ownership boundary

Vehicle definitions stay above the adapter:

```text
MBLink/JAGLink vehicle knowledge
        |
LinkDiagnosticRequestDefinition
        |
Link response/timing/ISO-TP policy
   /       |        |         \
ELM     OpenPort   STM32   Mercedes me
```

That also means improvements recovered from the retired Mercedes software,
such as robust response selection and acquisition policy, can improve every
adapter without making other hardware emulate the proprietary protocol.
