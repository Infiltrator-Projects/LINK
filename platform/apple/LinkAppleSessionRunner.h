// SPDX-License-Identifier: GPL-3.0-or-later
#import <Foundation/Foundation.h>
#import "LinkBLETransport.h"
#import "link/elm327_session.h"
#import "link/elm327_simulator.h"

NS_ASSUME_NONNULL_BEGIN

@class LinkAppleSessionRunner;

@protocol LinkAppleSessionRunnerDelegate <NSObject>
- (void)linkAppleSessionRunnerDidUpdate:(LinkAppleSessionRunner *)runner;
@end

/**
 * Owns the active Apple ELM327 session, simulation transport and tick timer.
 * The diagnostics controller remains the policy coordinator; this component
 * serializes command execution and publishes session-state changes upward.
 */
@interface LinkAppleSessionRunner : NSObject
@property(nonatomic, weak, nullable) id<LinkAppleSessionRunnerDelegate> delegate;
@property(nonatomic, readonly, getter=isInitialized) BOOL initialized;
@property(nonatomic, readonly, getter=isConnected) BOOL connected;
@property(nonatomic, readonly) LinkElm327SessionStatus status;
@property(nonatomic, readonly) LinkElm327Result elmResult;
@property(nonatomic, readonly) BOOL needsResync;

- (instancetype)initWithProvider:(LinkBLETransport *)provider
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

- (BOOL)startReal;
- (BOOL)startSimulatedWithAdapterIdentifier:(const char *)adapterIdentifier
                                        vin:(const char *)vin
                            customResponder:
                                (LinkElm327SimulatorCustomResponderFn _Nullable)responder
                                    context:(void * _Nullable)context;
/** Disconnect the active session and its underlying transport. */
- (void)disconnect;
/** Drop local session state when the provider has already disconnected. */
- (void)invalidate;

- (LinkElm327SessionOpResult)beginCommand:(const char *)command
                                      now:(uint64_t)nowMs
                                  timeout:(uint64_t)timeoutMs;
- (LinkElm327SessionOpResult)beginResynchronizationAt:(uint64_t)nowMs
                                               timeout:(uint64_t)timeoutMs;
- (const LinkElm327Response * _Nullable)response;
- (const char * _Nullable)currentCommand;
@end

NS_ASSUME_NONNULL_END
