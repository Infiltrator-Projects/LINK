// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/workspace.h"

#include <stddef.h>

static const LinkWorkspaceSectionDescriptor link_workspace_sections[] = {
    {
        .section = LINK_WORKSPACE_VEHICLE,
        .key = "vehicle",
        .title = "Vehicle",
        .summary = "Vehicle identity, adapter and connection information"
    },
    {
        .section = LINK_WORKSPACE_MODULES,
        .key = "modules",
        .title = "Modules",
        .summary = "Discovered control modules and ECU identification"
    },
    {
        .section = LINK_WORKSPACE_FAULTS,
        .key = "faults",
        .title = "Faults",
        .summary = "Diagnostic trouble codes by control module"
    },
    {
        .section = LINK_WORKSPACE_LIVE_DATA,
        .key = "live-data",
        .title = "Live Data",
        .summary = "Search, select and favourite live diagnostic parameters"
    },
    {
        .section = LINK_WORKSPACE_TABLE,
        .key = "table",
        .title = "Table",
        .summary = "Dense live values for selected diagnostic parameters"
    },
    {
        .section = LINK_WORKSPACE_DASHBOARD,
        .key = "dashboard",
        .title = "Dashboard",
        .summary = "At-a-glance live diagnostic measurements"
    },
    {
        .section = LINK_WORKSPACE_GRAPHS,
        .key = "graphs",
        .title = "Graphs",
        .summary = "Time-series views for selected diagnostic parameters"
    },
    {
        .section = LINK_WORKSPACE_LOG,
        .key = "log",
        .title = "Log",
        .summary = "Diagnostic session history and exported telemetry"
    },
    {
        .section = LINK_WORKSPACE_SETTINGS,
        .key = "settings",
        .title = "Settings",
        .summary = "Display, adapter, units, logging and application preferences"
    }
};

size_t link_workspace_section_count(void)
{
    return sizeof(link_workspace_sections) / sizeof(link_workspace_sections[0]);
}

const LinkWorkspaceSectionDescriptor *link_workspace_section_at(size_t index)
{
    if (index >= link_workspace_section_count()) {
        return NULL;
    }
    return &link_workspace_sections[index];
}

const LinkWorkspaceSectionDescriptor *link_workspace_section(
    LinkWorkspaceSection section)
{
    size_t index;

    for (index = 0U; index < link_workspace_section_count(); ++index) {
        if (link_workspace_sections[index].section == section) {
            return &link_workspace_sections[index];
        }
    }
    return NULL;
}
