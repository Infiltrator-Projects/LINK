#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Generate LINK's complete generic OBD-II DTC catalogue from pinned OBDex data.

The OBDex data is CC0-1.0. This importer deliberately consumes only the
five-character generic DTC identifier, broad category, and independently
authored English title. Manufacturer-specific definitions never enter LINK's
generic catalogue.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import unicodedata
from pathlib import Path

EXPECTED = {
    "P0xxx_enriched.yaml": ("P0", 3705),
    "P2xxx_enriched.yaml": ("P2", 3495),
    "P3xxx_enriched.yaml": ("P3", 155),
    "B0xxx_enriched.yaml": ("B0", 323),
    "C0xxx_enriched.yaml": ("C0", 626),
    "U0xxx_enriched.yaml": ("U0", 1055),
    "U3xxx_enriched.yaml": ("U3", 174),
}
EXPECTED_TOTAL = 9533
CODE_RE = re.compile(r"^[PBCU][0-3][0-9A-F]{3}$")
OBDEX_COMMIT = "bc58b0eb7273226a1aabae98e956b70b8362bda1"


def yaml_scalar(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] == "'":
        return value[1:-1].replace("''", "'")
    if len(value) >= 2 and value[0] == value[-1] == '"':
        try:
            return json.loads(value)
        except json.JSONDecodeError as exc:
            raise ValueError(f"invalid quoted YAML scalar: {value}") from exc
    return value


def parse_family(path: Path, expected_prefix: str) -> list[tuple[str, str, str]]:
    rows: list[tuple[str, str, str]] = []
    current_code: str | None = None
    current_category: str | None = None
    in_title = False

    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if raw.startswith("- code: "):
            if current_code is not None:
                raise ValueError(f"{path}:{lineno}: previous code {current_code} has no English title")
            current_code = raw[len("- code: "):].strip().upper()
            current_category = None
            in_title = False
            if not CODE_RE.fullmatch(current_code):
                raise ValueError(f"{path}:{lineno}: invalid DTC {current_code}")
            if not current_code.startswith(expected_prefix):
                raise ValueError(f"{path}:{lineno}: {current_code} outside {expected_prefix} family")
            continue

        if current_code is None:
            continue

        if raw.startswith("  category: "):
            current_category = yaml_scalar(raw[len("  category: "):])
            continue

        if raw == "  title:":
            in_title = True
            continue

        if in_title and raw.startswith("    en: "):
            title = yaml_scalar(raw[len("    en: "):])
            if not title:
                raise ValueError(f"{path}:{lineno}: empty English title for {current_code}")
            rows.append((current_code, current_category or "generic", title))
            current_code = None
            current_category = None
            in_title = False
            continue

        if in_title and raw.startswith("  ") and not raw.startswith("    "):
            in_title = False

    if current_code is not None:
        raise ValueError(f"{path}: trailing code {current_code} has no English title")
    return rows


def c_ascii(text: str) -> str:
    replacements = {
        "\u2018": "'", "\u2019": "'", "\u201a": "'", "\u201b": "'",
        "\u201c": '"', "\u201d": '"', "\u201e": '"', "\u201f": '"',
        "\u2010": "-", "\u2011": "-", "\u2012": "-", "\u2013": "-", "\u2014": "-", "\u2212": "-",
        "\u00a0": " ",
    }
    for old, new in replacements.items():
        text = text.replace(old, new)
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode("ascii")
    return text.replace("\\", "\\\\").replace('"', '\\"')


def write_outputs(source_dir: Path, tsv_path: Path, inc_path: Path, source_note: Path) -> None:
    all_rows: list[tuple[str, str, str]] = []
    family_counts: dict[str, int] = {}

    for filename, (prefix, expected_count) in EXPECTED.items():
        path = source_dir / filename
        if not path.is_file():
            raise FileNotFoundError(path)
        rows = parse_family(path, prefix)
        if len(rows) != expected_count:
            raise ValueError(f"{filename}: expected {expected_count} entries, got {len(rows)}")
        family_counts[prefix] = len(rows)
        all_rows.extend(rows)

    by_code: dict[str, tuple[str, str]] = {}
    for code, category, title in all_rows:
        previous = by_code.get(code)
        if previous is not None and previous != (category, title):
            raise ValueError(f"conflicting duplicate DTC {code}: {previous!r} vs {(category, title)!r}")
        if previous is not None:
            raise ValueError(f"duplicate DTC {code}")
        by_code[code] = (category, title)

    if len(by_code) != EXPECTED_TOTAL:
        raise ValueError(f"expected {EXPECTED_TOTAL} unique generic DTCs, got {len(by_code)}")

    ordered = sorted((code, *by_code[code]) for code in by_code)
    tsv_path.parent.mkdir(parents=True, exist_ok=True)
    inc_path.parent.mkdir(parents=True, exist_ok=True)
    source_note.parent.mkdir(parents=True, exist_ok=True)

    tsv_lines = ["# OBDex generic OBD-II DTC snapshot; data CC0-1.0", "# upstream commit: " + OBDEX_COMMIT, "code\tcategory\ttitle"]
    for code, category, title in ordered:
        clean_title = title.replace("\t", " ").replace("\r", " ").replace("\n", " ")
        tsv_lines.append(f"{code}\t{category}\t{clean_title}")
    tsv_path.write_text("\n".join(tsv_lines) + "\n", encoding="utf-8")

    inc_lines = [
        "/* SPDX-License-Identifier: CC0-1.0 */",
        "/* Generated by scripts/import-obdex-dtcs.py; do not edit by hand. */",
        f"/* OBDex snapshot {OBDEX_COMMIT}; {EXPECTED_TOTAL} generic DTC definitions. */",
        "static const LinkDtcCatalogueEntry link_dtc_catalogue[] = {",
    ]
    for code, category, title in ordered:
        inc_lines.append(f'    {{"{code}", "{c_ascii(category)}", "{c_ascii(title)}"}},')
    inc_lines.extend([
        "};",
        f"#define LINK_DTC_CATALOGUE_EXPECTED_COUNT {EXPECTED_TOTAL}U",
        f'#define LINK_DTC_CATALOGUE_SNAPSHOT "{OBDEX_COMMIT}"',
        "",
    ])
    inc_path.write_text("\n".join(inc_lines), encoding="utf-8")

    source_lines = [
        "# OBDex generic DTC snapshot",
        "",
        f"Upstream commit: `{OBDEX_COMMIT}`",
        "Data license: `CC0-1.0`",
        f"Definitions: **{EXPECTED_TOTAL}**",
        "",
        "Family counts:",
        "",
    ]
    for prefix in ("P0", "P2", "P3", "B0", "C0", "U0", "U3"):
        source_lines.append(f"- `{prefix}`: {family_counts[prefix]}")
    source_lines.extend([
        "",
        "LINK imports only the generic DTC identifier, broad category and independently authored English title.",
        "Manufacturer-specific definitions are intentionally excluded and remain the responsibility of product repositories.",
        "SAE J2012 itself is copyrighted; this snapshot is not represented as a copy of the SAE publication.",
        "",
    ])
    source_note.write_text("\n".join(source_lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_dir", type=Path)
    parser.add_argument("tsv", type=Path)
    parser.add_argument("inc", type=Path)
    parser.add_argument("source_note", type=Path)
    args = parser.parse_args()
    try:
        write_outputs(args.source_dir, args.tsv, args.inc, args.source_note)
    except Exception as exc:  # fail closed on malformed/incomplete source data
        print(f"DTC import failed: {exc}", file=sys.stderr)
        return 1
    print(f"Generated {EXPECTED_TOTAL} generic DTC definitions from OBDex {OBDEX_COMMIT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
