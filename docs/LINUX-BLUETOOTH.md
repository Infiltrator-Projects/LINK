<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux Bluetooth transport

LINK 0.14.3 provides native BlueZ Bluetooth Low Energy (GATT) and Bluetooth Classic Serial Port Profile (RFCOMM) transports alongside ordinary Linux tty devices.

The GTK shell discovers `/dev/ttyUSB*`, `/dev/ttyACM*`, existing `/dev/rfcomm*`, likely ELM/OBD BLE devices and likely ELM/OBD Bluetooth Classic devices. BLE selections are represented as `BLE:<address> <name>` and Classic selections as `BT:<address> <name>`. Unrelated phones, mice and generic BLE beacons are no longer emitted into the diagnostic-adapter chooser.

Selecting a BLE adapter performs an LE refresh, connects through `org.bluez.Device1`, waits for GATT service resolution, chooses a writable/notifiable characteristic pair, enables notifications and verifies the adapter with `ATI`. No Vgate-specific GATT UUID is required.

Selecting a Bluetooth Classic adapter performs native SDP discovery for the Serial Port Profile and opens an RFCOMM socket directly. If a device does not expose SDP before connection, LINK falls back to RFCOMM channel 1, which is common for ELM327-compatible adapters. No manual `rfcomm bind` or `/dev/rfcomm0` setup is required. Existing `/dev/rfcomm*` devices remain supported for compatibility.

Vgate dual-mode adapters can therefore expose both sides independently: a name such as `IOS-Vlink` is normally reached over BLE/GATT, while `ANDROID-Vlink` is normally reached over Bluetooth Classic/SPP. The product UI uses the selected transport rather than assuming that either advertising name is tied to the host operating system.

The ELM layer above both providers owns command framing, parser state, timeouts and diagnostic semantics. Mercedes- and Jaguar-specific diagnostic knowledge remains outside LINK.
