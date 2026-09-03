// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file LinkDiagnosticsController.h
 * @brief Shared Apple diagnostic-session coordinator for LINK product faces.
 *
 * The coordinator owns CoreBluetooth transport lifecycle, ELM327 session
 * driving, the standard LINK diagnostic flow, SAE VIN/DTC/live-data handling,
 * telemetry/CSV recording and deterministic simulation. Manufacturer products
 * remain responsible for interpreting VINs and for any manufacturer extension.
 */
#import <Foundation/Foundation.h>

#import "LinkBLETransport.h"
#import "link/diagnostic_capability.h"
#import "link/diagnostic_flow.h"
#import "link/elm327_session.h"
#import "link/elm327_simulator.h"
#import "link/units.h"

NS_ASSUME_NONNULL_BEGIN

@class LinkDiagnosticsController;

@protocol LinkDiagnosticsControllerDelegate <NSObject>
- (void)linkDiagnosticsControllerDidUpdate:(LinkDiagnosticsController *)controller;
@optional
- (void)linkDiagnosticsController:(LinkDiagnosticsController *)controller
              didReceiveFlowEvent:(const LinkDiagnosticFlowEvent *)event;
- (void)linkDiagnosticsControllerBeginManufacturerExtension:
    (LinkDiagnosticsController *)controller;
- (void)linkDiagnosticsController:(LinkDiagnosticsController *)controller
  didReceiveManufacturerResponse:(const LinkElm327Response *)response;
- (void)linkDiagnosticsController:(LinkDiagnosticsController *)controller
 manufacturerExtensionDidFailWithStatus:(NSString *)status;
@end

@interface LinkDiagnosticsController : NSObject

@property(nonatomic, weak, nullable) id<LinkDiagnosticsControllerDelegate> delegate;

@property(nonatomic, copy, readonly) NSString *statusText;
@property(nonatomic, copy, readonly, nullable) NSString *peripheralName;
@property(nonatomic, copy, readonly, nullable) NSString *adapterIdentifier;
@property(nonatomic, copy, readonly) NSString *faultScanStatusText;
@property(nonatomic, copy, readonly) NSArray<NSString *> *storedDTCs;
@property(nonatomic, copy, readonly) NSArray<NSString *> *pendingDTCs;
@property(nonatomic, copy, readonly) NSArray<NSString *> *permanentDTCs;
/** Human-readable standards-backed readiness summary for the active investigation. */
@property(nonatomic, copy, readonly) NSString *readinessStatusText;
/** Named readiness monitor rows. Unsupported monitors are omitted. */
@property(nonatomic, copy, readonly) NSArray<NSString *> *readinessMonitorStatus;
/** Canonical Mode 02 frame-zero context values captured for stored faults. */
@property(nonatomic, copy, readonly) NSArray<NSString *> *freezeFrameContext;
/** Evidence-based diagnostic generation label shared by all product faces. */
@property(nonatomic, copy, readonly) NSString *diagnosticCapabilityText;
/** Caveated explanation for the current capability classification. */
@property(nonatomic, copy, readonly) NSString *diagnosticCapabilityDetailText;
/** Shared responder/PID summaries for the standard OBD workspace. */
@property(nonatomic, copy, readonly) NSString *standardResponderSummary;
@property(nonatomic, copy, readonly) NSString *supportedPIDSummary;
/** Standard Mode 09 VIN, independent of manufacturer profile decoding. */
@property(nonatomic, copy, readonly) NSString *standardVINText;
/** Shared formatted rows for every advertised standard Mode 01 PID. */
@property(nonatomic, copy, readonly) NSArray<NSString *> *standardLiveValueRows;
@property(nonatomic, readonly, getter=isActive) BOOL active;
@property(nonatomic, readonly, getter=isReady) BOOL ready;
@property(nonatomic, readonly, getter=isNativeAdapterConnected)
    BOOL nativeAdapterConnected;
@property(nonatomic, readonly, getter=isSimulated) BOOL simulated;
@property(nonatomic, readonly, getter=isManufacturerExtensionActive)
    BOOL manufacturerExtensionActive;
@property(nonatomic, readonly) NSUInteger recordedSampleCount;

/* Shared Apple presentation preferences owned by LINK. */
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableLanguageTags;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableLanguageNames;
@property(nonatomic, copy, readonly) NSString *selectedLanguageTag;
@property(nonatomic, copy, readonly) NSString *effectiveLanguageTag;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableMeasurementSystemKeys;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableMeasurementSystemNames;
@property(nonatomic, copy, readonly) NSString *selectedMeasurementSystemKey;
@property(nonatomic, readonly) BOOL preferFavouriteSignals;
@property(nonatomic, readonly) BOOL showUnavailableParameters;

- (instancetype)initWithProductSlug:(NSString *)productSlug
                         flowConfig:(LinkDiagnosticFlowConfig)flowConfig
                     liveStatusText:(NSString *)liveStatusText
            simulatedLiveStatusText:(NSString *)simulatedLiveStatusText
              standardVINStatusText:(NSString *)standardVINStatusText
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
- (void)startSimulatedWithAdapterIdentifier:(const char *)adapterIdentifier
                                        vin:(const char *)vin
                            customResponder:
                                (LinkElm327SimulatorCustomResponderFn _Nullable)responder
                                    context:(void * _Nullable)context;
- (void)disconnect;

/**
 * Pause an idle live OBD scheduler and enter a product-owned manufacturer
 * extension.  No manufacturer command is sent by this call; the product can
 * then issue one or more beginManufacturerCommand:timeout: operations and
 * finish with completeManufacturerExtensionRestoringAdapter:.
 */
- (BOOL)beginLiveManufacturerExtension;
- (BOOL)beginManufacturerCommand:(const char *)command
                         timeout:(uint64_t)timeoutMs;
- (BOOL)completeManufacturerExtensionRestoringAdapter:(BOOL)restore;
- (void)failWithStatus:(NSString *)status;
- (void)updateStatusText:(NSString *)status;
- (void)setVehicleIdentifier:(const char *)vehicleIdentifier;
/**
 * Manufacturer products call this only after positively identifying a
 * legacy/pre-OBD-II diagnostic exchange. Modern proprietary UDS/KWP traffic
 * must not set it.
 */
- (void)setLegacyDiagnosticResponseObserved:(BOOL)observed;

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit;
/** Presentation-converted history; canonical telemetry remains unchanged. */
- (NSArray<NSNumber *> *)displayRecentValuesForPID:(uint8_t)pid
                                             limit:(NSUInteger)limit;
- (NSString *)displayUnitForPID:(uint8_t)pid;
- (NSArray<NSNumber *> *)displayRangeForPID:(uint8_t)pid;

- (NSString *)localizedTextForKey:(NSString *)key;
- (void)setSelectedLanguageTag:(NSString *)tag;
- (void)setSelectedMeasurementSystemKey:(NSString *)key;
- (void)setPreferFavouriteSignals:(BOOL)enabled;
- (void)setShowUnavailableParameters:(BOOL)enabled;
/** Recent values returned by one exact 11/29-bit CAN responder. */
- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                     responderCANIdentifier:(uint32_t)responderCANIdentifier
                                  extendedID:(BOOL)extendedID
                                       limit:(NSUInteger)limit;
- (NSArray<NSNumber *> *)observedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                      extendedID:(BOOL)extendedID;
/** Capability-advertised Mode 01 PIDs for one exact CAN responder. */
- (NSArray<NSNumber *> *)supportedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                       extendedID:(BOOL)extendedID;
- (BOOL)favouriteForPID:(uint8_t)pid;
- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid;

/**
 * Runtime polling policy is independent of vehicle capability. A supported PID
 * may remain visible in the catalogue while disabled here, preventing routine
 * requests until the caller enables it again.
 */
- (BOOL)pollingEnabledForPID:(uint8_t)pid;
- (void)setPollingEnabled:(BOOL)enabled forPID:(uint8_t)pid;
/** Immutable byte snapshot for non-blocking export/write paths. */
- (nullable NSData *)csvDataSnapshot;
- (nullable NSString *)csvSnapshot;

- (const LinkDiagnosticFlow *)diagnosticFlow;

@end

NS_ASSUME_NONNULL_END
