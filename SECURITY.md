<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Security

## Supported source

Security fixes target current `main` and, where appropriate, the latest release.

## Reporting

Do not publish vulnerabilities that could expose vehicles, users, private data, credentials, adapter secrets, build infrastructure or signing material.

Use GitHub private vulnerability reporting when available. Otherwise contact `infiltratr@yandex.com` with the subject `LINK security report`.

Include exact revision, platform/provider/adapter context, impact, reproduction and sanitised evidence.

## Security-sensitive contracts

Particular attention belongs to:

- diagnostic transmit allowlists and request classification;
- ISO-TP/UDS/KWP/DoIP parsing of untrusted vehicle/adapter traffic;
- Bluetooth, J2534, USB and embedded provider boundaries;
- security/session material and any adapter-specific secret handling;
- malformed evidence/catalogue input;
- memory safety, integer/buffer bounds and concurrency;
- release/dependency identity.

A successful simulated exchange is not proof of safe physical-vehicle behaviour.

## Response

Reproduce at the safest narrow layer, add regression coverage, fix the owning contract and validate physical hardware when required. Do not test in ways that can endanger people, vehicles or third-party systems.

## Disclosure

Public disclosure should follow a fix or mitigation and identify affected/corrected release identities.