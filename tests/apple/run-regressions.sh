#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
cd "$(dirname "$0")/../.."

ui=platform/apple/LinkDiagnosticUI.swift
ble=platform/apple/LinkBLETransport.m
grep -Fq 'remainingPresentationAttempts: 3' "$ui"
grep -Fq 'unavailable: @escaping () -> Void' "$ui"
grep -Fq 'Unable to open adapter picker · try Connect again' "$ui"
if grep -Fq 'selection(.automatic)' "$ui"; then
    echo 'Apple Connect must not silently switch to automatic scan when the picker cannot be presented.' >&2
    exit 1
fi
grep -Fq 'const BOOL explicitlySelected =' "$ble"
grep -Fq '!explicitlySelected &&' "$ble"
controller=platform/apple/LinkDiagnosticsController.m
profile_store=platform/apple/LinkVehicleProfileStore.inc
settings_store=platform/apple/LinkAppleSettings.inc
polling_coordinator=platform/apple/LinkApplePollingCoordinator.inc
test -s "$profile_store"
test -s "$settings_store"
test -s "$polling_coordinator"
grep -Fq '@implementation LinkVehicleProfileStore' "$profile_store"
grep -Fq '@implementation LinkAppleSettingsStore' "$settings_store"
grep -Fq '@implementation LinkApplePollingCoordinator' "$polling_coordinator"
if grep -Fq '@implementation LinkVehicleProfileStore' "$controller"; then
    echo 'Apple diagnostic controller must not own vehicle-profile persistence.' >&2
    exit 1
fi
if grep -Fq 'LinkAppleLanguageDefaultsKey' "$controller"; then
    echo 'Apple diagnostic controller must not own settings persistence keys.' >&2
    exit 1
fi
grep -Fq 'Connected · polling idle · no PIDs selected' "$controller"
if grep -Fq 'LinkPollingPolicy _pollingPolicy;' "$controller"; then
    echo 'Apple diagnostic controller must not own polling policy state.' >&2
    exit 1
fi
grep -Fq 'LinkPollingPolicy _policy;' "$polling_coordinator"
grep -Fq 'link_polling_policy_apply_to_scheduler(' "$polling_coordinator"
if grep -Fq '_pidPollingEnabled' "$controller"; then
    echo 'Apple controller must not own a second PID polling-policy array.' >&2
    exit 1
fi
grep -Fq 'link_scheduler_enabled_standard_count(&_flow.scheduler)' "$controller"
grep -Fq 'completedStage == LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS' "$controller"
grep -Fq '[self applyPollingPreferencesToScheduler];' "$controller"
grep -Fq 'private var adapterDiscoveryOrder = [String]()' "$ui"
grep -Fq 'adapterDiscoveryOrder.compactMap { adaptersByIdentifier[$0] }' "$ui"
grep -Fq 'adapterDiscoveryOrder.append(identifier)' "$ui"
grep -Fq 'adapterDiscoveryOrder.removeAll()' "$ui"
if grep -Fq 'if $0.rssi != $1.rssi' "$ui"; then
    echo 'Apple picker must not reorder rows by live RSSI.' >&2
    exit 1
fi

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
