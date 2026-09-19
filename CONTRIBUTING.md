<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to LINK

LINK owns product-neutral automotive diagnostics for the Infiltrator vehicle family. Changes here have a wide blast radius because MBLINK, JAGLINK, BMWLINK, AUDILINK and FORDLINK depend on this source.

## Ownership

- Common owns broadly reusable non-automotive primitives.
- LINK owns transports, adapter capability, standard OBD knowledge, ISO-TP, UDS/KWP/DoIP codecs, diagnostic sequencing, evidence, safety policy and common diagnostic application behaviour.
- Product repositories own branding and genuinely manufacturer-specific knowledge.
- Do not move manufacturer-specific assumptions into LINK merely because more than one vehicle happens to use a similar service.

## Language and implementation

C and C++ are preferred first-party implementation languages; choose the one that produces the strongest implementation for the component. Platform providers may use required native languages at their boundary. Shared protocol/state semantics must not be duplicated in Swift/Objective-C/product shells.

## Safety

Codec availability, adapter capability, diagnostic request planning and transmit permission are separate contracts. Any change that can broaden transmitted requests requires explicit safety regression coverage.

## Build and test

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Keep strict warnings, sanitizer coverage and installed-package consumer tests healthy. Physical adapter claims require physical evidence beyond host simulation.

## Standards and evidence

Licensed standards that cannot be redistributed are recorded through provenance and verified public baselines rather than guessed current rows. Captures and external tool observations are evidence; LINK's reviewed source remains the product contract.

## Documentation

Use `docs/README.md` as the map. Update architecture, design, decisions, roadmap and validation in the same change as the contract they describe.

## Repository policy

`main` is the development/release branch. Published tags/releases are immutable exact-source identities.

Participation standards remain in [.github/CODE_OF_CONDUCT.md](.github/CODE_OF_CONDUCT.md).