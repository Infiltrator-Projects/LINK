// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file version.h
 * @brief Authoritative LINK source version for C and direct-source consumers.
 *
 * VERSION remains the release-control file. CMake rejects a build if this
 * public C value ever drifts from VERSION, preventing telemetry provenance
 * from silently reporting an obsolete shared-engine version.
 */
#ifndef LINK_VERSION_H
#define LINK_VERSION_H

#define LINK_VERSION_STRING "0.14.54"

#endif
