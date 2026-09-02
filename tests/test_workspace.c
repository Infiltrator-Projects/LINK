// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/workspace.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    size_t index;

    if (link_workspace_section_count() != (size_t)LINK_WORKSPACE_SECTION_COUNT) {
        (void)fprintf(stderr, "workspace section count mismatch\n");
        return 1;
    }

    for (index = 0U; index < link_workspace_section_count(); ++index) {
        const LinkWorkspaceSectionDescriptor *descriptor =
            link_workspace_section_at(index);
        if (descriptor == NULL ||
            descriptor->section != (LinkWorkspaceSection)index ||
            descriptor->key == NULL || descriptor->key[0] == '\0' ||
            descriptor->title == NULL || descriptor->title[0] == '\0' ||
            descriptor->summary == NULL || descriptor->summary[0] == '\0' ||
            link_workspace_section(descriptor->section) != descriptor) {
            (void)fprintf(stderr, "invalid workspace descriptor at %zu\n", index);
            return 1;
        }
    }

    if (link_workspace_section_at(link_workspace_section_count()) != NULL ||
        link_workspace_section((LinkWorkspaceSection)LINK_WORKSPACE_SECTION_COUNT) != NULL) {
        (void)fprintf(stderr, "invalid workspace index was accepted\n");
        return 1;
    }

    if (strcmp(link_workspace_section(LINK_WORKSPACE_OBD)->key, "obd") != 0) {
        (void)fprintf(stderr, "OBD stable key changed\n");
        return 1;
    }

    if (strcmp(link_workspace_section(LINK_WORKSPACE_LIVE_DATA)->key,
               "live-data") != 0) {
        (void)fprintf(stderr, "live-data stable key changed\n");
        return 1;
    }

    return 0;
}
