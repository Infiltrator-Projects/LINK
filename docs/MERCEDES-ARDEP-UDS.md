# Mercedes-Benz ARDEP UDS reference

Mercedes-Benz publishes **ARDEP (Automotive Rapid Development Platform)**,
an Apache-2.0 open-source automotive development platform with CAN/LIN support
and a Zephyr-based UDS (ISO 14229) implementation.

LINK does not depend on ARDEP at runtime. Instead, ARDEP is used as an
independent manufacturer-published conformance reference for LINK's generic UDS
layer.

ARDEP currently documents/implements these services:

`0x10 0x11 0x14 0x19 0x22 0x23 0x27 0x28 0x29 0x2C 0x2E 0x2F 0x31 0x34 0x35 0x36 0x37 0x38 0x3D 0x3E 0x85 0x87`

LINK's standard catalogue covers all of them and additionally includes services
such as ReadScalingDataByIdentifier, periodic DID reads, timing access, secured
data transmission and ResponseOnEvent.

The automated `link-ardep-uds-reference` test checks that every ARDEP service
above remains represented in LINK. ARDEP's README service table currently shows
LinkControl as `0x86`, but ARDEP's own Kconfig, section heading and tests use
the ISO 14229 LinkControl SID `0x87`; LINK therefore treats `0x87` as the
reference value and keeps `0x86` for ResponseOnEvent.
