#!/usr/bin/env python3
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
COMMON_COMMIT = "7070c5812b50821fd7580101cb2289a3184f6b2c"
OLD_VERSION = "0.15.90"
NEW_VERSION = "0.15.91"


def replace_exact(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"expected exactly one match in {path}: {old!r}, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# Advance Common through LINK's single canonical dependency edge.
subprocess.run(["git", "-C", str(ROOT / "src/infiltratr-common"), "fetch", "origin", COMMON_COMMIT], check=True)
subprocess.run(["git", "-C", str(ROOT / "src/infiltratr-common"), "checkout", "--detach", COMMON_COMMIT], check=True)
common_version = (ROOT / "src/infiltratr-common/VERSION").read_text(encoding="utf-8").strip()
if common_version != "1.19.38":
    raise SystemExit(f"unexpected Common VERSION at {COMMON_COMMIT}: {common_version}")

replace_exact(ROOT / "VERSION", OLD_VERSION + "\n", NEW_VERSION + "\n")
replace_exact(
    ROOT / "CMakeLists.txt",
    'set(LINK_COMMON_VERSION "1.19.35")\nset(LINK_COMMON_COMMIT "7cc5de3de0e94ed2cfcff0840bbb5346eb5c9c9f")',
    f'set(LINK_COMMON_VERSION "1.19.38")\nset(LINK_COMMON_COMMIT "{COMMON_COMMIT}")')

changelog = ROOT / "CHANGELOG.md"
text = changelog.read_text(encoding="utf-8")
needle = "# Changelog\n\n"
entry = (
    "## 0.15.91 - 2026-10-03\n\n"
    "- Advance the canonical nested Infiltratr Common dependency from 1.19.35 to 1.19.38, including the latest portable-source, localisation, graphics and CI hardening fixes.\n"
    "- Keep the dependency graph single-rooted: LINK remains the sole owner of the Common pin consumed by MBLINK and the other product faces.\n\n"
)
if not text.startswith(needle):
    raise SystemExit("unexpected CHANGELOG header")
changelog.write_text(needle + entry + text[len(needle):], encoding="utf-8")

# Remove this one-shot scaffolding before the resulting commit is made.
(ROOT / "scripts/hal-maintenance-patch.py").unlink()
(ROOT / ".github/workflows/hal-maintenance.yml").unlink()
