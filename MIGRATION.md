# Migration staging

This repository is being populated incrementally from MBLINK and JAGLINK.

Raw imports under `migration/` are temporary comparison snapshots only. They are not the final LINK API. Product-specific Mercedes and Jaguar code is intentionally excluded. Shared candidates are promoted into neutral `include/link`, `src`, `platform`, `app`, `tests`, and build/release paths only after comparison and neutralisation.

The dependency hierarchy remains: Infiltratr Common -> LINK -> MBLINK/JAGLINK.
