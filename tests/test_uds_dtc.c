// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_dtc.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

typedef struct {
    uint8_t subfunction;
    uint8_t status_mask;
    uint8_t severity_mask;
    uint32_t dtc;
    uint8_t record_number;
    uint8_t memory_selection;
    uint8_t functional_group;
    uint8_t expected[8];
    size_t expected_length;
} ReadDtcRequestCase;

static int test_complete_report_catalogue(void)
{
    static const uint8_t expected_subfunctions[
        LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT] = {
        0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U,
        0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU,
        0x0fU, 0x10U, 0x11U, 0x12U, 0x13U, 0x14U, 0x15U,
        0x16U, 0x17U, 0x18U, 0x19U, 0x42U, 0x55U
    };
    size_t index;

    CHECK(link_uds_dtc_report_definition_count() ==
          LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT);
    for (index = 0U;
         index < LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT;
         ++index) {
        const LinkUdsDtcReportDefinition *definition =
            link_uds_dtc_report_definition_at(index);
        CHECK(definition != NULL);
        CHECK(definition->subfunction == expected_subfunctions[index]);
        CHECK(definition->name != NULL && definition->name[0] != '\0');
        CHECK(link_uds_dtc_report_definition(definition->subfunction) ==
              definition);
    }
    CHECK(link_uds_dtc_report_definition_at(
              LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT) == NULL);
    CHECK(link_uds_dtc_report_definition(0x00U) == NULL);
    CHECK(link_uds_dtc_report_definition(0x1aU) == NULL);

    CHECK(link_uds_dtc_report_definition(0x0fU)->withdrawn_in_2020);
    CHECK(link_uds_dtc_report_definition(0x10U)->withdrawn_in_2020);
    CHECK(link_uds_dtc_report_definition(0x11U)->withdrawn_in_2020);
    CHECK(link_uds_dtc_report_definition(0x12U)->withdrawn_in_2020);
    CHECK(link_uds_dtc_report_definition(0x13U)->withdrawn_in_2020);
    CHECK(!link_uds_dtc_report_definition(0x19U)->withdrawn_in_2020);
    return 0;
}

static int test_all_report_requests(void)
{
    static const ReadDtcRequestCase cases[] = {
        {0x01U,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x01U,0xa5U},3U},
        {0x02U,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x02U,0xa5U},3U},
        {0x03U,0U,0U,0U,0U,0U,0U,{0x19U,0x03U},2U},
        {0x04U,0U,0U,0x123456U,0x22U,0U,0U,
            {0x19U,0x04U,0x12U,0x34U,0x56U,0x22U},6U},
        {0x05U,0U,0U,0U,0x23U,0U,0U,{0x19U,0x05U,0x23U},3U},
        {0x06U,0U,0U,0x123456U,0x24U,0U,0U,
            {0x19U,0x06U,0x12U,0x34U,0x56U,0x24U},6U},
        {0x07U,0xa5U,0xc0U,0U,0U,0U,0U,
            {0x19U,0x07U,0xc0U,0xa5U},4U},
        {0x08U,0xa5U,0xc0U,0U,0U,0U,0U,
            {0x19U,0x08U,0xc0U,0xa5U},4U},
        {0x09U,0U,0U,0x123456U,0U,0U,0U,
            {0x19U,0x09U,0x12U,0x34U,0x56U},5U},
        {0x0aU,0U,0U,0U,0U,0U,0U,{0x19U,0x0aU},2U},
        {0x0bU,0U,0U,0U,0U,0U,0U,{0x19U,0x0bU},2U},
        {0x0cU,0U,0U,0U,0U,0U,0U,{0x19U,0x0cU},2U},
        {0x0dU,0U,0U,0U,0U,0U,0U,{0x19U,0x0dU},2U},
        {0x0eU,0U,0U,0U,0U,0U,0U,{0x19U,0x0eU},2U},
        {0x0fU,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x0fU,0xa5U},3U},
        {0x10U,0U,0U,0x123456U,0x25U,0U,0U,
            {0x19U,0x10U,0x12U,0x34U,0x56U,0x25U},6U},
        {0x11U,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x11U,0xa5U},3U},
        {0x12U,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x12U,0xa5U},3U},
        {0x13U,0xa5U,0U,0U,0U,0U,0U,{0x19U,0x13U,0xa5U},3U},
        {0x14U,0U,0U,0U,0U,0U,0U,{0x19U,0x14U},2U},
        {0x15U,0U,0U,0U,0U,0U,0U,{0x19U,0x15U},2U},
        {0x16U,0U,0U,0U,0x26U,0U,0U,{0x19U,0x16U,0x26U},3U},
        {0x17U,0xa5U,0U,0U,0U,0x31U,0U,
            {0x19U,0x17U,0xa5U,0x31U},4U},
        {0x18U,0U,0U,0x123456U,0x27U,0x31U,0U,
            {0x19U,0x18U,0x12U,0x34U,0x56U,0x27U,0x31U},7U},
        {0x19U,0U,0U,0x123456U,0x28U,0x31U,0U,
            {0x19U,0x19U,0x12U,0x34U,0x56U,0x28U,0x31U},7U},
        /*
         * ISO 14229-1:2013 Table 267 orders the WWH mask record as
         * FunctionalGroupIdentifier, DTCStatusMask, DTCSeverityMask.
         */
        {0x42U,0xa5U,0xc0U,0U,0U,0U,0x33U,
            {0x19U,0x42U,0x33U,0xa5U,0xc0U},5U},
        {0x55U,0U,0U,0U,0U,0U,0x33U,
            {0x19U,0x55U,0x33U},3U}
    };
    uint8_t request_buffer[8];
    size_t index;

    CHECK(sizeof(cases) / sizeof(cases[0]) ==
          LINK_UDS_DTC_REPORT_SUBFUNCTION_COUNT);
    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        LinkUdsDtcInformationRequest request =
            LINK_UDS_DTC_INFORMATION_REQUEST_INIT;
        size_t written = 99U;
        LinkUdsResult result;

        request.subfunction = cases[index].subfunction;
        request.status_mask = cases[index].status_mask;
        request.severity_mask = cases[index].severity_mask;
        request.dtc = cases[index].dtc;
        request.record_number = cases[index].record_number;
        request.memory_selection = cases[index].memory_selection;
        request.functional_group_identifier = cases[index].functional_group;

        memset(request_buffer, 0xa5, sizeof(request_buffer));
        result = link_uds_build_read_dtc_information_request(
            &request, request_buffer, sizeof(request_buffer), &written);
        CHECK(result == LINK_UDS_RESULT_OK);
        CHECK(written == cases[index].expected_length);
        CHECK(memcmp(request_buffer, cases[index].expected, written) == 0);

        if (written > 1U) {
            request.suppress_positive_response = true;
            CHECK(link_uds_build_read_dtc_information_request(
                &request, request_buffer, sizeof(request_buffer),
                &written) == LINK_UDS_RESULT_OK);
            CHECK((request_buffer[1] & 0x80U) != 0U);
            CHECK((request_buffer[1] & 0x7fU) == cases[index].subfunction);
        }
    }

    {
        LinkUdsDtcInformationRequest invalid =
            LINK_UDS_DTC_INFORMATION_REQUEST_INIT;
        size_t written = 99U;
        invalid.subfunction = 0x1aU;
        CHECK(link_uds_build_read_dtc_information_request(
                  &invalid, request_buffer, sizeof(request_buffer),
                  &written) == LINK_UDS_RESULT_INVALID_ARGUMENT);
        CHECK(written == 0U && request_buffer[0] == 0U);
        invalid.subfunction = LINK_UDS_DTC_REPORT_SEVERITY_INFORMATION_OF_DTC;
        invalid.dtc = UINT32_C(0x01000000);
        CHECK(link_uds_build_read_dtc_information_request(
                  &invalid, request_buffer, sizeof(request_buffer),
                  &written) == LINK_UDS_RESULT_INVALID_ARGUMENT);
        CHECK(written == 0U);
    }
    return 0;
}

static int test_generic_report_responses(void)
{
    LinkUdsDtcInformationResponse response;

    {
        const uint8_t pdu[] = {0x59U,0x01U,0xffU,0x04U,0x00U,0x12U};
        CHECK(link_uds_decode_read_dtc_information_response(
            0x01U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.status_availability_mask_available);
        CHECK(response.status_availability_mask == 0xffU);
        CHECK(response.dtc_format_identifier_available);
        CHECK(response.dtc_format_identifier == 0x04U);
        CHECK(response.dtc_count_available && response.dtc_count == 0x12U);
        CHECK(response.records_length == 0U);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x08U,0xffU,
            0x20U,0x10U,0x12U,0x34U,0x56U,0x09U
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x08U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.record_format == LINK_UDS_DTC_RECORDS_DTC_SEVERITY);
        CHECK(response.records_length == 6U);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x14U,0x12U,0x34U,0x56U,0x7fU
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x14U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.record_format ==
              LINK_UDS_DTC_RECORDS_FAULT_DETECTION_COUNTER);
        CHECK(response.records_length == 4U);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x17U,0x31U,0xffU,
            0x12U,0x34U,0x56U,0x09U
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x17U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.memory_selection_available);
        CHECK(response.memory_selection == 0x31U);
        CHECK(response.status_availability_mask == 0xffU);
        CHECK(response.records_length == 4U);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x19U,0x31U,
            0x12U,0x34U,0x56U,0x09U,0x90U,0xaaU,0xbbU
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x19U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.memory_selection_available);
        CHECK(response.memory_selection == 0x31U);
        CHECK(response.record_format == LINK_UDS_DTC_RECORDS_RAW);
        CHECK(response.records_length == 7U);
        CHECK(response.records[0] == 0x12U);
        CHECK(response.records[6] == 0xbbU);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x42U,0x33U,0xffU,0xe0U,0x04U,
            0x20U,0x12U,0x34U,0x56U,0x09U
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x42U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.functional_group_identifier_available);
        CHECK(response.functional_group_identifier == 0x33U);
        CHECK(response.status_availability_mask == 0xffU);
        CHECK(response.severity_availability_mask == 0xe0U);
        CHECK(response.dtc_format_identifier == 0x04U);
        CHECK(response.record_format == LINK_UDS_DTC_RECORDS_WWH_SEVERITY);
        CHECK(response.records_length == 5U);
    }
    {
        const uint8_t pdu[] = {
            0x59U,0x55U,0x33U,0xffU,0x04U,
            0x12U,0x34U,0x56U,0x09U
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x55U, pdu, sizeof(pdu), &response) == LINK_UDS_RESULT_OK);
        CHECK(response.functional_group_identifier == 0x33U);
        CHECK(response.record_format == LINK_UDS_DTC_RECORDS_DTC_STATUS);
        CHECK(response.records_length == 4U);
    }
    {
        const uint8_t malformed[] = {
            0x59U,0x02U,0xffU,0x12U,0x34U
        };
        CHECK(link_uds_decode_read_dtc_information_response(
            0x02U, malformed, sizeof(malformed), &response) ==
            LINK_UDS_RESULT_MALFORMED_PDU);
    }
    {
        const uint8_t negative[] = {0x7fU,0x19U,0x31U};
        CHECK(link_uds_decode_read_dtc_information_response(
            0x02U, negative, sizeof(negative), &response) ==
            LINK_UDS_RESULT_NEGATIVE_RESPONSE);
    }
    return 0;
}

static int test_build_request(void)
{
    uint8_t request[4] = {0xa5U, 0xa5U, 0xa5U, 0xa5U};
    size_t written = 99U;

    CHECK(link_uds_build_report_dtcs_by_status_mask_request(
              LINK_UDS_DTC_STATUS_MASK_ALL,
              request, sizeof(request), &written) == LINK_UDS_RESULT_OK);
    CHECK(written == 3U);
    CHECK(request[0] == 0x19U && request[1] == 0x02U && request[2] == 0xffU);

    request[0] = 0xa5U;
    written = 99U;
    CHECK(link_uds_build_report_dtcs_by_status_mask_request(
              LINK_UDS_DTC_STATUS_MASK_ALL,
              request, 2U, &written) == LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    CHECK(request[0] == 0U && written == 0U);
    CHECK(link_uds_build_report_dtcs_by_status_mask_request(
              0U, request, sizeof(request), &written) ==
          LINK_UDS_RESULT_INVALID_ARGUMENT);
    return 0;
}

static int test_decode_records(void)
{
    const uint8_t pdu[] = {
        0x59U, 0x02U, 0xffU,
        0x12U, 0x34U, 0x56U, 0x09U,
        0xabU, 0xcdU, 0xefU, 0x28U
    };
    LinkUdsDtcList list;
    char text[7];

    memset(&list, 0xa5, sizeof(list));
    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              pdu, sizeof(pdu), &list) == LINK_UDS_RESULT_OK);
    CHECK(list.availability_mask == 0xffU);
    CHECK(list.count == 2U && !list.truncated);
    CHECK(list.records[0].code == UINT32_C(0x123456));
    CHECK(list.records[0].status == 0x09U);
    CHECK(list.records[1].code == UINT32_C(0xabcdef));
    CHECK(list.records[1].status == 0x28U);
    CHECK(link_uds_dtc_status_matches(
        &list.records[0], LINK_UDS_DTC_STATUS_CONFIRMED_DTC));
    CHECK(!link_uds_dtc_status_matches(
        &list.records[0], LINK_UDS_DTC_STATUS_PENDING_DTC));
    CHECK(link_uds_dtc_format_hex(list.records[1].code, text, sizeof(text)));
    CHECK(strcmp(text, "ABCDEF") == 0);
    return 0;
}

static int test_invalid_responses(void)
{
    const uint8_t empty[] = {0x59U, 0x02U, 0xffU};
    const uint8_t wrong_subfunction[] = {0x59U, 0x0aU, 0xffU};
    const uint8_t truncated[] = {0x59U, 0x02U};
    const uint8_t partial_record[] = {0x59U, 0x02U, 0xffU, 0x12U, 0x34U};
    const uint8_t negative[] = {0x7fU, 0x19U, 0x31U};
    LinkUdsDtcList list;

    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              empty, sizeof(empty), &list) == LINK_UDS_RESULT_OK);
    CHECK(list.count == 0U);
    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              wrong_subfunction, sizeof(wrong_subfunction), &list) ==
          LINK_UDS_RESULT_UNEXPECTED_RESPONSE);
    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              truncated, sizeof(truncated), &list) ==
          LINK_UDS_RESULT_MALFORMED_PDU);
    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              partial_record, sizeof(partial_record), &list) ==
          LINK_UDS_RESULT_MALFORMED_PDU);
    CHECK(link_uds_decode_report_dtcs_by_status_mask_response(
              negative, sizeof(negative), &list) ==
          LINK_UDS_RESULT_NEGATIVE_RESPONSE);
    return 0;
}

int main(void)
{
    char text[7] = "bad";
    if (test_complete_report_catalogue() != 0) return 1;
    if (test_all_report_requests() != 0) return 1;
    if (test_generic_report_responses() != 0) return 1;
    if (test_build_request() != 0) return 1;
    if (test_decode_records() != 0) return 1;
    if (test_invalid_responses() != 0) return 1;
    CHECK(!link_uds_dtc_format_hex(UINT32_C(0x01000000), text, sizeof(text)));
    CHECK(text[0] == '\0');
    CHECK(!link_uds_dtc_format_hex(UINT32_C(0x123456), text, 6U));
    CHECK(text[0] == '\0');
    puts("LINK UDS DTC tests passed");
    return 0;
}
