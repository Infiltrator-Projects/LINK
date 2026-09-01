#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail

VECTOR="${1:-./build/link-isotp-vector}"
PAYLOAD="${2:-22F19000112233445566778899AABBCCDDEEFF}"
IFACE="${3:-vcan0}"

for cmd in candump isotpsend isotprecv ip; do
    command -v "$cmd" >/dev/null || {
        echo "missing required command: $cmd" >&2
        exit 2
    }
done
[[ -x "$VECTOR" ]] || { echo "LINK vector tool not executable: $VECTOR" >&2; exit 2; }
ip link show "$IFACE" >/dev/null 2>&1 || {
    echo "$IFACE does not exist; create and bring up a vcan interface first" >&2
    exit 2
}

tmp="$(mktemp -d)"
trap 'kill "${cap_pid:-}" "${recv_pid:-}" 2>/dev/null || true; rm -rf "$tmp"' EXIT

"$VECTOR" "$PAYLOAD" >"$tmp/link.frames"

candump -L "$IFACE" >"$tmp/candump.log" 2>/dev/null &
cap_pid=$!
isotprecv -s 321 -d 123 "$IFACE" >"$tmp/received.hex" 2>/dev/null &
recv_pid=$!
sleep 0.1
printf '%s\n' "$PAYLOAD" | isotpsend -s 123 -d 321 "$IFACE"
sleep 0.2
kill "$cap_pid" "$recv_pid" 2>/dev/null || true
wait "$cap_pid" "$recv_pid" 2>/dev/null || true

awk '$3 ~ /^123#/ { print $3 }' "$tmp/candump.log" >"$tmp/kernel.frames"

if diff -u "$tmp/link.frames" "$tmp/kernel.frames"; then
    echo "LINK ISO-TP framing matches Linux SocketCAN ISO-TP for this vector."
else
    echo "ISO-TP differential mismatch." >&2
    echo "LINK frames: $tmp/link.frames" >&2
    echo "Kernel capture: $tmp/kernel.frames" >&2
    exit 1
fi
