// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file workspace.h
 * @brief Product-neutral operator-task workspace model shared by LINK products.
 */
#ifndef LINK_WORKSPACE_H
#define LINK_WORKSPACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * User-facing workspaces are organised by the operator's job, not by the
 * protocol that supplied the data. OBD/UDS/legacy diagnostics are sources
 * underneath these destinations.
 */
typedef enum LinkWorkspaceSection {
    LINK_WORKSPACE_VEHICLE = 0,
    LINK_WORKSPACE_FAULTS,
    LINK_WORKSPACE_TABLE,
    LINK_WORKSPACE_DASHBOARD,
    LINK_WORKSPACE_GRAPHS,
    LINK_WORKSPACE_TESTS,
    LINK_WORKSPACE_SERVICES,
    LINK_WORKSPACE_LOG,
    LINK_WORKSPACE_SETTINGS,
    LINK_WORKSPACE_SECTION_COUNT,

    /* Compatibility-only internal IDs; deliberately absent from navigation. */
    LINK_WORKSPACE_OBD = 100,
    LINK_WORKSPACE_MODULES = 101,
    LINK_WORKSPACE_LIVE_DATA = 102
} LinkWorkspaceSection;

typedef struct LinkWorkspaceSectionDescriptor {
    LinkWorkspaceSection section;
    const char *key;
    const char *title;
    const char *summary;
    const char *title_i18n_key;
    const char *summary_i18n_key;
} LinkWorkspaceSectionDescriptor;

size_t link_workspace_section_count(void);
const LinkWorkspaceSectionDescriptor *link_workspace_section_at(size_t index);
const LinkWorkspaceSectionDescriptor *link_workspace_section(
    LinkWorkspaceSection section);

#ifdef __cplusplus
}
#endif

#endif
