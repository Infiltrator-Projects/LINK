#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate LINK's generic OBD PID catalogue from pinned OBDex CC0 data.

The importer intentionally keeps standards data separate from transport and
product code. It validates the exact pinned Mode 01 + Mode 09 snapshot and
emits immutable C metadata consumed by LINK on every platform.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import unicodedata
from dataclasses import dataclass
from pathlib import Path

OBDEX_COMMIT = "bc58b0eb7273226a1aabae98e956b70b8362bda1"
EXPECTED = {"01": 119, "09": 13}
EXPECTED_TOTAL = 132

FORMULA_KINDS = {
    "100/255 * A": "LINK_OBD2_FORMULA_PERCENT_A",
    "A - 40": "LINK_OBD2_FORMULA_TEMP_A",
    "A": "LINK_OBD2_FORMULA_A",
    "(256*A + B) / 4": "LINK_OBD2_FORMULA_U16_DIV4",
    "(256*A + B) / 100": "LINK_OBD2_FORMULA_U16_DIV100",
    "100/128 * A - 100": "LINK_OBD2_FORMULA_TRIM_A",
    "3 * A": "LINK_OBD2_FORMULA_A_X3",
    "A/2 - 64": "LINK_OBD2_FORMULA_TIMING_ADVANCE",
    "A/200 (voltage); 100/128 * B - 100 (trim)": "LINK_OBD2_FORMULA_O2_VOLTAGE_TRIM",
    "256 * A + B": "LINK_OBD2_FORMULA_U16",
    "0.079 * (256 * A + B)": "LINK_OBD2_FORMULA_U16_X_079",
    "10 * (256 * A + B)": "LINK_OBD2_FORMULA_U16_X10",
    "(2/65536) * (256*A + B) (lambda); (8/65536) * (256*C + D) (voltage)": "LINK_OBD2_FORMULA_LAMBDA_VOLTAGE",
    "((256*A + B) / 4) - 8192": "LINK_OBD2_FORMULA_EVAP_SIGNED_QUARTER",
    "(2/65536) * (256*A + B) (lambda); (256*C + D)/256 - 128 (current, mA)": "LINK_OBD2_FORMULA_LAMBDA_CURRENT",
    "(256*A + B) / 10 - 40": "LINK_OBD2_FORMULA_U16_DIV10_MINUS40",
    "(256*A + B) / 1000": "LINK_OBD2_FORMULA_U16_DIV1000",
    "100/255 * (256*A + B)": "LINK_OBD2_FORMULA_U16_PERCENT",
    "(2 / 65536) * (256*A + B)": "LINK_OBD2_FORMULA_U16_LAMBDA",
    "256*A + B": "LINK_OBD2_FORMULA_U16",
    "A * 10": "LINK_OBD2_FORMULA_A_X10",
    "(256*A + B) / 200": "LINK_OBD2_FORMULA_U16_DIV200",
    "(256*A + B) - 32767": "LINK_OBD2_FORMULA_U16_MINUS32767",
    "100/128 * A - 100 (Bank 1); 100/128 * B - 100 (Bank 3)": "LINK_OBD2_FORMULA_TWO_TRIMS_13",
    "100/128 * A - 100 (Bank 2); 100/128 * B - 100 (Bank 4)": "LINK_OBD2_FORMULA_TWO_TRIMS_24",
    "((256*A + B) - 26880) / 128": "LINK_OBD2_FORMULA_INJECTION_TIMING",
    "(256*A + B) / 20": "LINK_OBD2_FORMULA_U16_DIV20",
    "A - 125": "LINK_OBD2_FORMULA_A_MINUS125",
    "A - 125 (idle); B - 125 (point2); C - 125 (point3); D - 125 (point4); E - 125 (point5)": "LINK_OBD2_FORMULA_TORQUE_FIVE",
    "((256*B + C) / 32) (sensor A); ((256*D + E) / 32) (sensor B)": "LINK_OBD2_FORMULA_MAF_TWO",
    "B - 40 (sensor 1); C - 40 (sensor 2)": "LINK_OBD2_FORMULA_TEMP_TWO",
    "((256*B + C)/10) - 40 (sensor 1); ((256*D + E)/10) - 40 (sensor 2); ((256*F + G)/10) - 40 (sensor 3); ((256*H + I)/10) - 40 (sensor 4)": "LINK_OBD2_FORMULA_EGT_FOUR",
    "(256*A + B) / 50 (engine fuel rate); (256*C + D) / 50 (vehicle fuel rate)": "LINK_OBD2_FORMULA_FUEL_RATE_TWO",
    "(256*A + B) / 5": "LINK_OBD2_FORMULA_U16_DIV5",
    "(256*A + B) / 32": "LINK_OBD2_FORMULA_U16_DIV32",
    "((A * 16777216) + (B * 65536) + (C * 256) + D) / 10": "LINK_OBD2_FORMULA_ODOMETER",
}


@dataclass(frozen=True)
class Row:
    mode: str
    pid: str
    byte_count: int
    unit: str
    name: str
    formula: str
    minimum: float | None
    maximum: float | None


def yaml_scalar(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == '"':
        return json.loads(value)
    if len(value) >= 2 and value[0] == value[-1] == "'":
        return value[1:-1].replace("''", "'")
    return value


def parse(path: Path) -> list[Row]:
    rows: list[Row] = []
    chunks = re.split(r"\n(?=- pid: )", path.read_text(encoding="utf-8"))
    for chunk in chunks:
        pid_match = re.search(r'^- pid: "([0-9A-F]+)"', chunk, re.M)
        mode_match = re.search(r'^  mode: "([0-9A-F]+)"', chunk, re.M)
        bytes_match = re.search(r"^  bytes: (\d+)", chunk, re.M)
        name_match = re.search(r"^    en: (.*)$", chunk, re.M)
        if not all((pid_match, mode_match, bytes_match, name_match)):
            raise ValueError(f"{path}: malformed PID record")
        formula_match = re.search(r'^  formula: "(.*)"$', chunk, re.M)
        unit_match = re.search(r"^  unit: (.*)$", chunk, re.M)
        range_match = re.search(
            r"^  range:\s*\[\s*([^,\]]+)\s*,\s*([^\]]+)\s*\]",
            chunk,
            re.M,
        )
        rows.append(
            Row(
                mode=mode_match.group(1),
                pid=pid_match.group(1),
                byte_count=int(bytes_match.group(1)),
                unit=yaml_scalar(unit_match.group(1)) if unit_match else "",
                name=yaml_scalar(name_match.group(1)),
                formula=formula_match.group(1) if formula_match else "",
                minimum=float(range_match.group(1)) if range_match else None,
                maximum=float(range_match.group(2)) if range_match else None,
            )
        )
    return rows


def c_ascii(text: str) -> str:
    replacements = {
        "\u2018": "'", "\u2019": "'", "\u201c": '"', "\u201d": '"',
        "\u2010": "-", "\u2011": "-", "\u2012": "-", "\u2013": "-",
        "\u2014": "-", "\u2212": "-", "\u00a0": " ",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return text.replace("\\", "\\\\").replace('"', '\\"')


def value_kind(row: Row) -> str:
    if row.formula:
        return (
            "LINK_OBD2_VALUE_MULTI_SCALAR"
            if ";" in row.formula
            else "LINK_OBD2_VALUE_SCALAR"
        )
    return {
        "bitmap": "LINK_OBD2_VALUE_BITMAP",
        "encoded": "LINK_OBD2_VALUE_ENCODED",
        "dtc": "LINK_OBD2_VALUE_DTC",
        "ascii": "LINK_OBD2_VALUE_ASCII",
        "hex": "LINK_OBD2_VALUE_HEX",
    }.get(row.unit.lower(), "LINK_OBD2_VALUE_RAW")


def generate(mode01: Path, mode09: Path, output: Path, source_note: Path) -> None:
    rows = parse(mode01) + parse(mode09)
    counts: dict[str, int] = {}
    seen: set[tuple[str, str]] = set()
    for row in rows:
        counts[row.mode] = counts.get(row.mode, 0) + 1
        key = (row.mode, row.pid)
        if key in seen:
            raise ValueError(f"duplicate mode/PID {key}")
        seen.add(key)
        if row.formula and row.formula not in FORMULA_KINDS:
            raise ValueError(f"unmapped formula {row.mode}/{row.pid}: {row.formula}")
    if counts != EXPECTED or len(rows) != EXPECTED_TOTAL:
        raise ValueError(
            f"expected {EXPECTED} / {EXPECTED_TOTAL}; got {counts} / {len(rows)}"
        )

    rows.sort(key=lambda row: (int(row.mode, 16), int(row.pid, 16)))
    output.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* SPDX-License-Identifier: CC0-1.0 */",
        "/* Generated from pinned OBDex PID data; do not edit by hand. */",
        f"/* OBDex snapshot {OBDEX_COMMIT}; 119 Mode 01 + 13 Mode 09 definitions. */",
        "static const LinkObd2CatalogueEntry link_obd2_catalogue[] = {",
    ]
    for row in rows:
        formula_kind = FORMULA_KINDS.get(row.formula, "LINK_OBD2_FORMULA_NONE")
        has_range = row.minimum is not None and row.maximum is not None
        lines.append(
            "    {{UINT8_C(0x%s), UINT8_C(0x%s), UINT8_C(%d), %s, "
            '"%s", "%s", "%s", %s, %s, %s}, %s},'
            % (
                row.mode,
                row.pid,
                row.byte_count,
                value_kind(row),
                c_ascii(row.name),
                c_ascii(row.unit),
                c_ascii(row.formula),
                "true" if has_range else "false",
                repr(row.minimum if has_range else 0.0),
                repr(row.maximum if has_range else 0.0),
                formula_kind,
            )
        )
    lines += [
        "};",
        f"#define LINK_OBD2_CATALOGUE_EXPECTED_COUNT {EXPECTED_TOTAL}U",
        f'#define LINK_OBD2_CATALOGUE_SNAPSHOT "{OBDEX_COMMIT}"',
        "",
    ]
    output.write_text("\n".join(lines), encoding="utf-8")

    source_note.parent.mkdir(parents=True, exist_ok=True)
    source_note.write_text(
        "\n".join(
            [
                "# OBDex generic PID snapshot",
                "",
                f"Upstream commit: `{OBDEX_COMMIT}`",
                "Data license: `CC0-1.0`",
                "",
                "- Mode 01 definitions: **119**",
                "- Mode 09 definitions: **13**",
                "- Total definitions: **132**",
                "",
                "LINK vendors the independently maintained OBDex data snapshot and "
                "generates transport-neutral metadata from it.",
                "The standards engine preserves bitmap, encoded, DTC, ASCII and other "
                "structured payloads rather than forcing every PID into one scalar.",
                "Formula-backed entries are decoded by LINK's portable C11 core; "
                "manufacturer-specific data remains outside this catalogue.",
                "",
                "SAE J1979 and its Digital Annex are copyrighted publications. "
                "This vendored CC0 snapshot is not represented as a copy of those "
                "publications.",
                "",
            ]
        ),
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("mode01", type=Path)
    parser.add_argument("mode09", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("source_note", type=Path)
    args = parser.parse_args()
    try:
        generate(args.mode01, args.mode09, args.output, args.source_note)
    except Exception as exc:
        print(f"PID import failed: {exc}", file=sys.stderr)
        return 1
    print(f"Generated {EXPECTED_TOTAL} PID definitions from OBDex {OBDEX_COMMIT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
