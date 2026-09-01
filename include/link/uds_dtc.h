// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_dtc.h
 * @brief Read-only ISO 14229 ReadDTCInformation (0x19) helpers.
 *
 * The catalogue covers the 27 report types requested by the 2013-era
 * ReadDTCInformation surface: 0x01..0x19 plus WWH-OBD 0x42 and 0x55.
 * Report types withdrawn by ISO 14229-1:2020 remain represented for
 * compatibility with ECUs that implement the older standard.
 *
 * Request construction is complete for all 27 report types. Response decoding
 * validates every fixed standard envelope and exposes implementation-defined
 * snapshot/extended-data tails as raw records rather than inventing DID or
 * manufacturer-specific record sizes.
 */
#ifndef LINK_UDS_DTC_H
#define LINK_UDS_DTC_H

#include "link/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_SERVICE_READ_DTC_INFORMATION 0x19U

#define LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK 0x01U
#define LINK_UDS_DTC_REPORT_BY_STATUS_MASK 0x02U
#define LINK_UDS_DTC_REPORT_SNAPSHOT_IDENTIFICATION 0x03U
#define LINK_UDS_DTC_REPORT_SNAPSHOT_BY_DTC_NUMBER 0x04U
#define LINK_UDS_DTC_REPORT_STORED_DATA_BY_RECORD_NUMBER 0x05U
#define LINK_UDS_DTC_REPORT_EXT_DATA_BY_DTC_NUMBER 0x06U
#define LINK_UDS_DTC_REPORT_NUMBER_BY_SEVERITY_MASK_RECORD 0x07U
#define LINK_UDS_DTC_REPORT_BY_SEVERITY_MASK_RECORD 0x08U
#define LINK_UDS_DTC_REPORT_SEVERITY_INFORMATION_OF_DTC 0x09U
#define LINK_UDS_DTC_REPORT_SUPPORTED_DTC 0x0aU
#define LINK_UDS_DTC_REPORT_FIRST_TEST_FAILED_DTC 0x0bU
#define LINK_UDS_DTC_REPORT_FIRST_CONFIRMED_DTC 0x0cU
#define LINK_UDS_DTC_REPORT_MOST_RECENT_TEST_FAILED_DTC 0x0dU
#define LINK_UDS_DTC_REPORT_MOST_RECENT_CONFIRMED_DTC 0x0eU
#define LINK_UDS_DTC_REPORT_MIRROR_MEMORY_BY_STATUS_MASK 0x0fU
#define LINK_UDS_DTC_REPORT_MIRROR_MEMORY_EXT_DATA_BY_DTC_NUMBER 0x10U
#define LINK_UDS_DTC_REPORT_NUMBER_OF_MIRROR_MEMORY_BY_STATUS_MASK 0x11U
#define LINK_UDS_DTC_REPORT_NUMBER_OF_EMISSIONS_OBD_BY_STATUS_MASK 0x12U
#define LINK_UDS_DTC_REPORT_EMISSIONS_OBD_BY_STATUS_MASK 0x13U
#define LINK_UDS_DTC_REPORT_FAULT_DETECTION_COUNTER 0x14U
#define LINK_UDS_DTC_REPORT_WITH_PERMANENT_STATUS 0x15U
#define LINK_UDS_DTC_REPORT_EXT_DATA_BY_RECORD_NUMBER 0x16U
#define LINK_UDS_DTC_REPORT_USER_MEMORY_BY_STATUS_MASK 0x17U
#define LINK_UDS_DTC_REPORT_USER_MEMORY_SNAPSHOT_BY_DTC_NUMBER 0x18U
#define LINK_UDS_DTC_REPORT_USER_MEMORY_EXT_DATA_BY_DTC_NUMBER 0x19U
#define LINK_UDS_DTC_REPORT_WWH_OBD_BY_MASK_RECORD 0x42U
#define LINK_UDS_DTC_REPORT_WWH_OBD_WITH_PERMANENT_STATUS 0x55U

#define LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT 27U
#define LINK_UDS_DTC_STATUS_MASK_ALL 0xffU
#define LINK_UDS_DTC_MAX_RECORDS 64U

#define LINK_UDS_DTC_STATUS_TEST_FAILED 0x01U
#define LINK_UDS_DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE 0x02U
#define LINK_UDS_DTC_STATUS_PENDING_DTC 0x04U
#define LINK_UDS_DTC_STATUS_CONFIRMED_DTC 0x08U
#define LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR 0x10U
#define LINK_UDS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR 0x20U
#define LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE 0x40U
#define LINK_UDS_DTC_STATUS_WARNING_INDICATOR_REQUESTED 0x80U

typedef struct {
    uint32_t code;
    uint8_t status;
} LinkUdsDtcRecord;

typedef struct {
    uint8_t availability_mask;
    size_t count;
    bool truncated;
    LinkUdsDtcRecord records[LINK_UDS_DTC_MAX_RECORDS];
} LinkUdsDtcList;

typedef enum {
    LINK_UDS_DTC_REQUEST_NONE = 0,
    LINK_UDS_DTC_REQUEST_STATUS_MASK,
    LINK_UDS_DTC_REQUEST_DTC_MASK,
    LINK_UDS_DTC_REQUEST_DTC_AND_RECORD,
    LINK_UDS_DTC_REQUEST_RECORD_NUMBER,
    LINK_UDS_DTC_REQUEST_SEVERITY_AND_STATUS_MASK,
    LINK_UDS_DTC_REQUEST_STATUS_MASK_AND_MEMORY,
    LINK_UDS_DTC_REQUEST_DTC_RECORD_AND_MEMORY,
    LINK_UDS_DTC_REQUEST_WWH_MASK_RECORD,
    LINK_UDS_DTC_REQUEST_FUNCTIONAL_GROUP
} LinkUdsDtcRequestShape;

typedef struct {
    uint8_t subfunction;
    const char *name;
    LinkUdsDtcRequestShape request_shape;
    bool withdrawn_in_2020;
} LinkUdsDtcReportDefinition;

typedef struct {
    uint8_t subfunction;
    bool suppress_positive_response;
    uint8_t status_mask;
    uint8_t severity_mask;
    uint32_t dtc;
    uint8_t record_number;
    uint8_t memory_selection;
    uint8_t functional_group_identifier;
} LinkUdsDtcInformationRequest;

#define LINK_UDS_DTC_INFORMATION_REQUEST_INIT \
    { .subfunction = 0U, .suppress_positive_response = false, \
      .status_mask = 0U, .severity_mask = 0U, .dtc = 0U, \
      .record_number = 0U, .memory_selection = 0U, \
      .functional_group_identifier = 0U }

typedef enum {
    LINK_UDS_DTC_RECORDS_RAW = 0,
    LINK_UDS_DTC_RECORDS_DTC_STATUS,
    LINK_UDS_DTC_RECORDS_SNAPSHOT_IDENTIFICATION,
    LINK_UDS_DTC_RECORDS_DTC_SEVERITY,
    LINK_UDS_DTC_RECORDS_FAULT_DETECTION_COUNTER,
    LINK_UDS_DTC_RECORDS_WWH_SEVERITY
} LinkUdsDtcRecordFormat;

typedef struct {
    uint8_t subfunction;

    bool status_availability_mask_available;
    uint8_t status_availability_mask;
    bool severity_availability_mask_available;
    uint8_t severity_availability_mask;
    bool dtc_format_identifier_available;
    uint8_t dtc_format_identifier;
    bool dtc_count_available;
    uint16_t dtc_count;

    bool memory_selection_available;
    uint8_t memory_selection;
    bool functional_group_identifier_available;
    uint8_t functional_group_identifier;
    bool record_number_available;
    uint8_t record_number;

    LinkUdsDtcRecordFormat record_format;
    const uint8_t *records;
    size_t records_length;
} LinkUdsDtcInformationResponse;

static inline size_t link_uds_dtc_report_definition_count(void)
{
    return LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT;
}

static inline const LinkUdsDtcReportDefinition *
link_uds_dtc_report_definition_at(size_t index)
{
    static const LinkUdsDtcReportDefinition definitions[
        LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT] = {
        {0x01U, "reportNumberOfDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, false},
        {0x02U, "reportDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, false},
        {0x03U, "reportDTCSnapshotIdentification",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x04U, "reportDTCSnapshotRecordByDTCNumber",
            LINK_UDS_DTC_REQUEST_DTC_AND_RECORD, false},
        {0x05U, "reportDTCStoredDataByRecordNumber",
            LINK_UDS_DTC_REQUEST_RECORD_NUMBER, false},
        {0x06U, "reportDTCExtDataRecordByDTCNumber",
            LINK_UDS_DTC_REQUEST_DTC_AND_RECORD, false},
        {0x07U, "reportNumberOfDTCBySeverityMaskRecord",
            LINK_UDS_DTC_REQUEST_SEVERITY_AND_STATUS_MASK, false},
        {0x08U, "reportDTCBySeverityMaskRecord",
            LINK_UDS_DTC_REQUEST_SEVERITY_AND_STATUS_MASK, false},
        {0x09U, "reportSeverityInformationOfDTC",
            LINK_UDS_DTC_REQUEST_DTC_MASK, false},
        {0x0aU, "reportSupportedDTC",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x0bU, "reportFirstTestFailedDTC",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x0cU, "reportFirstConfirmedDTC",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x0dU, "reportMostRecentTestFailedDTC",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x0eU, "reportMostRecentConfirmedDTC",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x0fU, "reportMirrorMemoryDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, true},
        {0x10U, "reportMirrorMemoryDTCExtDataRecordByDTCNumber",
            LINK_UDS_DTC_REQUEST_DTC_AND_RECORD, true},
        {0x11U, "reportNumberOfMirrorMemoryDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, true},
        {0x12U, "reportNumberOfEmissionsOBDDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, true},
        {0x13U, "reportEmissionsOBDDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK, true},
        {0x14U, "reportDTCFaultDetectionCounter",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x15U, "reportDTCWithPermanentStatus",
            LINK_UDS_DTC_REQUEST_NONE, false},
        {0x16U, "reportDTCExtDataRecordByRecordNumber",
            LINK_UDS_DTC_REQUEST_RECORD_NUMBER, false},
        {0x17U, "reportUserDefMemoryDTCByStatusMask",
            LINK_UDS_DTC_REQUEST_STATUS_MASK_AND_MEMORY, false},
        {0x18U, "reportUserDefMemoryDTCSnapshotRecordByDTCNumber",
            LINK_UDS_DTC_REQUEST_DTC_RECORD_AND_MEMORY, false},
        {0x19U, "reportUserDefMemoryDTCExtDataRecordByDTCNumber",
            LINK_UDS_DTC_REQUEST_DTC_RECORD_AND_MEMORY, false},
        {0x42U, "reportWWHOBDDTCByMaskRecord",
            LINK_UDS_DTC_REQUEST_WWH_MASK_RECORD, false},
        {0x55U, "reportWWHOBDDTCWithPermanentStatus",
            LINK_UDS_DTC_REQUEST_FUNCTIONAL_GROUP, false}
    };
    return index < LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT
        ? &definitions[index] : NULL;
}

static inline const LinkUdsDtcReportDefinition *
link_uds_dtc_report_definition(uint8_t subfunction)
{
    size_t index;
    for (index = 0U; index < link_uds_dtc_report_definition_count(); ++index) {
        const LinkUdsDtcReportDefinition *definition =
            link_uds_dtc_report_definition_at(index);
        if (definition != NULL && definition->subfunction == subfunction)
            return definition;
    }
    return NULL;
}

static inline LinkUdsResult link_uds_dtc_write_failure(
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written,
    LinkUdsResult result)
{
    if (written != NULL) *written = 0U;
    if (buffer != NULL && buffer_size != 0U) buffer[0] = 0U;
    return result;
}

static inline void link_uds_dtc_write_dtc(
    uint8_t *buffer,
    size_t offset,
    uint32_t dtc)
{
    buffer[offset] = (uint8_t)(dtc >> 16U);
    buffer[offset + 1U] = (uint8_t)(dtc >> 8U);
    buffer[offset + 2U] = (uint8_t)dtc;
}

/**
 * Build any of the 27 supported ReadDTCInformation requests.
 *
 * For 0x42 the 2013 request byte order is FunctionalGroupIdentifier,
 * DTCStatusMask, DTCSeverityMask.
 */
static inline LinkUdsResult link_uds_build_read_dtc_information_request(
    const LinkUdsDtcInformationRequest *request,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    const LinkUdsDtcReportDefinition *definition;
    uint8_t encoded[7U] = {0U};
    size_t required = 2U;

    if (request == NULL || buffer == NULL || written == NULL) {
        return link_uds_dtc_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    definition = link_uds_dtc_report_definition(request->subfunction);
    if (definition == NULL || request->dtc > UINT32_C(0x00ffffff)) {
        return link_uds_dtc_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    encoded[0] = LINK_UDS_SERVICE_READ_DTC_INFORMATION;
    encoded[1] = request->subfunction;
    if (request->suppress_positive_response) encoded[1] |= UINT8_C(0x80);

    switch (definition->request_shape) {
    case LINK_UDS_DTC_REQUEST_NONE:
        break;
    case LINK_UDS_DTC_REQUEST_STATUS_MASK:
        required = 3U;
        encoded[2] = request->status_mask;
        break;
    case LINK_UDS_DTC_REQUEST_DTC_MASK:
        required = 5U;
        link_uds_dtc_write_dtc(encoded, 2U, request->dtc);
        break;
    case LINK_UDS_DTC_REQUEST_DTC_AND_RECORD:
        required = 6U;
        link_uds_dtc_write_dtc(encoded, 2U, request->dtc);
        encoded[5] = request->record_number;
        break;
    case LINK_UDS_DTC_REQUEST_RECORD_NUMBER:
        required = 3U;
        encoded[2] = request->record_number;
        break;
    case LINK_UDS_DTC_REQUEST_SEVERITY_AND_STATUS_MASK:
        required = 4U;
        encoded[2] = request->severity_mask;
        encoded[3] = request->status_mask;
        break;
    case LINK_UDS_DTC_REQUEST_STATUS_MASK_AND_MEMORY:
        required = 4U;
        encoded[2] = request->status_mask;
        encoded[3] = request->memory_selection;
        break;
    case LINK_UDS_DTC_REQUEST_DTC_RECORD_AND_MEMORY:
        required = 7U;
        link_uds_dtc_write_dtc(encoded, 2U, request->dtc);
        encoded[5] = request->record_number;
        encoded[6] = request->memory_selection;
        break;
    case LINK_UDS_DTC_REQUEST_WWH_MASK_RECORD:
        required = 5U;
        encoded[2] = request->functional_group_identifier;
        encoded[3] = request->status_mask;
        encoded[4] = request->severity_mask;
        break;
    case LINK_UDS_DTC_REQUEST_FUNCTIONAL_GROUP:
        required = 3U;
        encoded[2] = request->functional_group_identifier;
        break;
    }

    if (buffer_size < required) {
        return link_uds_dtc_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    }
    memcpy(buffer, encoded, required);
    *written = required;
    return LINK_UDS_RESULT_OK;
}

static inline bool link_uds_dtc_response_records_valid(
    const LinkUdsDtcInformationResponse *response,
    size_t record_size)
{
    return response != NULL && record_size != 0U &&
           (response->records_length % record_size) == 0U;
}

/**
 * Decode the fixed standard envelope of any supported 0x19 response.
 *
 * Snapshot and extended-data payload lengths can depend on DID/OEM definitions.
 * Those variable tails are deliberately returned in records/records_length
 * with LINK_UDS_DTC_RECORDS_RAW instead of being guessed.
 */
static inline LinkUdsResult link_uds_decode_read_dtc_information_response(
    uint8_t expected_subfunction,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsDtcInformationResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsDtcInformationResponse decoded;
    LinkUdsResult result;
    const LinkUdsDtcReportDefinition *definition =
        link_uds_dtc_report_definition(expected_subfunction);

    if (definition == NULL || response == NULL)
        return LINK_UDS_RESULT_INVALID_ARGUMENT;

    result = link_uds_decode_response(
        LINK_UDS_SERVICE_READ_DTC_INFORMATION, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length < 1U ||
        (generic.data[0] & UINT8_C(0x7f)) != expected_subfunction)
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;

    memset(&decoded, 0, sizeof(decoded));
    decoded.subfunction = expected_subfunction;
    decoded.record_format = LINK_UDS_DTC_RECORDS_RAW;
    decoded.records = generic.data + 1U;
    decoded.records_length = generic.data_length - 1U;

    switch (expected_subfunction) {
    case LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_NUMBER_BY_SEVERITY_MASK_RECORD:
    case LINK_UDS_DTC_REPORT_NUMBER_OF_MIRROR_MEMORY_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_NUMBER_OF_EMISSIONS_OBD_BY_STATUS_MASK:
        if (generic.data_length != 5U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[1];
        decoded.dtc_format_identifier_available = true;
        decoded.dtc_format_identifier = generic.data[2];
        decoded.dtc_count_available = true;
        decoded.dtc_count = (uint16_t)(
            ((uint16_t)generic.data[3] << 8U) | generic.data[4]);
        decoded.records = generic.data + generic.data_length;
        decoded.records_length = 0U;
        break;

    case LINK_UDS_DTC_REPORT_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_SUPPORTED_DTC:
    case LINK_UDS_DTC_REPORT_FIRST_TEST_FAILED_DTC:
    case LINK_UDS_DTC_REPORT_FIRST_CONFIRMED_DTC:
    case LINK_UDS_DTC_REPORT_MOST_RECENT_TEST_FAILED_DTC:
    case LINK_UDS_DTC_REPORT_MOST_RECENT_CONFIRMED_DTC:
    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_EMISSIONS_OBD_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_WITH_PERMANENT_STATUS:
        if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[1];
        decoded.record_format = LINK_UDS_DTC_RECORDS_DTC_STATUS;
        decoded.records = generic.data + 2U;
        decoded.records_length = generic.data_length - 2U;
        if (!link_uds_dtc_response_records_valid(&decoded, 4U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_SNAPSHOT_IDENTIFICATION:
        decoded.record_format =
            LINK_UDS_DTC_RECORDS_SNAPSHOT_IDENTIFICATION;
        if (!link_uds_dtc_response_records_valid(&decoded, 4U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_BY_SEVERITY_MASK_RECORD:
    case LINK_UDS_DTC_REPORT_SEVERITY_INFORMATION_OF_DTC:
        if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[1];
        decoded.record_format = LINK_UDS_DTC_RECORDS_DTC_SEVERITY;
        decoded.records = generic.data + 2U;
        decoded.records_length = generic.data_length - 2U;
        if (!link_uds_dtc_response_records_valid(&decoded, 6U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_FAULT_DETECTION_COUNTER:
        decoded.record_format =
            LINK_UDS_DTC_RECORDS_FAULT_DETECTION_COUNTER;
        if (!link_uds_dtc_response_records_valid(&decoded, 4U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_USER_MEMORY_BY_STATUS_MASK:
        if (generic.data_length < 3U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.memory_selection_available = true;
        decoded.memory_selection = generic.data[1];
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[2];
        decoded.record_format = LINK_UDS_DTC_RECORDS_DTC_STATUS;
        decoded.records = generic.data + 3U;
        decoded.records_length = generic.data_length - 3U;
        if (!link_uds_dtc_response_records_valid(&decoded, 4U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_USER_MEMORY_SNAPSHOT_BY_DTC_NUMBER:
    case LINK_UDS_DTC_REPORT_USER_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.memory_selection_available = true;
        decoded.memory_selection = generic.data[1];
        decoded.records = generic.data + 2U;
        decoded.records_length = generic.data_length - 2U;
        break;

    case LINK_UDS_DTC_REPORT_STORED_DATA_BY_RECORD_NUMBER:
    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_RECORD_NUMBER:
        if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.record_number_available = true;
        decoded.record_number = generic.data[1];
        decoded.records = generic.data + 2U;
        decoded.records_length = generic.data_length - 2U;
        break;

    case LINK_UDS_DTC_REPORT_WWH_OBD_BY_MASK_RECORD:
        if (generic.data_length < 5U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.functional_group_identifier_available = true;
        decoded.functional_group_identifier = generic.data[1];
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[2];
        decoded.severity_availability_mask_available = true;
        decoded.severity_availability_mask = generic.data[3];
        decoded.dtc_format_identifier_available = true;
        decoded.dtc_format_identifier = generic.data[4];
        decoded.record_format = LINK_UDS_DTC_RECORDS_WWH_SEVERITY;
        decoded.records = generic.data + 5U;
        decoded.records_length = generic.data_length - 5U;
        if (!link_uds_dtc_response_records_valid(&decoded, 5U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_WWH_OBD_WITH_PERMANENT_STATUS:
        if (generic.data_length < 4U) return LINK_UDS_RESULT_MALFORMED_PDU;
        decoded.functional_group_identifier_available = true;
        decoded.functional_group_identifier = generic.data[1];
        decoded.status_availability_mask_available = true;
        decoded.status_availability_mask = generic.data[2];
        decoded.dtc_format_identifier_available = true;
        decoded.dtc_format_identifier = generic.data[3];
        decoded.record_format = LINK_UDS_DTC_RECORDS_DTC_STATUS;
        decoded.records = generic.data + 4U;
        decoded.records_length = generic.data_length - 4U;
        if (!link_uds_dtc_response_records_valid(&decoded, 4U))
            return LINK_UDS_RESULT_MALFORMED_PDU;
        break;

    case LINK_UDS_DTC_REPORT_SNAPSHOT_BY_DTC_NUMBER:
    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_DTC_NUMBER:
    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        /*
         * These responses contain variable-size snapshot/extended-data
         * records. Preserve the complete post-subfunction payload.
         */
        break;

    default:
        return LINK_UDS_RESULT_UNSUPPORTED;
    }

    *response = decoded;
    return LINK_UDS_RESULT_OK;
}

/* Compatibility helper retained for existing callers of 0x19/0x02. */
static inline LinkUdsResult link_uds_build_report_dtcs_by_status_mask_request(
    uint8_t status_mask,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    LinkUdsDtcInformationRequest request =
        LINK_UDS_DTC_INFORMATION_REQUEST_INIT;
    if (status_mask == 0U) {
        return link_uds_dtc_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    request.subfunction = LINK_UDS_DTC_REPORT_BY_STATUS_MASK;
    request.status_mask = status_mask;
    return link_uds_build_read_dtc_information_request(
        &request, buffer, buffer_size, written);
}

/* Compatibility decoder retained for existing callers of 0x19/0x02. */
static inline LinkUdsResult link_uds_decode_report_dtcs_by_status_mask_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsDtcList *list)
{
    LinkUdsDtcInformationResponse response;
    LinkUdsDtcList decoded = {0};
    LinkUdsResult result;
    size_t record_count;
    size_t index;

    if (list == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = link_uds_decode_read_dtc_information_response(
        LINK_UDS_DTC_REPORT_BY_STATUS_MASK,
        pdu, pdu_length, &response);
    if (result != LINK_UDS_RESULT_OK) return result;

    decoded.availability_mask = response.status_availability_mask;
    record_count = response.records_length / 4U;
    decoded.truncated = record_count > LINK_UDS_DTC_MAX_RECORDS;
    if (record_count > LINK_UDS_DTC_MAX_RECORDS)
        record_count = LINK_UDS_DTC_MAX_RECORDS;

    for (index = 0U; index < record_count; ++index) {
        const uint8_t *record = response.records + (index * 4U);
        decoded.records[index].code =
            ((uint32_t)record[0] << 16U) |
            ((uint32_t)record[1] << 8U) |
            (uint32_t)record[2];
        decoded.records[index].status = record[3];
    }
    decoded.count = record_count;
    *list = decoded;
    return LINK_UDS_RESULT_OK;
}

static inline bool link_uds_dtc_status_matches(
    const LinkUdsDtcRecord *record,
    uint8_t status_mask)
{
    return record != NULL && status_mask != 0U &&
           (record->status & status_mask) != 0U;
}

static inline bool link_uds_dtc_format_hex(
    uint32_t code,
    char *buffer,
    size_t buffer_size)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t index;

    if (buffer == NULL || buffer_size < 7U || code > UINT32_C(0x00ffffff)) {
        if (buffer != NULL && buffer_size != 0U) buffer[0] = '\0';
        return false;
    }
    for (index = 0U; index < 6U; ++index) {
        const unsigned int shift = (unsigned int)((5U - index) * 4U);
        buffer[index] = digits[(code >> shift) & 0x0fU];
    }
    buffer[6] = '\0';
    return true;
}

#ifdef __cplusplus
}
#endif

#endif
