<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux diagnostic-adapter transports

Link owns the native Linux vehicle-interface layer used by MBLink, JAGLink and
other Link-family products. Product code does not implement its own Vgate,
Bluetooth or Tactrix transport.

The shared Linux adapter chooser supports:

- ordinary ELM-compatible tty devices such as `/dev/ttyUSB*` and
  `/dev/ttyACM*`;
- existing `/dev/rfcomm*` ELM-compatible devices;
- native BlueZ Bluetooth Low Energy/GATT ELM-compatible adapters;
- native BlueZ Bluetooth Classic/SPP ELM-compatible adapters; and
- native USB **Tactrix OpenPort 2.0** devices.

## Vgate / ELM327

BLE selections are represented as `BLE:<address> <name>` and Bluetooth
Classic selections as `BT:<address> <name>`. Link filters discovery toward
likely ELM/OBD devices rather than showing unrelated phones, mice and generic
BLE beacons.

BLE uses BlueZ D-Bus/GATT directly. Link refreshes the LE device, connects,
waits for GATT service resolution, chooses writable/notifiable characteristics,
enables notifications and verifies the adapter with `ATI`.

Bluetooth Classic performs native SDP discovery for the Serial Port Profile and
opens an RFCOMM socket directly. If SDP is unavailable before connection, Link
falls back to RFCOMM channel 1, which is common for ELM327-compatible adapters.
No manual `rfcomm bind` is required.

Dual-mode Vgate adapters can expose both sides independently. For example,
`IOS-Vlink` normally appears over BLE/GATT and `ANDROID-Vlink` normally
appears over Bluetooth Classic/SPP. Link uses the transport actually selected;
the advertising name does not hard-code the host operating system.

## Tactrix OpenPort 2.0

On Linux, a connected OpenPort 2.0 appears as:

`OP2:Tactrix OpenPort 2.0`

Link talks to it **directly over USB through libusb**. It does not require the
Windows Tactrix DLL, Wine, a separately installed J2534 library or a
product-specific driver.

The USB/J2534 wire backend is based on the BSD-3-Clause OpenPort implementation
vendored under `third_party/openport2-j2534`, with attribution and licence kept
alongside the source. Link owns the surrounding adapter discovery, lifecycle,
diagnostic transaction bridge, state handling and product integration.

The current native Linux OpenPort path supports the CAN/ISO 15765 modes used by
the present Link-family diagnostics:

- 11-bit ISO 15765 at 500 kbit/s;
- 11-bit ISO 15765 at 250 kbit/s;
- directed 29-bit ISO 15765 at 500 or 250 kbit/s;
- functional OBD requests with all eight standard `0x7E8-0x7EF` responders;
- directed transmit/receive identifiers;
- ISO-TP flow-control filters;
- response-pending handling; and
- read-only Link diagnostic, inventory and manufacturer-scan traffic.

Automatic protocol acquisition starts with 11-bit 500 kbit/s and then 11-bit
250 kbit/s. Explicit protocol/header selections cover 29-bit traffic. K-line is
supported by OpenPort 2.0 hardware/J2534 generally, but **is not yet exposed by
this Link Linux transaction bridge** and must not be implied by product UIs.

The provider deliberately translates the OpenPort/J2534 transaction into the
same Link diagnostic response model already used by ELM adapters. Mercedes and
Jaguar code therefore stays above the transport boundary and does not fork for
Tactrix.

## USB permissions

Link packages install a shared udev rule for USB VID:PID `0403:cc4d`:

- the active desktop seat receives `uaccess`; and
- members of `dialout` receive group access.

The rule uses mode `0660`; the OpenPort is not made world-writable. Replugging
the OpenPort after package installation is normally sufficient for the rule to
take effect.

Some OpenPort firmware/USB implementations have historically behaved better
with the microSD card removed. Treat that as a troubleshooting step rather than
a normal requirement.

## Ownership

The transport layer owns framing, connection lifecycle, USB/Bluetooth details
and adapter semantics. The portable Link diagnostic engine owns OBD-II, ISO-TP,
UDS and diagnostic sequencing. Mercedes- and Jaguar-specific knowledge remains
in the appropriate product repositories.
