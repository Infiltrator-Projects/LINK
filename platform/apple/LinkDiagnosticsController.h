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
#import "link/diagnostic_flow.h"
#import "link/elm327_session.h"
#import "link/elm327_simulator.h"

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
@property(nonatomic, readonly, getter=isActive) BOOL active;
@property(nonatomic, readonly, getter=isReady) BOOL ready;
@property(nonatomic, readonly, getter=isSimulated) BOOL simulated;
@property(nonatomic, readonly, getter=isManufacturerExtensionActive)
    BOOL manufacturerExtensionActive;
@property(nonatomic, readonly) NSUInteger recordedSampleCount;

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

- (BOOL)beginManufacturerCommand:(const char *)command
                         timeout:(uint64_t)timeoutMs;
- (BOOL)completeManufacturerExtensionRestoringAdapter:(BOOL)restore;
- (void)failWithStatus:(NSString *)status;
- (void)updateStatusText:(NSString *)status;
- (void)setVehicleIdentifier:(const char *)vehicleIdentifier;

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit;
- (BOOL)favouriteForPID:(uint8_t)pid;
- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid;
- (nullable NSString *)csvSnapshot;

- (const LinkDiagnosticFlow *)diagnosticFlow;

@end

NS_ASSUME_NONNULL_END
