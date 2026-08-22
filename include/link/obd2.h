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
    LINK_OBD2_UNIT_LITRES_PER_HOUR
} LinkObd2Unit;

typedef struct {
    uint8_t pid;
    double value;
    LinkObd2Unit unit;
} LinkObd2Sample;

typedef struct {
    uint8_t bits[LINK_OBD2_PID_SET_BYTES];
} LinkObd2PidSet;

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
LinkObd2Result link_obd2_decode_live_pid(
    const LinkElm327Response *response,
    uint8_t pid,
    LinkObd2Sample *sample);
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
LinkObd2Result link_obd2_decode_dtcs(
    const LinkElm327Response *response,
    LinkObd2DtcKind kind,
    LinkObd2DtcList *list);
LinkObd2Result link_obd2_decode_dtc_pair(
    uint8_t high,
    uint8_t low,
    char code[LINK_OBD2_DTC_TEXT_LENGTH]);

#ifdef __cplusplus
}
#endif

#endif
