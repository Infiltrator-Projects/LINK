<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux Bluetooth transport

LINK 0.14.0 adds a native BlueZ BLE/GATT path alongside the existing Linux tty, USB serial and RFCOMM paths.

The GTK shell discovers `/dev/ttyUSB*`, `/dev/ttyACM*`, `/dev/rfcomm*` and nearby Bluetooth LE devices. BLE choices are represented internally as `BLE:<address> <name>`. Selecting one and pressing **LINK UP** performs an LE discovery refresh, connects through `org.bluez.Device1`, waits for GATT service resolution, inspects the device's characteristics and chooses a compatible writable/notifiable characteristic pair. It then enables notifications and sends `ATI`. The adapter is promoted to the normal LINK byte-stream transport only if a complete ELM-style prompt response is received.

No Vgate-specific GATT UUID is required. The selector supports both a single UART characteristic carrying write and notify flags and split write/notify characteristics within the same GATT service. Common Vgate iCar Pro layouts therefore work without special-casing their UUIDs, while the same path remains usable by other ELM-compatible BLE adapters.

Writes are split into conservative 20-byte ATT payloads so longer ELM text commands can cross the default BLE MTU without assuming a negotiated larger MTU. The ELM layer above the provider still owns command framing, parser state, timeouts and diagnostic semantics.

The implementation talks to the system BlueZ daemon directly through GIO/GDBus. It does not spawn `bluetoothctl`, does not require an RFCOMM bridge for BLE, and does not move Mercedes- or Jaguar-specific diagnostic knowledge into LINK.
