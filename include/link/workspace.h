// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file workspace.h
 * @brief Product-neutral diagnostic workspace model shared by LINK products.
 */
#ifndef LINK_WORKSPACE_H
#define LINK_WORKSPACE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkWorkspaceSection {
    LINK_WORKSPACE_VEHICLE = 0,
    LINK_WORKSPACE_MODULES,
    LINK_WORKSPACE_FAULTS,
    LINK_WORKSPACE_LIVE_DATA,
    LINK_WORKSPACE_TABLE,
    LINK_WORKSPACE_DASHBOARD,
    LINK_WORKSPACE_GRAPHS,
    LINK_WORKSPACE_LOG,
    LINK_WORKSPACE_SETTINGS,
    LINK_WORKSPACE_SECTION_COUNT
} LinkWorkspaceSection;

typedef struct LinkWorkspaceSectionDescriptor {
    LinkWorkspaceSection section;
    const char *key;
    const char *title;
    const char *summary;
} LinkWorkspaceSectionDescriptor;

size_t link_workspace_section_count(void);
const LinkWorkspaceSectionDescriptor *link_workspace_section_at(size_t index);
const LinkWorkspaceSectionDescriptor *link_workspace_section(
    LinkWorkspaceSection section);

#ifdef __cplusplus
}
#endif

#endif
