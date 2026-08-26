// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_dtc.h
 * @brief Read-only ISO 14229 ReadDTCInformation helpers.
 */
#ifndef LINK_UDS_DTC_H
#define LINK_UDS_DTC_H

#include "link/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_SERVICE_READ_DTC_INFORMATION 0x19U
#define LINK_UDS_DTC_REPORT_BY_STATUS_MASK 0x02U
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

static inline LinkUdsResult link_uds_build_report_dtcs_by_status_mask_request(
    uint8_t status_mask,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (written != NULL) *written = 0U;
    if (buffer != NULL && buffer_size != 0U) buffer[0] = 0U;
    if (status_mask == 0U || buffer == NULL || written == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    if (buffer_size < 3U) return LINK_UDS_RESULT_BUFFER_TOO_SMALL;

    buffer[0] = LINK_UDS_SERVICE_READ_DTC_INFORMATION;
    buffer[1] = LINK_UDS_DTC_REPORT_BY_STATUS_MASK;
    buffer[2] = status_mask;
    *written = 3U;
    return LINK_UDS_RESULT_OK;
}

static inline LinkUdsResult link_uds_decode_report_dtcs_by_status_mask_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsDtcList *list)
{
    LinkUdsResponse generic;
    LinkUdsDtcList decoded = {0};
    LinkUdsResult result;
    size_t record_bytes;
    size_t record_count;
    size_t index;

    if (list == NULL) return LINK_UDS_RESULT_INVALID_ARGUMENT;
    result = link_uds_decode_response(LINK_UDS_SERVICE_READ_DTC_INFORMATION,
                                      pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) return result;
    if (generic.data_length < 2U) return LINK_UDS_RESULT_MALFORMED_PDU;
    if (generic.data[0] != LINK_UDS_DTC_REPORT_BY_STATUS_MASK) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    /*
     * ISO 14229-1 ReportDTCByStatusMask positive response is:
     *   59 02 DTCStatusAvailabilityMask [DTCAndStatusRecord...]
     * There is no DTCFormatIdentifier byte for sub-function 0x02.
     */
    decoded.availability_mask = generic.data[1];
    record_bytes = generic.data_length - 2U;
    if ((record_bytes % 4U) != 0U) return LINK_UDS_RESULT_MALFORMED_PDU;

    record_count = record_bytes / 4U;
    decoded.truncated = record_count > LINK_UDS_DTC_MAX_RECORDS;
    if (record_count > LINK_UDS_DTC_MAX_RECORDS) record_count = LINK_UDS_DTC_MAX_RECORDS;

    for (index = 0U; index < record_count; ++index) {
        const uint8_t *record = generic.data + 2U + (index * 4U);
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
