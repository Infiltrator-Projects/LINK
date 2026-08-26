// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_dtc.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

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
