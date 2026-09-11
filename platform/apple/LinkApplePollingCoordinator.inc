// SPDX-License-Identifier: GPL-3.0-or-later
/* Internal implementation include; compiled through LinkDiagnosticsController.m. */

@implementation LinkApplePollingCoordinator {
    LinkPollingPolicy _policy;
}

- (instancetype)init
{
    self = [super init];
    if (self == nil) return nil;
    link_polling_policy_init(&_policy, true);
    return self;
}

- (BOOL)isEnabledForPID:(uint8_t)pid
{
    return link_polling_policy_is_enabled(&_policy, pid) ? YES : NO;
}

- (LinkApplePollingUpdateDisposition)setEnabled:(BOOL)enabled
                                         forPID:(uint8_t)pid
                                           flow:(LinkDiagnosticFlow *)flow
                                         active:(BOOL)active
                    manufacturerExtensionActive:(BOOL)manufacturerExtensionActive
{
    link_polling_policy_set_enabled(&_policy, pid, enabled ? true : false);
    if (flow == NULL) return LinkApplePollingUpdateDispositionNone;

    /* Capability may not have created this scheduler entry yet; retention is authoritative. */
    (void)link_scheduler_set_enabled(&flow->scheduler, pid, enabled);

    if (!active || flow->awaiting_response || manufacturerExtensionActive ||
        (flow->stage != LINK_DIAGNOSTIC_FLOW_LIVE &&
         flow->stage != LINK_DIAGNOSTIC_FLOW_READING_LIVE)) {
        return LinkApplePollingUpdateDispositionNone;
    }

    if (enabled) return LinkApplePollingUpdateDispositionRestartLiveFlow;
    return link_scheduler_enabled_standard_count(&flow->scheduler) == 0U
        ? LinkApplePollingUpdateDispositionBecameIdle
        : LinkApplePollingUpdateDispositionNone;
}

- (void)applyToFlow:(LinkDiagnosticFlow *)flow
{
    if (flow == NULL) return;
    (void)link_polling_policy_apply_to_scheduler(&_policy, &flow->scheduler);
}

@end
