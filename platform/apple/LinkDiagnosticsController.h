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
  beginScheduledManufacturerJob:(uint32_t)token;
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
/** Active ELM-selected OBD transport, or a clear unavailable status. */
@property(nonatomic, copy, readonly) NSString *obdProtocolText;
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

/* Shared language and measurement preferences used by Apple product faces. */
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableLanguageTags;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableLanguageNames;
@property(nonatomic, copy, readonly) NSString *selectedLanguageTag;
@property(nonatomic, copy, readonly) NSString *effectiveLanguageTag;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableMeasurementSystemKeys;
@property(nonatomic, copy, readonly) NSArray<NSString *> *availableMeasurementSystemNames;
@property(nonatomic, copy, readonly) NSString *selectedMeasurementSystemKey;

- (instancetype)initWithProductSlug:(NSString *)productSlug
                         flowConfig:(LinkDiagnosticFlowConfig)flowConfig
                     liveStatusText:(NSString *)liveStatusText
            simulatedLiveStatusText:(NSString *)simulatedLiveStatusText
              standardVINStatusText:(NSString *)standardVINStatusText
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (void)start;
/** Start a real session against one exact CoreBluetooth peripheral UUID. */
- (void)startWithPeripheralIdentifier:(NSString *)peripheralIdentifier;
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
/** Register an opaque recurring manufacturer transaction on LINK's single wire queue. */
- (BOOL)registerLiveManufacturerJobWithToken:(uint32_t)token
                        intervalMilliseconds:(uint32_t)intervalMs
                                    priority:(LinkSchedulerPriority)priority;
- (BOOL)setLiveManufacturerJobEnabled:(BOOL)enabled token:(uint32_t)token;
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

/**
 * Format one raw five-character SAE-style DTC for presentation without
 * changing the stored/evidence code. Generic definitions come from LINK's
 * standards-backed catalogue; manufacturer-specific numbers remain clearly
 * classified for the owning product to refine.
 */
- (NSString *)dtcDisplayTextForCode:(NSString *)code;
- (NSString *)localizedTextForKey:(NSString *)key;
- (void)setSelectedLanguageTag:(NSString *)tag;
- (void)setSelectedMeasurementSystemKey:(NSString *)key;
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

/**
 * Shared persistent vehicle-profile/session store for Apple product faces.
 *
 * LINK owns the generic persistence and VIN/adapter association rules. Product
 * faces store their own manufacturer-specific fields in the profile dictionary,
 * while this class provides the common vehicle-selection and adapter-binding
 * behaviour. Legacy keys may be supplied so an existing product migrates
 * without losing its saved vehicles.
 */
@interface LinkVehicleProfileStore : NSObject

- (instancetype)initWithProductNamespace:(NSString *)productNamespace
                         legacyProfileKey:(NSString * _Nullable)legacyProfileKey
                    legacySelectedVINKey:(NSString * _Nullable)legacySelectedVINKey
                 legacyAdapterMappingKey:(NSString * _Nullable)legacyAdapterMappingKey
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, copy, readonly) NSString *productNamespace;
@property(nonatomic, copy, readonly) NSArray<NSDictionary *> *savedProfiles;
@property(nonatomic, copy, readonly, nullable) NSString *selectedVehicleVIN;

- (nullable NSDictionary *)profileForVIN:(NSString *)vin;
- (BOOL)selectOfflineVehicleWithVIN:(NSString *)vin;
- (void)clearSelectedVehicle;
- (nullable NSString *)associatedAdapterIdentifierForVIN:(NSString *)vin;

/**
 * Accept a live VIN as authoritative. This persists the current vehicle and,
 * when LINK has a successfully probed CoreBluetooth peripheral identifier,
 * updates that vehicle's adapter association.
 */
- (void)recordLiveVIN:(NSString *)vin;

- (void)saveProfile:(NSDictionary *)profile forVIN:(NSString *)vin;
- (void)removeProfileForVIN:(NSString *)vin;


/** Merge standard responder/PID capability evidence while preserving product fields. */
- (BOOL)mergeStandardCapabilitiesFromDiagnosticFlow:
    (const LinkDiagnosticFlow *)flow
                                             forVIN:(NSString *)vin;
- (BOOL)mergeStandardCapabilitiesFromFlowEvent:
    (const LinkDiagnosticFlowEvent *)event
                                        forVIN:(NSString *)vin;

@end

/**
 * Extract the cached standard Mode 01 PID set for one exact responder from a
 * product profile. The profile dictionary may contain arbitrary manufacturer
 * fields; LINK only owns the `liveResponders` standard-capability member.
 */
FOUNDATION_EXPORT NSArray<NSNumber *> *LinkVehicleProfileCachedPIDs(
    NSDictionary * _Nullable profile,
    uint32_t responderCANIdentifier,
    BOOL extendedID);

/** Product-neutral cached standard OBD responder capability. */
@interface LinkVehicleProfileStandardResponder : NSObject
@property(nonatomic, readonly) uint32_t responderCANIdentifier;
@property(nonatomic, readonly, getter=isExtendedID) BOOL extendedID;
@property(nonatomic, copy, readonly) NSArray<NSNumber *> *pids;
@end

FOUNDATION_EXPORT NSArray<LinkVehicleProfileStandardResponder *> *
LinkVehicleProfileStandardResponders(NSDictionary * _Nullable profile);

/** Number of valid standard OBD responder records cached in a profile. */
FOUNDATION_EXPORT NSUInteger LinkVehicleProfileStandardResponderCount(
    NSDictionary * _Nullable profile);

/**
 * Shared per-product standard PID selection persistence for Apple faces.
 * LINK owns both the global selection and VIN/controller-bounded choices;
 * product repositories retain only one-time migration rules for obsolete
 * product-specific defaults.
 */
@interface LinkPIDSelectionStore : NSObject
- (instancetype)initWithProductNamespace:(NSString *)productNamespace
                         legacyGlobalKey:(NSString * _Nullable)legacyGlobalKey
                        legacyVehicleKey:(NSString * _Nullable)legacyVehicleKey
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
@property(nonatomic, readonly) BOOL hasGlobalSelection;
@property(nonatomic, copy, readonly) NSArray<NSString *> *globalStableKeys;
- (void)setGlobalStableKeys:(NSArray<NSString *> *)stableKeys;
- (BOOL)hasSelectionForVIN:(NSString *)vin
      controllerIdentifier:(NSString *)controllerIdentifier;
- (NSArray<NSString *> *)stableKeysForVIN:(NSString *)vin
                     controllerIdentifier:(NSString *)controllerIdentifier;
- (void)setStableKeys:(NSArray<NSString *> *)stableKeys
               forVIN:(NSString *)vin
 controllerIdentifier:(NSString *)controllerIdentifier;
@end

NS_ASSUME_NONNULL_END
