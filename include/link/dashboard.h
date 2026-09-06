// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dashboard.h
 * @brief Shared dashboard presentation policy for every LINK product face.
 *
 * The core owns mode names and gauge-range mathematics. Native platform faces
 * own drawing, while manufacturer products supply only branding and any
 * evidence-backed parameter metadata not already present in LINK.
 */
#ifndef LINK_DASHBOARD_H
#define LINK_DASHBOARD_H

#include "link/parameter.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkDashboardPresentationMode {
    LINK_DASHBOARD_PRESENTATION_NUMBERS = 0,
    LINK_DASHBOARD_PRESENTATION_DIALS,
    LINK_DASHBOARD_PRESENTATION_COMBINED
} LinkDashboardPresentationMode;

typedef struct LinkDashboardGaugeRange {
    double minimum;
    double maximum;
} LinkDashboardGaugeRange;

/** Stable persistence key: numbers, dials or combined. */
const char *link_dashboard_presentation_mode_key(
    LinkDashboardPresentationMode mode);

/** Parses a stable persistence key without accepting aliases. */
bool link_dashboard_presentation_mode_from_key(
    const char *key,
    LinkDashboardPresentationMode *mode);

/** Returns a finite numeric range; otherwise the parameter stays text-only. */
bool link_dashboard_gauge_range_for_parameter(
    const LinkParameterDefinition *definition,
    LinkDashboardGaugeRange *range);

/** Clamps a measured value to a 0..1 position in a validated range. */
bool link_dashboard_gauge_fraction(
    const LinkDashboardGaugeRange *range,
    double value,
    double *fraction);

#ifdef __cplusplus
}
#endif

#endif
