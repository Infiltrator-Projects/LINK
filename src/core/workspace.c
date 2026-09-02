// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/workspace.h"
#include "link/i18n.h"

#include <stddef.h>

static LinkWorkspaceSectionDescriptor link_workspace_sections[] = {
    { LINK_WORKSPACE_VEHICLE, "vehicle", "Vehicle",
      "Vehicle identity, connection, networks and module inventory",
      "nav.vehicle", "nav.vehicle.summary" },
    { LINK_WORKSPACE_FAULTS, "errors", "Errors",
      "Standard and manufacturer diagnostic trouble codes",
      "nav.errors", "nav.errors.summary" },
    { LINK_WORKSPACE_TABLE, "table", "Table",
      "Search, select and view live diagnostic parameters",
      "nav.table", "nav.table.summary" },
    { LINK_WORKSPACE_DASHBOARD, "dashboard", "Dashboard",
      "Selected live measurements at a glance",
      "nav.dashboard", "nav.dashboard.summary" },
    { LINK_WORKSPACE_GRAPHS, "graph", "Graph",
      "Time-series views for selected diagnostic parameters",
      "nav.graph", "nav.graph.summary" },
    { LINK_WORKSPACE_TESTS, "tests", "Tests",
      "Readiness, monitor results and supported diagnostic tests",
      "nav.tests", "nav.tests.summary" },
    { LINK_WORKSPACE_SERVICES, "services", "Services",
      "Supported service procedures with explicit safety gating",
      "nav.services", "nav.services.summary" },
    { LINK_WORKSPACE_LOG, "log", "Log",
      "Chronological diagnostic session history and evidence",
      "nav.log", "nav.log.summary" },
    { LINK_WORKSPACE_SETTINGS, "settings", "Settings",
      "Display, adapter, units, logging and application preferences",
      "nav.settings", "nav.settings.summary" }
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
        if (link_workspace_sections[index].section == section)
            return localise(index);
    }
    return NULL;
}
