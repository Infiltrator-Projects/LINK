// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file obd2.h
 * @brief Portable SAE OBD-II request, parser and decoder API.
 *
 * This layer consumes normalised ELM327 responses and owns standard OBD-II
 * request construction, supported-PID discovery, common live/freeze-frame PID
 * formulas, readiness decoding, VIN extraction and diagnostic trouble codes.
 * It is independent of transports and manufacturer-specific data.
 */
#ifndef LINK_OBD2_H
#define LINK_OBD2_H

#include "link/elm327.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_OBD2_VIN_LENGTH 17U
#define LINK_OBD2_DTC_TEXT_LENGTH 6U
#define LINK_OBD2_MAX_DTCS 64U
#define LINK_OBD2_PID_SET_BYTES 32U

typedef enum {
    LINK_OBD2_RESULT_OK = 0,
    LINK_OBD2_RESULT_INVALID_ARGUMENT,
    LINK_OBD2_RESULT_ELM_ERROR,
    LINK_OBD2_RESULT_MALFORMED_RESPONSE,
    LINK_OBD2_RESULT_UNEXPECTED_RESPONSE,
    LINK_OBD2_RESULT_UNSUPPORTED_PID,
    LINK_OBD2_RESULT_BUFFER_TOO_SMALL,
    LINK_OBD2_RESULT_TOO_MANY_DTCS,
    LINK_OBD2_RESULT_NOT_AUTHORIZED
} LinkObd2Result;

typedef enum {
    LINK_OBD2_UNIT_NONE = 0,
    LINK_OBD2_UNIT_PERCENT,
    LINK_OBD2_UNIT_CELSIUS,
    LINK_OBD2_UNIT_KPA,
    LINK_OBD2_UNIT_RPM,
    LINK_OBD2_UNIT_KMH,
    LINK_OBD2_UNIT_GRAMS_PER_SECOND,
    LINK_OBD2_UNIT_VOLTS,
    LINK_OBD2_UNIT_LITRES_PER_HOUR,
    LINK_OBD2_UNIT_SECONDS,
    LINK_OBD2_UNIT_MINUTES,
    LINK_OBD2_UNIT_KILOMETRES,
    LINK_OBD2_UNIT_COUNT,
    LINK_OBD2_UNIT_RATIO
} LinkObd2Unit;

typedef struct {
    uint8_t pid;
    double value;
    LinkObd2Unit unit;
} LinkObd2Sample;

/**
 * Generic standard-data representation.
 *
 * SAE PID payloads are not all single scalar values. LINK therefore exposes
 * the standards catalogue independently of the legacy one-double-per-PID
 * convenience API. Product faces may enumerate every known standard item,
 * preserve raw structured payloads, and consume decoded scalar fields where a
 * deterministic public formula is available.
 */
typedef enum {
    LINK_OBD2_VALUE_RAW = 0,
    LINK_OBD2_VALUE_SCALAR,
    LINK_OBD2_VALUE_MULTI_SCALAR,
    LINK_OBD2_VALUE_BITMAP,
    LINK_OBD2_VALUE_ENCODED,
    LINK_OBD2_VALUE_DTC,
    LINK_OBD2_VALUE_ASCII,
    LINK_OBD2_VALUE_HEX
} LinkObd2ValueKind;

typedef struct {
    uint8_t mode;
    uint8_t pid;
    uint8_t bytes;
    LinkObd2ValueKind value_kind;
    const char *name;
    const char *unit;
    const char *formula;
    bool has_range;
    double minimum;
    double maximum;
} LinkObd2PidDefinition;

#define LINK_OBD2_MAX_PID_PAYLOAD 64U
#define LINK_OBD2_MAX_DECODED_SIGNALS 16U
#define LINK_OBD2_MAX_DECODED_TEXT 65U

typedef struct {
    const char *label;
    double value;
    const char *unit;
} LinkObd2DecodedSignal;

typedef struct {
    const LinkObd2PidDefinition *definition;
    uint8_t raw[LINK_OBD2_MAX_PID_PAYLOAD];
    size_t raw_length;
    LinkObd2DecodedSignal signals[LINK_OBD2_MAX_DECODED_SIGNALS];
    size_t signal_count;
    bool text_available;
    char text[LINK_OBD2_MAX_DECODED_TEXT];
} LinkObd2DecodedPid;

typedef struct {
    uint8_t mode;
    const char *name;
    bool read_only;
    bool parameterized;
} LinkObd2ServiceDefinition;

#define LINK_OBD2_MAX_RESPONDER_SAMPLES 8U
#define LINK_OBD2_MAX_RESPONDER_PID_SETS 8U

/** One standard OBD-II value attributed to the CAN ECU that returned it. */
typedef struct {
    bool responder_id_available;
    bool extended_id;
    uint32_t responder_id;
    LinkObd2Sample sample;
} LinkObd2ResponderSample;

/** Bounded set of replies to one functional Mode 01 PID request. */
typedef struct {
    LinkObd2ResponderSample samples[LINK_OBD2_MAX_RESPONDER_SAMPLES];
    size_t count;
    bool truncated;
} LinkObd2ResponderSampleList;

/** Full structured Mode 01 decode attributed to the ECU that returned it. */
typedef struct {
    bool responder_id_available;
    bool extended_id;
    uint32_t responder_id;
    LinkObd2DecodedPid decoded;
} LinkObd2ResponderDecodedPid;

typedef struct {
    LinkObd2ResponderDecodedPid entries[LINK_OBD2_MAX_RESPONDER_SAMPLES];
    size_t count;
    bool truncated;
} LinkObd2ResponderDecodedPidList;

typedef struct {
    uint8_t bits[LINK_OBD2_PID_SET_BYTES];
} LinkObd2PidSet;

/** Standard PID capability bitmap attributed to one physical CAN responder. */
typedef struct {
    bool extended_id;
    uint32_t responder_id;
    LinkObd2PidSet supported_pids;
} LinkObd2ResponderPidSet;

/** Bounded set of responder-specific capability bitmaps. */
typedef struct {
    LinkObd2ResponderPidSet entries[LINK_OBD2_MAX_RESPONDER_PID_SETS];
    size_t count;
    bool truncated;
} LinkObd2ResponderPidSetList;

typedef enum {
    LINK_OBD2_DTC_STORED = 0,
    LINK_OBD2_DTC_PENDING,
    LINK_OBD2_DTC_PERMANENT
} LinkObd2DtcKind;

typedef struct {
    LinkObd2DtcKind kind;
    char code[LINK_OBD2_DTC_TEXT_LENGTH];
} LinkObd2Dtc;

typedef struct {
    LinkObd2Dtc entries[LINK_OBD2_MAX_DTCS];
    size_t count;
} LinkObd2DtcList;

typedef struct {
    bool mil_on;
    uint8_t confirmed_dtc_count;
    bool compression_ignition;
    uint8_t continuous_supported;
    uint8_t continuous_incomplete;
    uint8_t noncontinuous_supported;
    uint8_t noncontinuous_incomplete;
    uint8_t raw[4];
} LinkObd2Readiness;

typedef struct {
    bool confirmed;
    bool acknowledge_readiness_reset;
} LinkObd2ClearAuthorization;

#define LINK_OBD2_CLEAR_AUTHORIZATION_INIT \
    { .confirmed = false, .acknowledge_readiness_reset = false }

const char *link_obd2_result_name(LinkObd2Result result);
const char *link_obd2_unit_name(LinkObd2Unit unit);
const char *link_obd2_pid_name(uint8_t pid);

/** Generic SAE service catalogue for modes 01 through 0A. */
size_t link_obd2_service_definition_count(void);
const LinkObd2ServiceDefinition *link_obd2_service_definition_at(size_t index);
const LinkObd2ServiceDefinition *link_obd2_service_definition(uint8_t mode);

/**
 * Pinned generic PID/InfoType catalogue plus LINK's independently curated
 * standards supplement.
 *
 * The OBDex CC0 snapshot contributes 119 Mode 01 + 13 Mode 09 definitions.
 * LINK supplements later assigned classic Mode 01 identifiers without
 * pretending that unknown public layouts are decoded: unverified layouts stay
 * raw-preserving until their byte format is independently corroborated.
 */
size_t link_obd2_pid_definition_count(void);
const LinkObd2PidDefinition *link_obd2_pid_definition_at(size_t index);
const LinkObd2PidDefinition *link_obd2_pid_definition(uint8_t mode, uint8_t pid);
const char *link_obd2_pid_catalogue_snapshot(void);

/**
 * Decode a PID payload without assuming that one PID equals one double.
 * Unknown/encoded/bitmap items remain available in raw[]; formula-backed
 * scalar fields are returned in signals[]; ASCII items also populate text.
 */
LinkObd2Result link_obd2_decode_pid_payload(
    uint8_t mode,
    uint8_t pid,
    const uint8_t *data,
    size_t data_length,
    LinkObd2DecodedPid *decoded);

/**
 * Build one generic read-only two-byte standard OBD request.
 *
 * Modes 01, 05, 06 and 09 are accepted here. Mode 02 carries a freeze-frame
 * number and therefore uses link_obd2_build_freeze_pid_request(). DTC modes
 * use link_obd2_build_dtc_request(). Write/control modes 04 and 08 are
 * deliberately rejected by this read-only API.
 */
LinkObd2Result link_obd2_build_standard_read_request(
    uint8_t mode,
    uint8_t identifier,
    char *buffer,
    size_t buffer_size);

/**
 * Map a logical SAE J1979 parameter identifier to its J1979-2 OBDonUDS DID.
 *
 * Classic PIDs 0x000-0x0FF map to F400-F4FF. The next logical parameter page
 * 0x100-0x1FF maps to F500-F5FF. This deliberately remains separate from the
 * one-byte Mode 01 API so an extended identifier is never emitted as an
 * invalid classic request.
 */
LinkObd2Result link_obd2_obdonuds_pid_to_did(
    uint16_t pid,
    uint16_t *did);
LinkObd2Result link_obd2_obdonuds_did_to_pid(
    uint16_t did,
    uint16_t *pid);
LinkObd2Result link_obd2_build_obdonuds_pid_request(
    uint16_t pid,
    char *buffer,
    size_t buffer_size);

/**
 * Decode a standard 32-bit support page (00/20/40/... style) into a PID set.
 * This helper is service-agnostic so Mode 01 PIDs, Mode 06 monitor IDs and
 * Mode 09 information types can share the same portable bitmap logic.
 */
LinkObd2Result link_obd2_decode_support_bitmap_payload(
    uint8_t base_identifier,
    const uint8_t *data,
    size_t data_length,
    LinkObd2PidSet *set,
    bool *has_more);

LinkObd2Result link_obd2_build_live_pid_request(
    uint8_t pid, char *buffer, size_t buffer_size);
LinkObd2Result link_obd2_build_freeze_pid_request(
    uint8_t pid, uint8_t frame_number, char *buffer, size_t buffer_size);
LinkObd2Result link_obd2_build_supported_pid_request(
    uint8_t base_pid, char *buffer, size_t buffer_size);
LinkObd2Result link_obd2_build_vin_request(char *buffer, size_t buffer_size);
LinkObd2Result link_obd2_build_dtc_request(
    LinkObd2DtcKind kind, char *buffer, size_t buffer_size);
LinkObd2Result link_obd2_build_clear_dtc_request(
    const LinkObd2ClearAuthorization *authorization,
    char *buffer, size_t buffer_size);

void link_obd2_pid_set_clear(LinkObd2PidSet *set);
bool link_obd2_pid_set_contains(const LinkObd2PidSet *set, uint8_t pid);

LinkObd2Result link_obd2_accept_supported_pids(
    const LinkElm327Response *response,
    uint8_t base_pid,
    LinkObd2PidSet *set,
    bool *has_more);

/**
 * Decode one supported-PID block while retaining the responder CAN ID for
 * every headered reply. The union set is always updated; responder_sets may
 * be NULL for callers that only need the standards-level union.
 */
LinkObd2Result link_obd2_accept_supported_pid_responders(
    const LinkElm327Response *response,
    uint8_t base_pid,
    LinkObd2PidSet *set,
    LinkObd2ResponderPidSetList *responder_sets,
    bool *has_more);

const LinkObd2PidSet *link_obd2_responder_pid_set_find(
    const LinkObd2ResponderPidSetList *responder_sets,
    uint32_t responder_id,
    bool extended_id);
LinkObd2Result link_obd2_decode_live_pid(
    const LinkElm327Response *response,
    uint8_t pid,
    LinkObd2Sample *sample);
/**
 * Decode the complete standard payload for a current-data PID.
 *
 * Unlike LinkObd2Sample, this API retains every deterministic scalar field and
 * the raw bytes for bitmap/encoded/unknown layouts. It also accepts ELM327
 * indexed multi-line messages used by larger modern PID payloads.
 */
LinkObd2Result link_obd2_decode_live_pid_payload(
    const LinkElm327Response *response,
    uint8_t pid,
    LinkObd2DecodedPid *decoded);
LinkObd2Result link_obd2_decode_live_pid_payload_responders(
    const LinkElm327Response *response,
    uint8_t pid,
    LinkObd2ResponderDecodedPidList *responders);
/**
 * Decode every matching Mode 01 reply while retaining its 11/29-bit CAN
 * responder identifier. Headerless transports still yield one sample with
 * `responder_id_available == false`.
 */
LinkObd2Result link_obd2_decode_live_pid_responders(
    const LinkElm327Response *response,
    uint8_t pid,
    LinkObd2ResponderSampleList *responders);
LinkObd2Result link_obd2_decode_freeze_pid(
    const LinkElm327Response *response,
    uint8_t pid,
    uint8_t frame_number,
    LinkObd2Sample *sample);
LinkObd2Result link_obd2_decode_readiness(
    const LinkElm327Response *response,
    LinkObd2Readiness *readiness);
LinkObd2Result link_obd2_decode_vin(
    const LinkElm327Response *response,
    char vin[LINK_OBD2_VIN_LENGTH + 1U]);
LinkObd2Result link_obd2_decode_vin_pdu(
    const uint8_t *pdu,
    size_t pdu_length,
    char vin[LINK_OBD2_VIN_LENGTH + 1U]);
LinkObd2Result link_obd2_decode_dtcs(
    const LinkElm327Response *response,
    LinkObd2DtcKind kind,
    LinkObd2DtcList *list);
/**
 * Recognise an ISO-style negative response for one requested OBD service.
 *
 * Some emissions ECUs answer optional modes such as permanent-DTC Mode 0A
 * with `7F <service> <NRC>` instead of ELM `NO DATA`.  This helper accepts
 * only a response made entirely of matching three-byte negative-response
 * records, so callers can distinguish an unavailable optional inventory from
 * malformed or unrelated traffic without hiding protocol errors.
 */
bool link_obd2_is_negative_response(
    const LinkElm327Response *response,
    uint8_t request_service,
    uint8_t *negative_response_code);
LinkObd2Result link_obd2_decode_dtc_pair(
    uint8_t high,
    uint8_t low,
    char code[LINK_OBD2_DTC_TEXT_LENGTH]);

#ifdef __cplusplus
}
#endif

#endif
