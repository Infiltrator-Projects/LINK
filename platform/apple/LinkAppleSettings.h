// SPDX-License-Identifier: GPL-3.0-or-later
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
