// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_capability.h"

#include <stdio.h>
#include <string.h>

static int expect_tier(
    LinkDiagnosticCapabilityEvidence evidence,
    LinkDiagnosticTier expected)
{
    const LinkDiagnosticTier actual =
        link_diagnostic_capability_classify(&evidence);
    if (actual != expected) {
        (void)fprintf(stderr, "expected tier %d, got %d\n",
                      (int)expected, (int)actual);
        return 1;
    }
    return 0;
}

int main(void)
{
    int failed = 0;

    failed |= expect_tier(
        (LinkDiagnosticCapabilityEvidence){0},
        LINK_DIAGNOSTIC_TIER_UNKNOWN);
    failed |= expect_tier(
        (LinkDiagnosticCapabilityEvidence){
            .probe_complete = true,
            .legacy_diagnostic_response = true
        },
        LINK_DIAGNOSTIC_TIER_LEGACY);
    failed |= expect_tier(
        (LinkDiagnosticCapabilityEvidence){
            .probe_complete = true,
            .standard_obd_response = true,
            .legacy_diagnostic_response = true
        },
        LINK_DIAGNOSTIC_TIER_TRANSITIONAL);
    failed |= expect_tier(
        (LinkDiagnosticCapabilityEvidence){
            .probe_complete = true,
            .standard_obd_response = true
        },
        LINK_DIAGNOSTIC_TIER_OBD2);
    failed |= expect_tier(
        (LinkDiagnosticCapabilityEvidence){ .probe_complete = true },
        LINK_DIAGNOSTIC_TIER_NONE);

    if (strcmp(link_diagnostic_tier_name(LINK_DIAGNOSTIC_TIER_TRANSITIONAL),
               "Transitional / OBD1.5") != 0) {
        (void)fprintf(stderr, "transitional public label changed\n");
        failed = 1;
    }
    if (strstr(
            link_diagnostic_tier_summary(LINK_DIAGNOSTIC_TIER_TRANSITIONAL),
            "informal") == NULL) {
        (void)fprintf(stderr, "OBD1.5 caveat disappeared\n");
        failed = 1;
    }

    return failed ? 1 : 0;
}
