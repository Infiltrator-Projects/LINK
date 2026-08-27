# OpenPort 2.0 J2534 USB implementation

LINK's native Linux Tactrix OpenPort 2.0 provider incorporates the BSD-3-Clause
OpenPort/J2534 implementation by Nikola Kozina and Dale Schultz from
https://github.com/NikolaKozina/j2534.

The imported source is kept private to LINK's Linux adapter layer. LINK adds its
own transport integration, ELM-compatible transaction bridge, safety/state
handling, discovery, packaging and UI integration around it.

The accompanying LICENSE file is the upstream BSD-3-Clause licence.
