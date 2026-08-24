// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file ecu_probe.h
 * @brief Portable read-only ECU/module identity probe used by Discover faces.
 *
 * The probe owns generic ELM-managed CAN channel setup, optional TesterPresent,
 * bounded UDS ReadDataByIdentifier acquisition and read-only DTC inventory.
 * Manufacturer repositories provide only endpoint addresses and DID lists.
 */
#ifndef LINK_ECU_PROBE_H
#define LINK_ECU_PROBE_H

#include "link/elm327_can.h"
#include "link/uds_dtc.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_ECU_PROBE_MAX_DIDS 32U
#define LINK_ECU_PROBE_MAX_DID_DATA 128U

typedef enum LinkEcuProbeReadStatus {
    LINK_ECU_PROBE_READ_NOT_ATTEMPTED = 0,
    LINK_ECU_PROBE_READ_AVAILABLE,
    LINK_ECU_PROBE_READ_NO_RESPONSE,
    LINK_ECU_PROBE_READ_NEGATIVE_RESPONSE,
    LINK_ECU_PROBE_READ_INVALID_RESPONSE
} LinkEcuProbeReadStatus;

typedef enum LinkEcuProbeStage {
    LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL = 0,
    LINK_ECU_PROBE_STAGE_TESTER_PRESENT,
    LINK_ECU_PROBE_STAGE_READ_DID,
    LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION,
    LINK_ECU_PROBE_STAGE_COMPLETE,
    LINK_ECU_PROBE_STAGE_FAILED
} LinkEcuProbeStage;

typedef enum LinkEcuProbeResult {
    LINK_ECU_PROBE_RESULT_OK = 0,
    LINK_ECU_PROBE_RESULT_COMPLETE,
    LINK_ECU_PROBE_RESULT_INVALID_ARGUMENT,
    LINK_ECU_PROBE_RESULT_BUFFER_TOO_SMALL,
    LINK_ECU_PROBE_RESULT_CHANNEL_ERROR,
    LINK_ECU_PROBE_RESULT_PDU_ERROR,
    LINK_ECU_PROBE_RESULT_UDS_ERROR,
    LINK_ECU_PROBE_RESULT_BLOCKED_BY_POLICY,
    LINK_ECU_PROBE_RESULT_FAILED_STATE
} LinkEcuProbeResult;

typedef struct LinkEcuProbeDidRequest {
    uint16_t did;
    const char *key;
    const char *name;
} LinkEcuProbeDidRequest;

typedef struct LinkEcuProbeDidResult {
    const LinkEcuProbeDidRequest *request;
    LinkEcuProbeReadStatus status;
    LinkElm327CanResult elm_can_result;
    LinkElm327Result elm_result;
    LinkUdsResult uds_result;
    uint8_t negative_response_code;
    size_t data_length;
    bool truncated;
    uint8_t data[LINK_ECU_PROBE_MAX_DID_DATA];
} LinkEcuProbeDidResult;

typedef struct LinkEcuProbeProfile {
    LinkElm327CanChannelConfig channel;
    const LinkEcuProbeDidRequest *dids;
    size_t did_count;
    bool tester_present;
    bool read_dtcs;
} LinkEcuProbeProfile;

typedef struct LinkEcuProbe {
    LinkEcuProbeProfile profile;
    LinkElm327CanChannelState channel;
    LinkEcuProbeStage stage;
    LinkEcuProbeResult failure;
    LinkElm327CanResult elm_can_failure;
    LinkElm327Result elm_failure;
    LinkUdsResult uds_failure;
    uint8_t uds_negative_response_code;
    size_t did_index;
    LinkEcuProbeDidResult did_results[LINK_ECU_PROBE_MAX_DIDS];
    LinkEcuProbeReadStatus dtc_status;
    LinkElm327CanResult dtc_elm_can_result;
    LinkElm327Result dtc_elm_result;
    LinkUdsResult dtc_uds_result;
    uint8_t dtc_negative_response_code;
    LinkUdsDtcList dtcs;
} LinkEcuProbe;

const char *link_ecu_probe_result_name(LinkEcuProbeResult result);
const char *link_ecu_probe_stage_name(LinkEcuProbeStage stage);
const char *link_ecu_probe_read_status_name(LinkEcuProbeReadStatus status);
bool link_ecu_probe_profile_is_valid(const LinkEcuProbeProfile *profile);
LinkEcuProbeResult link_ecu_probe_begin(
    LinkEcuProbe *probe,
    const LinkEcuProbeProfile *profile);
LinkEcuProbeResult link_ecu_probe_command(
    const LinkEcuProbe *probe,
    char *buffer,
    size_t buffer_size,
    size_t *written);
LinkEcuProbeResult link_ecu_probe_accept(
    LinkEcuProbe *probe,
    const LinkElm327Response *response);
size_t link_ecu_probe_did_result_count(const LinkEcuProbe *probe);
const LinkEcuProbeDidResult *link_ecu_probe_did_result_at(
    const LinkEcuProbe *probe,
    size_t index);

#ifdef __cplusplus
}
#endif

#endif
