// SPDX-License-Identifier: GPL-3.0-or-later
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
