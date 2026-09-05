#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -Eeuo pipefail

product_root="${1:?usage: build-native-product-installer.sh PRODUCT_ROOT OUTPUT}"
output="${2:?usage: build-native-product-installer.sh PRODUCT_ROOT OUTPUT}"
template="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/native-product-installer.sh.in"
version="$(tr -d '[:space:]' < "$product_root/VERSION")"

: "${LINK_NATIVE_PRODUCT_NAME:?}"
: "${LINK_NATIVE_PRODUCT_SLUG:?}"
: "${LINK_NATIVE_PACKAGE_NAME:?}"
: "${LINK_NATIVE_CMAKE_ENABLE_OPTION:?}"
: "${LINK_NATIVE_CMAKE_PROFILE_OPTION:?}"
: "${LINK_NATIVE_CMAKE_PACKAGE_VERSION_OPTION:?}"
: "${LINK_NATIVE_LEGACY_CLEANUP_PATHS:=}"

[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || {
    echo "Invalid VERSION: $version" >&2
    exit 1
}
test -f "$template"
grep -qx '__LINK_NATIVE_PAYLOAD_BELOW__' "$template"

temporary="$(mktemp -d)"
cleanup() { rm -rf -- "$temporary"; }
trap cleanup EXIT
source_epoch="$(git -C "$product_root" log -1 --format=%ct 2>/dev/null || printf '0')"
payload="$temporary/source.tar.gz"
generated_header="$temporary/native-installer.sh"

python3 - "$template" "$generated_header" "$version" <<'PY'
from pathlib import Path
import os, sys
source = Path(sys.argv[1]).read_text(encoding='utf-8')
values = {
    '__LINK_NATIVE_PRODUCT_NAME__': os.environ['LINK_NATIVE_PRODUCT_NAME'],
    '__LINK_NATIVE_PRODUCT_SLUG__': os.environ['LINK_NATIVE_PRODUCT_SLUG'],
    '__LINK_NATIVE_PACKAGE_NAME__': os.environ['LINK_NATIVE_PACKAGE_NAME'],
    '__LINK_NATIVE_VERSION__': sys.argv[3],
    '__LINK_NATIVE_CMAKE_ENABLE_OPTION__': os.environ['LINK_NATIVE_CMAKE_ENABLE_OPTION'],
    '__LINK_NATIVE_CMAKE_PROFILE_OPTION__': os.environ['LINK_NATIVE_CMAKE_PROFILE_OPTION'],
    '__LINK_NATIVE_CMAKE_PACKAGE_VERSION_OPTION__': os.environ['LINK_NATIVE_CMAKE_PACKAGE_VERSION_OPTION'],
    '__LINK_NATIVE_LEGACY_CLEANUP_PATHS__': os.environ.get('LINK_NATIVE_LEGACY_CLEANUP_PATHS', ''),
}
for key, value in values.items():
    if key not in source:
        raise SystemExit(f'LINK native installer template missing {key}')
    source = source.replace(key, value)
if '__LINK_NATIVE_' in source.replace('__LINK_NATIVE_PAYLOAD_BELOW__', ''):
    raise SystemExit('Unresolved LINK native installer placeholder')
Path(sys.argv[2]).write_text(source, encoding='utf-8')
PY

tar \
    --directory "$product_root" \
    --sort=name \
    --mtime="@$source_epoch" \
    --owner=0 --group=0 --numeric-owner \
    --pax-option=delete=atime,delete=ctime \
    --exclude-vcs \
    --exclude='./build' --exclude='./build-*' \
    --exclude='*.deb' --exclude='*.ipa' --exclude='*.run' \
    -cf - . | gzip -n -9 > "$payload"

mkdir -p -- "$(dirname "$output")"
cp "$generated_header" "$output"
cat "$payload" >> "$output"
chmod 0755 "$output"
test -x "$output"
"$output" --help >/dev/null
printf 'Created %s\n' "$output"
