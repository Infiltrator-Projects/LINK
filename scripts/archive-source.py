#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Archive exact tracked Git objects, including pinned submodules."""
import argparse
import io
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def git(repository, *arguments):
    return subprocess.check_output(["git", "-C", str(repository), *arguments])

def build(output):
    commit = git(ROOT, "rev-parse", "HEAD").decode().strip()
    version = git(ROOT, "show", commit + ":VERSION").decode().strip()
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise ValueError("Invalid source VERSION")
    output = Path(output)
    output.parent.mkdir(parents=True, exist_ok=True)
    names = set()
    with zipfile.ZipFile(output, "w") as destination:
        def append(repository, revision, prefix):
            data = git(repository, "archive", "--format=zip", "--prefix=" + prefix, revision)
            with zipfile.ZipFile(io.BytesIO(data)) as source:
                for entry in source.infolist():
                    if entry.filename in names:
                        if entry.is_dir():
                            continue
                        raise ValueError("Duplicate archive member " + entry.filename)
                    destination.writestr(entry, source.read(entry))
                    names.add(entry.filename)
            for record in git(repository, "ls-tree", "-rz", revision).split(b"\0"):
                if not record:
                    continue
                metadata, relative = record.split(b"\t", 1)
                mode, kind, identity = metadata.split()
                if mode != b"160000":
                    continue
                relative = relative.decode()
                child = repository / relative
                expected = identity.decode()
                if git(child, "rev-parse", "HEAD").decode().strip() != expected:
                    raise ValueError("Submodule checkout does not match gitlink: " + relative)
                append(child, expected, prefix + relative + "/")
        append(ROOT, commit, "LINK-" + version + "/")
    return output

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    print(build(parser.parse_args().output))
