// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_UNITS_H
#define LINK_UNITS_H

#include "link/obd2.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkMeasurementSystem {
    LINK_MEASUREMENT_SYSTEM_DEFAULT = 0,
    LINK_MEASUREMENT_SYSTEM_METRIC,
    LINK_MEASUREMENT_SYSTEM_US_CUSTOMARY
} LinkMeasurementSystem;

const char *link_measurement_system_key(LinkMeasurementSystem system);
bool link_measurement_system_from_key(const char *key, LinkMeasurementSystem *system);
LinkMeasurementSystem link_measurement_system_resolve(
    LinkMeasurementSystem requested, bool host_uses_metric);

bool link_units_convert_obd2(
    LinkObd2Unit unit,
    double canonical_value,
    LinkMeasurementSystem system,
    double *display_value,
    const char **display_unit);

#ifdef __cplusplus
}
#endif
#endif
