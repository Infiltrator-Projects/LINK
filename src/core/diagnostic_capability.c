// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_capability.h"

#include <stddef.h>

LinkDiagnosticTier link_diagnostic_capability_classify(
    const LinkDiagnosticCapabilityEvidence *evidence)
{
    if (evidence == NULL) return LINK_DIAGNOSTIC_TIER_UNKNOWN;

    if (evidence->standard_obd_response &&
        evidence->legacy_diagnostic_response) {
        return LINK_DIAGNOSTIC_TIER_TRANSITIONAL;
    }
    if (evidence->standard_obd_response) {
        return LINK_DIAGNOSTIC_TIER_OBD2;
    }
    if (evidence->legacy_diagnostic_response) {
        return LINK_DIAGNOSTIC_TIER_LEGACY;
    }
    if (evidence->probe_complete) {
        return LINK_DIAGNOSTIC_TIER_NONE;
    }
    return LINK_DIAGNOSTIC_TIER_UNKNOWN;
}

const char *link_diagnostic_tier_name(LinkDiagnosticTier tier)
{
    switch (tier) {
    case LINK_DIAGNOSTIC_TIER_LEGACY:
        return "Legacy / OBD-I-era";
    case LINK_DIAGNOSTIC_TIER_TRANSITIONAL:
        return "Transitional / OBD1.5";
    case LINK_DIAGNOSTIC_TIER_OBD2:
        return "Standard OBD-II / EOBD";
    case LINK_DIAGNOSTIC_TIER_NONE:
        return "No supported diagnostics detected";
    case LINK_DIAGNOSTIC_TIER_UNKNOWN:
    default:
        return "Unknown / probing";
    }
}

const char *link_diagnostic_tier_summary(LinkDiagnosticTier tier)
{
    switch (tier) {
    case LINK_DIAGNOSTIC_TIER_LEGACY:
        return "Only a positively identified legacy manufacturer diagnostic surface has answered.";
    case LINK_DIAGNOSTIC_TIER_TRANSITIONAL:
        return "Both a standards-shaped OBD surface and a legacy diagnostic surface answered. OBD1.5 is an informal transitional label, not a universal standard.";
    case LINK_DIAGNOSTIC_TIER_OBD2:
        return "A standards-shaped OBD-II/EOBD service surface answered. This is observed capability, not a regulatory-conformance certification.";
    case LINK_DIAGNOSTIC_TIER_NONE:
        return "The bounded capability probe completed without a supported standard or legacy diagnostic response.";
    case LINK_DIAGNOSTIC_TIER_UNKNOWN:
    default:
        return "Capability discovery has not yet produced enough evidence to classify the diagnostic surface.";
    }
}
