// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_whisper.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", \
                #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    const LinkMercedesMeWhisperVocabularyEntry *entry;

    CHECK(strcmp(
        link_mercedes_me_whisper_response_selection_name(
            LINK_MERCEDES_ME_WHISPER_SELECT_LOWEST_CANID_CACHED),
        "SELECT_LOWEST_CANID_CACHED") == 0);
    CHECK(link_mercedes_me_whisper_response_selection_from_name(
              "SELECT_MAXIMUM") ==
          LINK_MERCEDES_ME_WHISPER_SELECT_MAXIMUM);
    CHECK(strcmp(
        link_mercedes_me_whisper_dtc_presentation_name(
            LINK_MERCEDES_ME_WHISPER_SAE_DTC_UDS_DAI),
        "SAEDTC_UDS_DAI") == 0);
    CHECK(link_mercedes_me_whisper_dtc_presentation_from_name(
              "SAEDTC_OBD") ==
          LINK_MERCEDES_ME_WHISPER_SAE_DTC_OBD);
    CHECK(link_mercedes_me_whisper_response_selection_from_name(
              "bogus") ==
          LINK_MERCEDES_ME_WHISPER_RESPONSE_SELECTION_UNKNOWN);

    CHECK(link_mercedes_me_whisper_vocabulary_count() == 12U);
    entry = link_mercedes_me_whisper_vocabulary_find("config.properties");
    CHECK(entry != NULL);
    CHECK(entry->kind == LINK_MERCEDES_ME_WHISPER_VOCAB_RESOURCE);
    entry = link_mercedes_me_whisper_vocabulary_find("formula");
    CHECK(entry != NULL);
    CHECK(entry->kind == LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY);
    entry = link_mercedes_me_whisper_vocabulary_find("alwaysAvailable");
    CHECK(entry != NULL);
    CHECK(entry->kind == LINK_MERCEDES_ME_WHISPER_VOCAB_CONFIGURATION_KEY);
    CHECK(link_mercedes_me_whisper_vocabulary_find("not-recovered") == NULL);
    CHECK(link_mercedes_me_whisper_vocabulary_at(12U) == NULL);
    return 0;
}
