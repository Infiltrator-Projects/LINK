// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/kwp2000.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

int main(void)
{
    uint8_t request[8];
    size_t written = 0U;
    LinkKwp2000Response generic;

    check(link_kwp2000_build_tester_present_request(
              1, request, sizeof(request), &written) ==
              LINK_KWP2000_RESULT_OK &&
          written == 2U && request[0] == 0x3eU && request[1] == 0x01U,
          "response-required TesterPresent");
    {
        const uint8_t reply[] = { 0x7eU };
        check(link_kwp2000_decode_tester_present_response(
                  reply, sizeof(reply), 1) == LINK_KWP2000_RESULT_OK,
              "TesterPresent positive response");
    }
    {
        const uint8_t reply[] = { 0x7fU, 0x3eU, 0x11U };
        check(link_kwp2000_decode_response(
                  0x3eU, reply, sizeof(reply), &generic) ==
                  LINK_KWP2000_RESULT_NEGATIVE_RESPONSE &&
              generic.negative_response_code == 0x11U,
              "generic KWP negative response");
    }

    check(link_kwp2000_build_read_common_identifier_request(
              0xf100U, request, sizeof(request), &written) ==
              LINK_KWP2000_RESULT_OK &&
          written == 3U && request[0] == 0x22U &&
          request[1] == 0xf1U && request[2] == 0x00U,
          "ReadDataByCommonIdentifier builder");
    {
        const uint8_t reply[] = { 0x62U, 0xf1U, 0x00U, 0x4fU, 0x52U, 0x43U };
        LinkKwp2000CommonIdentifierRecord record;
        check(link_kwp2000_decode_read_common_identifier_response(
                  reply, sizeof(reply), 0xf100U, &record) ==
                  LINK_KWP2000_RESULT_OK &&
              record.identifier == 0xf100U && record.data_length == 3U &&
              memcmp(record.data, "ORC", 3U) == 0,
              "ReadDataByCommonIdentifier decoder");
    }

    check(link_kwp2000_build_read_ecu_identification_request(
              0x90U, request, sizeof(request), &written) ==
              LINK_KWP2000_RESULT_OK &&
          written == 2U && request[0] == 0x1aU && request[1] == 0x90U,
          "ReadECUIdentification builder");
    {
        const uint8_t reply[] = { 0x5aU, 0x90U, 'W', 'D', 'D' };
        LinkKwp2000EcuIdentificationRecord record;
        check(link_kwp2000_decode_read_ecu_identification_response(
                  reply, sizeof(reply), 0x90U, &record) ==
                  LINK_KWP2000_RESULT_OK &&
              record.option == 0x90U && record.data_length == 3U,
              "ReadECUIdentification decoder");
    }

    check(link_kwp2000_build_read_dtc_by_status_request(
              LINK_KWP2000_DTC_REQUEST_STORED_AND_STATUS,
              LINK_KWP2000_DTC_GROUP_ALL,
              request, sizeof(request), &written) ==
              LINK_KWP2000_RESULT_OK &&
          written == 4U && request[0] == 0x18U &&
          request[1] == 0x02U && request[2] == 0xffU &&
          request[3] == 0x00U,
          "ReadDiagnosticTroubleCodesByStatus builder");
    {
        const uint8_t reply[] = {
            0x58U, 0x02U,
            0xd6U, 0xaaU, 0x20U,
            0xd6U, 0xa1U, 0x28U
        };
        LinkKwp2000DtcList dtcs;
        check(link_kwp2000_decode_read_dtc_by_status_response(
                  reply, sizeof(reply), &dtcs) == LINK_KWP2000_RESULT_OK &&
              dtcs.count == 2U && dtcs.reported_count == 2U &&
              dtcs.entries[0].code == 0xd6aaU &&
              dtcs.entries[0].status == 0x20U &&
              dtcs.entries[1].code == 0xd6a1U &&
              dtcs.entries[1].status == 0x28U,
              "KWP DTC response decoder");
    }

    check(link_kwp2000_build_tester_present_request(
              1, request, 1U, &written) ==
              LINK_KWP2000_RESULT_BUFFER_TOO_SMALL && written == 0U,
          "bounded request builder");
    check(link_kwp2000_decode_response(
              0x99U, request, 1U, &generic) ==
              LINK_KWP2000_RESULT_INVALID_ARGUMENT,
          "unsupported service rejected");

    if (failures != 0) {
        (void)fprintf(stderr, "%d KWP2000 test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
