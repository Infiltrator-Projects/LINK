# Security

## Scope

Security issues include malformed vehicle/adaptor input causing memory corruption, unsafe diagnostic transmit permission, credential/key handling defects, insecure Bluetooth/USB/J2534 integration, unsafe file/evidence handling, and any path that can convert read-only behaviour into an unintended vehicle write.

## Reporting

Do not publish sensitive exploit, credential or vehicle-security material in a public issue. Use GitHub private vulnerability reporting/security advisories when available.

Include the exact revision/dependency tree, adapter/platform, reproduction and whether the issue requires a physical vehicle.

## Response

Security fixes should preserve deny-by-default behaviour, add regression coverage where practical and validate the narrowest affected layer plus any real hardware boundary implicated.

## Supported source

Current main and the current release line are the primary maintained sources unless explicitly documented otherwise.
