// SPDX-License-Identifier: GPL-3.0-or-later
#import <Foundation/Foundation.h>
#import "link/diagnostic_flow.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSUInteger, LinkApplePollingUpdateDisposition) {
    LinkApplePollingUpdateDispositionNone = 0,
    LinkApplePollingUpdateDispositionRestartLiveFlow,
    LinkApplePollingUpdateDispositionBecameIdle
};

/** Owns retained standard-PID enablement and scheduler reapplication policy. */
@interface LinkApplePollingCoordinator : NSObject
- (BOOL)isEnabledForPID:(uint8_t)pid;
- (uint64_t)fieldMaskForPID:(uint8_t)pid;
- (LinkApplePollingUpdateDisposition)setFieldMask:(uint64_t)fieldMask
                                           forPID:(uint8_t)pid
                                             flow:(LinkDiagnosticFlow *)flow
                                           active:(BOOL)active
                      manufacturerExtensionActive:(BOOL)manufacturerExtensionActive;
- (LinkApplePollingUpdateDisposition)setEnabled:(BOOL)enabled
                                         forPID:(uint8_t)pid
                                           flow:(LinkDiagnosticFlow *)flow
                                         active:(BOOL)active
                    manufacturerExtensionActive:(BOOL)manufacturerExtensionActive;
- (void)applyToFlow:(LinkDiagnosticFlow *)flow;
@end

NS_ASSUME_NONNULL_END
