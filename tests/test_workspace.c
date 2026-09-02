// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/workspace.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    static const char *expected_keys[] = {
        "vehicle", "errors", "table", "dashboard", "graph",
        "tests", "services", "log", "settings"
    };
    size_t index;

    if (link_workspace_section_count() !=
        sizeof(expected_keys) / sizeof(expected_keys[0])) {
        (void)fprintf(stderr, "workspace section count mismatch\n");
        return 1;
    }

    for (index = 0U; index < link_workspace_section_count(); ++index) {
        const LinkWorkspaceSectionDescriptor *descriptor =
            link_workspace_section_at(index);
        if (descriptor == NULL ||
            descriptor->section != (LinkWorkspaceSection)index ||
            descriptor->key == NULL ||
            strcmp(descriptor->key, expected_keys[index]) != 0 ||
            descriptor->title == NULL || descriptor->title[0] == '\0' ||
            descriptor->summary == NULL || descriptor->summary[0] == '\0' ||
            link_workspace_section(descriptor->section) != descriptor) {
            (void)fprintf(stderr, "invalid workspace descriptor at %zu\n", index);
            return 1;
        }
    }

    if (link_workspace_section_at(link_workspace_section_count()) != NULL ||
        link_workspace_section(LINK_WORKSPACE_OBD) != NULL ||
        link_workspace_section(LINK_WORKSPACE_MODULES) != NULL ||
        link_workspace_section(LINK_WORKSPACE_LIVE_DATA) != NULL) {
        (void)fprintf(stderr, "internal source section leaked into navigation\n");
        return 1;
    }

    return 0;
}
