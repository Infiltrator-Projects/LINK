// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_whisper.h"

#include <string.h>

static const char *const response_selection_names[] = {
    "SELECT_FIRST",
    "SELECT_LOWEST_CANID_CACHED",
    "SELECT_MAXIMUM",
    "MERGE_ELIMINATE_DUPLICATES"
};

static const char *const dtc_presentation_names[] = {
    "SAEDTC_KWP_DAI",
    "SAEDTC_KWP_VW",
    "SAEDTC_UDS_DAI",
    "SAEDTC_UDS_VW",
    "SAEDTC_OBD"
};

const char *link_mercedes_me_whisper_response_selection_name(
    LinkMercedesMeWhisperResponseSelection selection)
{
    const unsigned int index = (unsigned int)selection;
    if (index >=
        sizeof(response_selection_names) / sizeof(response_selection_names[0]))
        return "UNKNOWN";
    return response_selection_names[index];
}

LinkMercedesMeWhisperResponseSelection
link_mercedes_me_whisper_response_selection_from_name(const char *name)
{
    unsigned int index;
    if (name == NULL)
        return LINK_MERCEDES_ME_WHISPER_RESPONSE_SELECTION_UNKNOWN;
    for (index = 0U;
         index <
             sizeof(response_selection_names) /
                 sizeof(response_selection_names[0]);
         ++index) {
        if (strcmp(name, response_selection_names[index]) == 0)
            return (LinkMercedesMeWhisperResponseSelection)index;
    }
    return LINK_MERCEDES_ME_WHISPER_RESPONSE_SELECTION_UNKNOWN;
}

const char *link_mercedes_me_whisper_dtc_presentation_name(
    LinkMercedesMeWhisperDtcPresentation presentation)
{
    const unsigned int index = (unsigned int)presentation;
    if (index >=
        sizeof(dtc_presentation_names) / sizeof(dtc_presentation_names[0]))
        return "UNKNOWN";
    return dtc_presentation_names[index];
}

LinkMercedesMeWhisperDtcPresentation
link_mercedes_me_whisper_dtc_presentation_from_name(const char *name)
{
    unsigned int index;
    if (name == NULL)
        return LINK_MERCEDES_ME_WHISPER_DTC_PRESENTATION_UNKNOWN;
    for (index = 0U;
         index <
             sizeof(dtc_presentation_names) /
                 sizeof(dtc_presentation_names[0]);
         ++index) {
        if (strcmp(name, dtc_presentation_names[index]) == 0)
            return (LinkMercedesMeWhisperDtcPresentation)index;
    }
    return LINK_MERCEDES_ME_WHISPER_DTC_PRESENTATION_UNKNOWN;
}
