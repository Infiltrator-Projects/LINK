// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_adapter.h"
#include <stddef.h>
static unsigned char ascii_lower(unsigned char value)
{
    if (value >= (unsigned char)'A' && value <= (unsigned char)'Z')
        return (unsigned char)(value - (unsigned char)'A' + (unsigned char)'a');
    return value;
}
static bool prefix_nocase(const char *text, const char *prefix)
{
    size_t i = 0U;
    if (text == NULL || prefix == NULL) return false;
    while (prefix[i] != '\0') {
        if (text[i] == '\0' ||
            ascii_lower((unsigned char)text[i]) !=
            ascii_lower((unsigned char)prefix[i])) return false;
        ++i;
    }
    return true;
}
LinkMercedesMeAdapterFamily link_mercedes_me_adapter_family_from_name(const char *name)
{
    unsigned char selector;
    if (name == NULL) return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
    if (prefix_nocase(name, "VAN-")) return LINK_MERCEDES_ME_ADAPTER_OTHER_APPS;
    if (!prefix_nocase(name, "MB-") || name[3] == '\0')
        return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
    selector = (unsigned char)name[3];
    if (selector == '1' || selector == '8' || selector == '9')
        return LINK_MERCEDES_ME_ADAPTER_BLE;
    if (selector == '2' || selector == '3' ||
        selector == '4' || selector == '6')
        return LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION;
    if (selector == '5' || selector == '7')
        return LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION;
    return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
}
const char *link_mercedes_me_adapter_family_name(LinkMercedesMeAdapterFamily family)
{
    switch (family) {
    case LINK_MERCEDES_ME_ADAPTER_UNKNOWN: return "unknown";
    case LINK_MERCEDES_ME_ADAPTER_BLE: return "ble";
    case LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION: return "first-generation";
    case LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION: return "second-generation";
    case LINK_MERCEDES_ME_ADAPTER_OTHER_APPS: return "other-apps";
    }
    return "unknown";
}
bool link_mercedes_me_adapter_prefers_ble(LinkMercedesMeAdapterFamily family)
{
    return family == LINK_MERCEDES_ME_ADAPTER_BLE;
}
bool link_mercedes_me_adapter_prefers_classic_spp(LinkMercedesMeAdapterFamily family)
{
    return family == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION ||
           family == LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION;
}
