// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/workspace.h"
#include "link/i18n.h"

#include <stddef.h>

static LinkWorkspaceSectionDescriptor link_workspace_sections[] = {
    {
        .section = LINK_WORKSPACE_VEHICLE,
        .key = "vehicle",
        .title = "Vehicle",
        .summary = "Vehicle identity, adapter and connection information",
        .title_i18n_key = "nav.vehicle",
        .summary_i18n_key = "nav.vehicle.summary"
    },
    {
        .section = LINK_WORKSPACE_MODULES,
        .key = "modules",
        .title = "Modules",
        .summary = "Discovered control modules and ECU identification",
        .title_i18n_key = "nav.modules",
        .summary_i18n_key = "nav.modules.summary"
    },
    {
        .section = LINK_WORKSPACE_FAULTS,
        .key = "faults",
        .title = "Faults",
        .summary = "Diagnostic trouble codes by control module",
        .title_i18n_key = "nav.faults",
        .summary_i18n_key = "nav.faults.summary"
    },
    {
        .section = LINK_WORKSPACE_LIVE_DATA,
        .key = "live-data",
        .title = "Live Data",
        .summary = "Search, select and favourite live diagnostic parameters",
        .title_i18n_key = "nav.live_data",
        .summary_i18n_key = "nav.live_data.summary"
    },
    {
        .section = LINK_WORKSPACE_TABLE,
        .key = "table",
        .title = "Table",
        .summary = "Dense live values for selected diagnostic parameters",
        .title_i18n_key = "nav.table",
        .summary_i18n_key = "nav.table.summary"
    },
    {
        .section = LINK_WORKSPACE_DASHBOARD,
        .key = "dashboard",
        .title = "Dashboard",
        .summary = "At-a-glance live diagnostic measurements",
        .title_i18n_key = "nav.dashboard",
        .summary_i18n_key = "nav.dashboard.summary"
    },
    {
        .section = LINK_WORKSPACE_GRAPHS,
        .key = "graphs",
        .title = "Graphs",
        .summary = "Time-series views for selected diagnostic parameters",
        .title_i18n_key = "nav.graphs",
        .summary_i18n_key = "nav.graphs.summary"
    },
    {
        .section = LINK_WORKSPACE_LOG,
        .key = "log",
        .title = "Log",
        .summary = "Diagnostic session history and exported telemetry",
        .title_i18n_key = "nav.log",
        .summary_i18n_key = "nav.log.summary"
    },
    {
        .section = LINK_WORKSPACE_SETTINGS,
        .key = "settings",
        .title = "Settings",
        .summary = "Display, adapter, units, logging and application preferences",
        .title_i18n_key = "nav.settings",
        .summary_i18n_key = "nav.settings.summary"
    }
};

static const LinkWorkspaceSectionDescriptor *localise(size_t index)
{
    if (index >= sizeof(link_workspace_sections) / sizeof(link_workspace_sections[0]))
        return NULL;
    link_workspace_sections[index].title =
        link_i18n_tr(link_workspace_sections[index].title_i18n_key);
    link_workspace_sections[index].summary =
        link_i18n_tr(link_workspace_sections[index].summary_i18n_key);
    return &link_workspace_sections[index];
}

size_t link_workspace_section_count(void)
{
    return sizeof(link_workspace_sections) / sizeof(link_workspace_sections[0]);
}

const LinkWorkspaceSectionDescriptor *link_workspace_section_at(size_t index)
{
    return localise(index);
}

const LinkWorkspaceSectionDescriptor *link_workspace_section(
    LinkWorkspaceSection section)
{
    size_t index;

    for (index = 0U; index < link_workspace_section_count(); ++index) {
        if (link_workspace_sections[index].section == section) {
            return localise(index);
        }
    }
    return NULL;
}
