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

static const LinkMercedesMeWhisperVocabularyEntry whisper_vocabulary[] = {
    { "config.properties", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "_configs", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "deviceproviders", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "actionproviders", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "activeconfiguration", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "vinmapping", LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE },
    { "alwaysAvailable", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY },
    { "baudrate", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY },
    { "p2star", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY },
    { "readInterval", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY },
    { "encoding", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY },
    { "formula", LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY }
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

size_t link_mercedes_me_whisper_vocabulary_count(void)
{
    return sizeof(whisper_vocabulary) / sizeof(whisper_vocabulary[0]);
}

const LinkMercedesMeWhisperVocabularyEntry *
link_mercedes_me_whisper_vocabulary_at(size_t index)
{
    return index < link_mercedes_me_whisper_vocabulary_count()
        ? &whisper_vocabulary[index] : NULL;
}

const LinkMercedesMeWhisperVocabularyEntry *
link_mercedes_me_whisper_vocabulary_find(const char *name)
{
    size_t index;
    if (name == NULL) return NULL;
    for (index = 0U; index < link_mercedes_me_whisper_vocabulary_count(); ++index) {
        if (strcmp(name, whisper_vocabulary[index].name) == 0)
            return &whisper_vocabulary[index];
    }
    return NULL;
}
