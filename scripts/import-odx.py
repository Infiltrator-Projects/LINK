#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Normalise ODX/PDX diagnostic descriptions into LINK-neutral JSON.

This is a development/import tool.  It deliberately imports odxtools at
runtime so LINK's C library and applications do not acquire a Python or
odxtools runtime dependency.
"""
from __future__ import annotations

import argparse
import importlib.metadata
import json
from pathlib import Path
from typing import Any, Iterable


def _items(value: Any) -> list[Any]:
    if value is None:
        return []
    try:
        return list(value)
    except TypeError:
        return []


def _plain(value: Any) -> Any:
    if value is None or isinstance(value, (str, int, float, bool)):
        return value
    name = getattr(value, "value", None)
    if isinstance(name, (str, int, float, bool)):
        return name
    return str(value)


def _integer(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    try:
        if value is not None:
            return int(value)
    except (TypeError, ValueError):
        pass
    return None


def _parameter_summary(parameter: Any) -> dict[str, Any]:
    result: dict[str, Any] = {
        "kind": type(parameter).__name__,
        "short_name": getattr(parameter, "short_name", None),
    }
    for attr in ("byte_position", "bit_position", "coded_value"):
        value = getattr(parameter, attr, None)
        if value is not None:
            result[attr] = _plain(value)

    diag_type = getattr(parameter, "diag_coded_type", None)
    if diag_type is None:
        dop = getattr(parameter, "dop", None)
        diag_type = getattr(dop, "diag_coded_type", None) if dop is not None else None
    bit_length = getattr(diag_type, "bit_length", None)
    if bit_length is not None:
        result["bit_length"] = _plain(bit_length)
    return result


def _message_summary(message: Any) -> dict[str, Any] | None:
    if message is None:
        return None
    parameters = [_parameter_summary(p) for p in _items(getattr(message, "parameters", None))]
    return {
        "short_name": getattr(message, "short_name", None),
        "parameters": parameters,
    }


def _coded_prefix(message: Any) -> str | None:
    if message is None:
        return None
    bytes_by_position: dict[int, int] = {}
    for parameter in _items(getattr(message, "parameters", None)):
        position = _integer(getattr(parameter, "byte_position", None))
        coded = _integer(getattr(parameter, "coded_value", None))
        bit_position = _integer(getattr(parameter, "bit_position", None))
        if position is None or coded is None or bit_position not in (None, 0):
            continue

        diag_type = getattr(parameter, "diag_coded_type", None)
        if diag_type is None:
            dop = getattr(parameter, "dop", None)
            diag_type = getattr(dop, "diag_coded_type", None) if dop is not None else None
        bit_length = _integer(getattr(diag_type, "bit_length", None))
        if bit_length is None:
            bit_length = 8
        if bit_length <= 0 or bit_length > 32 or bit_length % 8 != 0:
            continue
        width = bit_length // 8
        if coded < 0 or coded >= (1 << bit_length):
            continue
        raw = coded.to_bytes(width, byteorder="big")
        for offset, byte in enumerate(raw):
            slot = position + offset
            previous = bytes_by_position.get(slot)
            if previous is not None and previous != byte:
                return None
            bytes_by_position[slot] = byte

    prefix = bytearray()
    while len(prefix) in bytes_by_position:
        prefix.append(bytes_by_position[len(prefix)])
    return prefix.hex().upper() if prefix else None


def _safe_can_id(ecu: Any, method_name: str) -> int | None:
    method = getattr(ecu, method_name, None)
    if not callable(method):
        return None
    try:
        return _integer(method())
    except Exception:
        return None


def _service_summary(service: Any) -> dict[str, Any]:
    request = getattr(service, "request", None)
    return {
        "short_name": getattr(service, "short_name", None),
        "semantic": _plain(getattr(service, "semantic", None)),
        "request_coded_prefix": _coded_prefix(request),
        "request": _message_summary(request),
        "positive_responses": [
            _message_summary(message)
            for message in _items(getattr(service, "positive_responses", None))
        ],
        "negative_responses": [
            _message_summary(message)
            for message in _items(getattr(service, "negative_responses", None))
        ],
    }


def normalise_database(db: Any, source: Path, odxtools_version: str) -> dict[str, Any]:
    ecus = []
    for ecu in _items(getattr(db, "ecus", None)):
        ecus.append({
            "short_name": getattr(ecu, "short_name", None),
            "variant_type": _plain(getattr(ecu, "variant_type", None)),
            "can_receive_id": _safe_can_id(ecu, "get_can_receive_id"),
            "can_send_id": _safe_can_id(ecu, "get_can_send_id"),
            "services": [
                _service_summary(service)
                for service in _items(getattr(ecu, "services", None))
            ],
        })
    return {
        "schema_version": 1,
        "generator": "LINK scripts/import-odx.py",
        "odxtools_version": odxtools_version,
        "source_file": source.name,
        "policy": {
            "runtime_dependency": False,
            "manufacturer_data_is_not_promoted_without_verification": True,
        },
        "ecus": ecus,
    }


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Normalise an ODX/PDX database into LINK-neutral JSON")
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    args = parser.parse_args()

    try:
        import odxtools  # type: ignore
    except ImportError:
        parser.error("odxtools is required for this import tool (pip install odxtools)")

    source = args.input.resolve()
    if not source.is_file():
        parser.error(f"input file does not exist: {source}")

    if source.suffix.lower() == ".pdx" and hasattr(odxtools, "load_pdx_file"):
        db = odxtools.load_pdx_file(str(source))
    else:
        db = odxtools.load_file(str(source))

    try:
        version = importlib.metadata.version("odxtools")
    except importlib.metadata.PackageNotFoundError:
        version = "unknown"

    document = normalise_database(db, source, version)
    encoded = json.dumps(document, indent=2, sort_keys=True) + "\n"
    if args.output is None:
        print(encoded, end="")
    else:
        args.output.write_text(encoded, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
