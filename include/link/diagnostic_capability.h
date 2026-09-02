// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file diagnostic_capability.h
 * @brief Evidence-based diagnostic generation classification.
 *
 * LINK does not infer a diagnostic generation from model year, connector shape
 * or branding. It classifies only surfaces that actually answered. "OBD1.5" is
 * intentionally represented as an informal transitional label rather than a
 * standards-compliance claim.
 */
#ifndef LINK_DIAGNOSTIC_CAPABILITY_H
#define LINK_DIAGNOSTIC_CAPABILITY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkDiagnosticTier {
    LINK_DIAGNOSTIC_TIER_UNKNOWN = 0,
    LINK_DIAGNOSTIC_TIER_LEGACY,
    LINK_DIAGNOSTIC_TIER_TRANSITIONAL,
    LINK_DIAGNOSTIC_TIER_OBD2,
    LINK_DIAGNOSTIC_TIER_NONE
} LinkDiagnosticTier;

typedef struct LinkDiagnosticCapabilityEvidence {
    /** The bounded capability probe has reached a terminal result. */
    bool probe_complete;
    /** At least one standards-shaped OBD service produced a valid response. */
    bool standard_obd_response;
    /**
     * A manufacturer product positively identified a legacy/pre-OBD-II
     * diagnostic exchange. This is not set merely because a modern
     * manufacturer-specific UDS/KWP service answered.
     */
    bool legacy_diagnostic_response;
} LinkDiagnosticCapabilityEvidence;

LinkDiagnosticTier link_diagnostic_capability_classify(
    const LinkDiagnosticCapabilityEvidence *evidence);
const char *link_diagnostic_tier_name(LinkDiagnosticTier tier);
const char *link_diagnostic_tier_summary(LinkDiagnosticTier tier);

#ifdef __cplusplus
}
#endif

#endif
