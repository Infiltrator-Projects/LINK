<!-- SPDX-License-Identifier: GPL-3.0-or-later -->

# Contributing to LINK

LINK owns product-neutral automotive transport, ELM327, OBD-II, ISO-TP, UDS,
diagnostic-flow, and evidence behavior. MBLINK and JAGLINK should remain thin
product/manufacturer layers, while non-automotive reusable primitives belong in
Infiltratr Common.

Keep portable behavior in strict C11 and platform providers at their native OS
boundaries. Do not duplicate shared protocol logic in Objective-C, Swift, or
product repositories. Treat undocumented identifiers as experimental until
their provenance and real-vehicle behavior are verified.

Every behavior change needs deterministic regression coverage. Builds must
remain warning-clean under the repository's strict compiler settings, and
source files must retain `GPL-3.0-or-later` SPDX identifiers.

Run the supported build before committing:

```sh
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Keep commits focused and update the documentation that owns any changed
contract. Contributions are accepted under `GPL-3.0-or-later`.
