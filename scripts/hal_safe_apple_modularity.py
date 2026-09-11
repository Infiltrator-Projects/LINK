#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path


def require_count(text: str, needle: str, count: int, label: str) -> None:
    actual = text.count(needle)
    if actual != count:
        raise SystemExit(f"{label}: expected {count} occurrence(s), found {actual}")


controller_h = Path("platform/apple/LinkDiagnosticsController.h")
controller_m = Path("platform/apple/LinkDiagnosticsController.m")

# 1) Physically separate the public vehicle-profile API while preserving every
# public symbol and caller import through LinkDiagnosticsController.h.
h = controller_h.read_text()
profile_marker = "/**\n * Shared persistent vehicle-profile/session store for Apple product faces."
require_count(h, profile_marker, 1, "vehicle-profile declaration marker")
profile_start = h.index(profile_marker)
ns_end = "\nNS_ASSUME_NONNULL_END"
require_count(h, ns_end, 1, "NS_ASSUME_NONNULL_END")
profile_end = h.index(ns_end)
if profile_end <= profile_start:
    raise SystemExit("vehicle-profile declarations are not at expected tail")
profile_declarations = h[profile_start:profile_end].rstrip() + "\n"
for required in (
    "@interface LinkVehicleProfileStore",
    "@interface LinkVehicleProfileStandardResponder",
    "LinkVehicleProfileCachedPIDs",
    "LinkVehicleProfileStandardResponders",
):
    if required not in profile_declarations:
        raise SystemExit(f"missing profile declaration {required}")

profile_header = (
    "// SPDX-License-Identifier: GPL-3.0-or-later\n"
    "/**\n"
    " * @file LinkVehicleProfileStore.h\n"
    " * @brief LINK-owned Apple vehicle/profile persistence and standard OBD capability cache.\n"
    " *\n"
    " * Persistence, VIN selection, adapter association and cached responder\n"
    " * capability live here rather than in the live diagnostic controller.\n"
    " */\n"
    "#import <Foundation/Foundation.h>\n"
    "#import \"link/diagnostic_flow.h\"\n\n"
    "NS_ASSUME_NONNULL_BEGIN\n\n"
    + profile_declarations
    + "\nNS_ASSUME_NONNULL_END\n"
)
Path("platform/apple/LinkVehicleProfileStore.h").write_text(profile_header)

h = h[:profile_start].rstrip() + "\n\nNS_ASSUME_NONNULL_END\n"
import_anchor = '#import "LinkBLETransport.h"\n'
require_count(h, import_anchor, 1, "LinkBLETransport import anchor")
h = h.replace(import_anchor, import_anchor + '#import "LinkVehicleProfileStore.h"\n', 1)
controller_h.write_text(h)

# Move the profile implementation tail into an internal include. This keeps the
# existing single Objective-C translation-unit contract so pinned product Xcode
# projects do not need a source-list migration merely to consume this refactor.
m = controller_m.read_text()
profile_impl_marker = "static NSString * const LinkVehicleKnownPeripheralDefaultsKey ="
require_count(m, profile_impl_marker, 1, "vehicle-profile implementation marker")
impl_start = m.index(profile_impl_marker)
profile_impl = m[impl_start:].lstrip()
if "@implementation LinkVehicleProfileStore" not in profile_impl:
    raise SystemExit("vehicle-profile implementation not found in extracted tail")
if not profile_impl.rstrip().endswith("@end"):
    raise SystemExit("vehicle-profile implementation is not the controller tail")
Path("platform/apple/LinkVehicleProfileStore.inc").write_text(
    "// SPDX-License-Identifier: GPL-3.0-or-later\n"
    "/* Internal implementation include; compiled through LinkDiagnosticsController.m. */\n\n"
    + profile_impl
)
m = m[:impl_start].rstrip() + '\n\n#include "LinkVehicleProfileStore.inc"\n'
controller_m.write_text(m)

# 2) Give Apple language/unit persistence one owner. The controller keeps its
# exact public API and notification behaviour, but no longer knows UserDefaults
# keys or stores duplicate preference state.
settings_header = """// SPDX-License-Identifier: GPL-3.0-or-later
#import <Foundation/Foundation.h>
#import "link/units.h"

NS_ASSUME_NONNULL_BEGIN

@interface LinkAppleSettingsStore : NSObject
@property(nonatomic, copy, readonly) NSString *selectedLanguageTag;
@property(nonatomic, copy, readonly) NSString *selectedMeasurementSystemKey;
- (BOOL)setSelectedLanguageTag:(NSString *)tag persist:(BOOL)persist;
- (BOOL)setSelectedMeasurementSystemKey:(NSString *)key persist:(BOOL)persist;
- (LinkMeasurementSystem)resolvedMeasurementSystem;
@end

NS_ASSUME_NONNULL_END
"""
Path("platform/apple/LinkAppleSettings.h").write_text(settings_header)

settings_impl = """// SPDX-License-Identifier: GPL-3.0-or-later
/* Internal implementation include; compiled through LinkDiagnosticsController.m. */

static NSString *const LinkAppleLanguageDefaultsKey = @"link.displayLanguage";
static NSString *const LinkAppleMeasurementDefaultsKey = @"link.measurementSystem";

@interface LinkAppleSettingsStore ()
@property(nonatomic, copy, readwrite, nullable) NSString *storedLanguageTag;
@property(nonatomic, copy, readwrite, nullable) NSString *storedMeasurementSystemKey;
@end

@implementation LinkAppleSettingsStore

- (instancetype)init
{
    self = [super init];
    if (self == nil) return nil;
    NSString *savedLanguage = [[NSUserDefaults standardUserDefaults]
        stringForKey:LinkAppleLanguageDefaultsKey];
    NSString *savedMeasurement = [[NSUserDefaults standardUserDefaults]
        stringForKey:LinkAppleMeasurementDefaultsKey];
    (void)[self setSelectedLanguageTag:
        savedLanguage.length != 0U ? savedLanguage : @"en-AU" persist:NO];
    (void)[self setSelectedMeasurementSystemKey:
        savedMeasurement.length != 0U ? savedMeasurement : @"metric" persist:NO];
    return self;
}

- (NSString *)selectedLanguageTag
{
    return self.storedLanguageTag != nil ? self.storedLanguageTag : @"en-AU";
}

- (NSString *)selectedMeasurementSystemKey
{
    return self.storedMeasurementSystemKey != nil
        ? self.storedMeasurementSystemKey : @"metric";
}

- (BOOL)setSelectedLanguageTag:(NSString *)tag persist:(BOOL)persist
{
    NSString *candidate = tag.length != 0U ? tag : @"en-AU";
    if (!link_i18n_select_locale(candidate.UTF8String)) return NO;
    self.storedLanguageTag = [candidate copy];
    if (persist) {
        [[NSUserDefaults standardUserDefaults]
            setObject:self.storedLanguageTag forKey:LinkAppleLanguageDefaultsKey];
    }
    return YES;
}

- (BOOL)setSelectedMeasurementSystemKey:(NSString *)key persist:(BOOL)persist
{
    LinkMeasurementSystem system;
    NSString *candidate = key.length != 0U ? key : @"metric";
    if (!link_measurement_system_from_key(candidate.UTF8String, &system)) return NO;
    self.storedMeasurementSystemKey =
        [NSString stringWithUTF8String:link_measurement_system_key(system)];
    if (persist) {
        [[NSUserDefaults standardUserDefaults]
            setObject:self.storedMeasurementSystemKey
               forKey:LinkAppleMeasurementDefaultsKey];
    }
    return YES;
}

- (LinkMeasurementSystem)resolvedMeasurementSystem
{
    LinkMeasurementSystem system = LINK_MEASUREMENT_SYSTEM_METRIC;
    (void)link_measurement_system_from_key(
        self.selectedMeasurementSystemKey.UTF8String, &system);
    return system;
}

@end
"""
Path("platform/apple/LinkAppleSettings.inc").write_text(settings_impl)

m = controller_m.read_text()
import_anchor = '#import "LinkDiagnosticsController.h"\n'
require_count(m, import_anchor, 1, "controller import anchor")
m = m.replace(import_anchor, import_anchor + '#import "LinkAppleSettings.h"\n', 1)

old_ivars = "    NSString *_selectedLanguageTag;\n    NSString *_selectedMeasurementSystemKey;\n"
require_count(m, old_ivars, 1, "settings ivars")
m = m.replace(old_ivars, "    LinkAppleSettingsStore *_settings;\n", 1)

old_keys = (
    'static NSString *const LinkAppleLanguageDefaultsKey = @"link.displayLanguage";\n'
    'static NSString *const LinkAppleMeasurementDefaultsKey = @"link.measurementSystem";\n\n'
)
require_count(m, old_keys, 1, "settings defaults keys")
m = m.replace(old_keys, "", 1)

old_private_methods = (
    "- (void)applyLanguagePreference:(NSString *)tag\n"
    "                        persist:(BOOL)persist\n"
    "                         notify:(BOOL)notify;\n"
    "- (void)applyMeasurementPreference:(NSString *)key\n"
    "                           persist:(BOOL)persist\n"
    "                            notify:(BOOL)notify;\n"
)
require_count(m, old_private_methods, 1, "settings private declarations")
m = m.replace(old_private_methods, "", 1)

old_init = """    link_i18n_init();
    NSString *savedLanguage =
        [[NSUserDefaults standardUserDefaults] stringForKey:LinkAppleLanguageDefaultsKey];
    NSString *savedMeasurement =
        [[NSUserDefaults standardUserDefaults] stringForKey:LinkAppleMeasurementDefaultsKey];
    [self applyLanguagePreference:
        savedLanguage.length != 0U ? savedLanguage : @"en-AU"
        persist:NO notify:NO];
    [self applyMeasurementPreference:
        savedMeasurement.length != 0U ? savedMeasurement : @"metric"
        persist:NO notify:NO];
"""
require_count(m, old_init, 1, "settings init block")
m = m.replace(old_init, "    link_i18n_init();\n    _settings = [[LinkAppleSettingsStore alloc] init];\n", 1)

old_settings_methods = """- (NSString *)selectedLanguageTag
{
    return _selectedLanguageTag != nil ? _selectedLanguageTag : @"en-AU";
}

- (NSString *)effectiveLanguageTag
{
    return self.selectedLanguageTag;
}

- (NSArray<NSString *> *)availableMeasurementSystemKeys
{
    return @[@"metric", @"us-customary"];
}

- (NSArray<NSString *> *)availableMeasurementSystemNames
{
    return @[
        [self localizedTextForKey:@"units.metric"],
        [self localizedTextForKey:@"units.us_customary"]
    ];
}

- (NSString *)selectedMeasurementSystemKey
{
    return _selectedMeasurementSystemKey != nil
        ? _selectedMeasurementSystemKey : @"metric";
}
"""
new_settings_methods = """- (NSString *)selectedLanguageTag
{
    return _settings.selectedLanguageTag;
}

- (NSString *)effectiveLanguageTag
{
    return self.selectedLanguageTag;
}

- (NSArray<NSString *> *)availableMeasurementSystemKeys
{
    return @[@"metric", @"us-customary"];
}

- (NSArray<NSString *> *)availableMeasurementSystemNames
{
    return @[
        [self localizedTextForKey:@"units.metric"],
        [self localizedTextForKey:@"units.us_customary"]
    ];
}

- (NSString *)selectedMeasurementSystemKey
{
    return _settings.selectedMeasurementSystemKey;
}
"""
require_count(m, old_settings_methods, 1, "settings getter block")
m = m.replace(old_settings_methods, new_settings_methods, 1)

old_apply = """- (void)applyLanguagePreference:(NSString *)tag
                        persist:(BOOL)persist
                         notify:(BOOL)notify
{
    NSString *candidate = tag.length != 0U ? tag : @"en-AU";
    if (!link_i18n_select_locale(candidate.UTF8String)) return;
    _selectedLanguageTag = [candidate copy];
    if (persist)
        [[NSUserDefaults standardUserDefaults]
            setObject:_selectedLanguageTag forKey:LinkAppleLanguageDefaultsKey];
    if (notify) [self notifyDelegate];
}

- (void)setSelectedLanguageTag:(NSString *)tag
{
    [self applyLanguagePreference:tag persist:YES notify:YES];
}

- (void)applyMeasurementPreference:(NSString *)key
                           persist:(BOOL)persist
                            notify:(BOOL)notify
{
    LinkMeasurementSystem system;
    NSString *candidate = key.length != 0U ? key : @"metric";
    if (!link_measurement_system_from_key(candidate.UTF8String, &system)) return;
    _selectedMeasurementSystemKey =
        [NSString stringWithUTF8String:link_measurement_system_key(system)];
    if (persist)
        [[NSUserDefaults standardUserDefaults]
            setObject:_selectedMeasurementSystemKey
               forKey:LinkAppleMeasurementDefaultsKey];
    if (notify) [self notifyDelegate];
}

- (void)setSelectedMeasurementSystemKey:(NSString *)key
{
    [self applyMeasurementPreference:key persist:YES notify:YES];
}

- (LinkMeasurementSystem)resolvedMeasurementSystem
{
    LinkMeasurementSystem system = LINK_MEASUREMENT_SYSTEM_METRIC;
    (void)link_measurement_system_from_key(
        self.selectedMeasurementSystemKey.UTF8String, &system);
    return system;
}
"""
new_apply = """- (void)setSelectedLanguageTag:(NSString *)tag
{
    if ([_settings setSelectedLanguageTag:tag persist:YES])
        [self notifyDelegate];
}

- (void)setSelectedMeasurementSystemKey:(NSString *)key
{
    if ([_settings setSelectedMeasurementSystemKey:key persist:YES])
        [self notifyDelegate];
}

- (LinkMeasurementSystem)resolvedMeasurementSystem
{
    return [_settings resolvedMeasurementSystem];
}
"""
require_count(m, old_apply, 1, "settings mutation block")
m = m.replace(old_apply, new_apply, 1)

include_anchor = '#include "LinkVehicleProfileStore.inc"\n'
require_count(m, include_anchor, 1, "profile include anchor")
m = m.replace(include_anchor, '#include "LinkAppleSettings.inc"\n' + include_anchor, 1)
controller_m.write_text(m)

# Strengthen Apple characterization around the extracted settings owner.
regression = Path("tests/apple/ProfileRegression.swift")
r = regression.read_text()
marker = """        pollingController.setPollingEnabled(true, forPID: 0x0C)
        precondition(pollingController.pollingEnabled(forPID: 0x0C))

        print("LINK Apple simulation, profile-patch and polling-policy regressions passed")
"""
addition = """        pollingController.setPollingEnabled(true, forPID: 0x0C)
        precondition(pollingController.pollingEnabled(forPID: 0x0C))

        // Apple settings persistence has one owner and preserves controller API.
        pollingController.setSelectedMeasurementSystemKey("us-customary")
        precondition(pollingController.selectedMeasurementSystemKey == "us-customary")
        pollingController.setSelectedLanguageTag("en-US")
        precondition(pollingController.selectedLanguageTag == "en-US")
        let settingsReload = LinkDiagnosticsController(
            productSlug: namespace + "-settings", flowConfig: flow,
            liveStatusText: "live", simulatedLiveStatusText: "simulated",
            standardVINStatusText: "VIN")
        precondition(settingsReload.selectedMeasurementSystemKey == "us-customary")
        precondition(settingsReload.selectedLanguageTag == "en-US")
        settingsReload.setSelectedMeasurementSystemKey("metric")
        settingsReload.setSelectedLanguageTag("en-AU")

        print("LINK Apple simulation, profile-patch, polling-policy and settings regressions passed")
"""
require_count(r, marker, 1, "ProfileRegression insertion marker")
regression.write_text(r.replace(marker, addition, 1))

# Protect the new source ownership boundaries.
runner = Path("tests/apple/run-regressions.sh")
t = runner.read_text()
anchor = "controller=platform/apple/LinkDiagnosticsController.m\n"
require_count(t, anchor, 1, "Apple regression controller anchor")
guard = """controller=platform/apple/LinkDiagnosticsController.m
profile_store=platform/apple/LinkVehicleProfileStore.inc
settings_store=platform/apple/LinkAppleSettings.inc
test -s "$profile_store"
test -s "$settings_store"
grep -Fq '@implementation LinkVehicleProfileStore' "$profile_store"
grep -Fq '@implementation LinkAppleSettingsStore' "$settings_store"
if grep -Fq '@implementation LinkVehicleProfileStore' "$controller"; then
    echo 'Apple diagnostic controller must not own vehicle-profile persistence.' >&2
    exit 1
fi
if grep -Fq 'LinkAppleLanguageDefaultsKey' "$controller"; then
    echo 'Apple diagnostic controller must not own settings persistence keys.' >&2
    exit 1
fi
"""
runner.write_text(t.replace(anchor, guard, 1))

# Record the completed physical boundaries in the ownership document.
docs = Path("docs/APPLE-PROVIDER-BOUNDARY.md")
d = docs.read_text()
doc_anchor = "- `LinkDiagnosticsController`: thin facade and coordinator over the above pieces.\n"
require_count(d, doc_anchor, 1, "ownership documentation anchor")
doc_add = doc_anchor + """
Current Apple source ownership also keeps vehicle/profile persistence in
`LinkVehicleProfileStore.h` / `LinkVehicleProfileStore.inc` and preference
persistence in `LinkAppleSettings.h` / `LinkAppleSettings.inc`. The `.inc`
implementation files are intentionally compiled through the existing controller
translation unit so pinned product Xcode projects do not need a source-list
migration merely to consume this behaviour-preserving refactor.
"""
docs.write_text(d.replace(doc_anchor, doc_add, 1))

# Release metadata is part of the same tested atomic commit.
Path("VERSION").write_text("0.15.18\n")
version_h = Path("include/link/version.h")
vh = version_h.read_text()
old_version = '#define LINK_VERSION_STRING "0.15.17"'
require_count(vh, old_version, 1, "version header baseline")
version_h.write_text(vh.replace(old_version, '#define LINK_VERSION_STRING "0.15.18"', 1))
