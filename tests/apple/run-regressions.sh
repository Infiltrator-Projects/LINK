#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/../.."
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/link-apple-regression.XXXXXX")
sdk=$(xcrun --sdk iphonesimulator --show-sdk-path)
arch=$(uname -m)
target="$arch-apple-ios17.0-simulator"
includes=(-Iinclude -Isrc/infiltratr-common/include -Iplatform/apple)
objects=()
for source in platform/apple/LinkPortableCore.c platform/apple/LinkPortableObd2.c \
    platform/apple/LinkPortableUds.c \
    src/infiltratr-common/src/{core,arithmetic,config,i18n,token,timing,format,quantity}.c; do
    object="$build_dir/$(basename "$source").o"
    xcrun clang -target "$target" -isysroot "$sdk" -std=c11 "${includes[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
for source in platform/apple/LinkDiagnosticsController.m platform/apple/LinkBLETransport.m; do
    object="$build_dir/$(basename "$source").o"
    xcrun clang -target "$target" -isysroot "$sdk" -fobjc-arc "${includes[@]}" -c "$source" -o "$object"
    objects+=("$object")
done
xcrun swiftc -target "$target" -sdk "$sdk" -swift-version 5 -parse-as-library \
    "${includes[@]}" -import-objc-header tests/apple/Regression-Bridging-Header.h \
    platform/apple/LinkDiagnosticUI.swift tests/apple/ProfileRegression.swift \
    "${objects[@]}" -framework Foundation -framework UIKit -framework SwiftUI \
    -framework CoreBluetooth -framework ExternalAccessory -o "$build_dir/regressions"
device=$(xcrun simctl list devices available -j | python3 -c 'import json,sys; print(next(d["udid"] for ds in json.load(sys.stdin)["devices"].values() for d in ds if d["name"].startswith("iPhone")))')
xcrun simctl boot "$device" || true
xcrun simctl bootstatus "$device" -b
xcrun simctl spawn "$device" "$build_dir/regressions"
