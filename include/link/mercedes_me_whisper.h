// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_MERCEDES_ME_WHISPER_H
#define LINK_MERCEDES_ME_WHISPER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkMercedesMeWhisperResponseSelection {
    LINK_MERCEDES_ME_WHISPER_SELECT_FIRST = 0,
    LINK_MERCEDES_ME_WHISPER_SELECT_LOWEST_CANID_CACHED,
    LINK_MERCEDES_ME_WHISPER_SELECT_MAXIMUM,
    LINK_MERCEDES_ME_WHISPER_MERGE_ELIMINATE_DUPLICATES,
    LINK_MERCEDES_ME_WHISPER_RESPONSE_SELECTION_UNKNOWN
} LinkMercedesMeWhisperResponseSelection;

typedef enum LinkMercedesMeWhisperDtcPresentation {
    LINK_MERCEDES_ME_WHISPER_SAE_DTC_KWP_DAI = 0,
    LINK_MERCEDES_ME_WHISPER_SAE_DTC_KWP_VW,
    LINK_MERCEDES_ME_WHISPER_SAE_DTC_UDS_DAI,
    LINK_MERCEDES_ME_WHISPER_SAE_DTC_UDS_VW,
    LINK_MERCEDES_ME_WHISPER_SAE_DTC_OBD,
    LINK_MERCEDES_ME_WHISPER_DTC_PRESENTATION_UNKNOWN
} LinkMercedesMeWhisperDtcPresentation;

const char *link_mercedes_me_whisper_response_selection_name(
    LinkMercedesMeWhisperResponseSelection selection);
LinkMercedesMeWhisperResponseSelection
link_mercedes_me_whisper_response_selection_from_name(const char *name);
const char *link_mercedes_me_whisper_dtc_presentation_name(
    LinkMercedesMeWhisperDtcPresentation presentation);
LinkMercedesMeWhisperDtcPresentation
link_mercedes_me_whisper_dtc_presentation_from_name(const char *name);

#ifdef __cplusplus
}
#endif
#endif
