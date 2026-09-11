// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file LinkVehicleProfileStore.h
 * @brief LINK-owned Apple vehicle/profile persistence and standard OBD capability cache.
 *
 * Persistence, VIN selection, adapter association and cached responder
 * capability live here rather than in the live diagnostic controller.
 */
#import <Foundation/Foundation.h>
#import "link/diagnostic_flow.h"

NS_ASSUME_NONNULL_BEGIN

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
/** Apply owned fields without discarding another layer's saved evidence. */
- (void)mergeProfileFields:(NSDictionary *)fields forVIN:(NSString *)vin
    NS_SWIFT_NAME(mergeProfileFields(_:forVIN:));
- (void)removeProfileForVIN:(NSString *)vin;


/** Merge standard responder/PID capability evidence while preserving product fields. */
- (BOOL)mergeStandardCapabilitiesFromDiagnosticFlow:
    (const LinkDiagnosticFlow *)flow
                                             forVIN:(NSString *)vin
    NS_SWIFT_NAME(mergeStandardCapabilities(fromDiagnosticFlow:forVIN:));
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
