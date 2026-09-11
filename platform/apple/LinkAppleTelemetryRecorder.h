// SPDX-License-Identifier: GPL-3.0-or-later
#import <Foundation/Foundation.h>
#import "link/diagnostic_flow.h"
#import "link/mercedes_me_adapter.h"
#import "link/telemetry.h"

NS_ASSUME_NONNULL_BEGIN

/**
 * Owns Apple-side telemetry stores, session evidence metadata, CSV recording
 * and native-adapter evidence framing. Canonical telemetry remains in LINK core.
 */
@interface LinkAppleTelemetryRecorder : NSObject
- (instancetype)initWithProductSlug:(NSString *)productSlug
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (BOOL)prepareForStart;
- (void)finish;
- (void)setAdapterIdentifier:(const char *)identifier;
- (void)setVehicleIdentifier:(const char *)vehicleIdentifier;
- (void)setOBDProtocolText:(NSString *)protocolText;
- (void)recordTransportStateName:(NSString *)stateName
                      statusText:(NSString *)statusText;
- (void)recordNativeTransportBytes:(const uint8_t *)data size:(size_t)size;
- (BOOL)recordTranscriptCommand:(const char *)command
                     resultCode:(uint32_t)resultCode
                     resultName:(const char *)resultName
                   responseText:(const char *)responseText;
/** Returns nil on success, otherwise the exact evidence-recording failure text. */
- (nullable NSString *)recordFlowEvent:(const LinkDiagnosticFlowEvent *)event;

- (BOOL)latestStructuredSampleForPID:(uint8_t)pid
                              sample:(LinkStructuredTelemetrySample *)sample;
- (NSUInteger)recordedSampleCount;
- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit;
- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                     responderCANIdentifier:(uint32_t)responderCANIdentifier
                                  extendedID:(BOOL)extendedID
                                       limit:(NSUInteger)limit;
- (NSArray<NSNumber *> *)observedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                      extendedID:(BOOL)extendedID;
- (BOOL)favouriteForPID:(uint8_t)pid;
- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid;
- (nullable NSData *)csvDataSnapshot;
- (nullable NSString *)csvSnapshot;
@end

NS_ASSUME_NONNULL_END
