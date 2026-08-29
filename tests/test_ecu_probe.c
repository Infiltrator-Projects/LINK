// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/ecu_probe.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    (void)fprintf(stderr, "check failed: %s at %s:%d\n", \
                  #expr, __FILE__, __LINE__); \
    return 1; \
} } while (0)

static LinkElm327Response ok_response(const char *text)
{
    LinkElm327Response response;
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    response.ok_seen = true;
    if (text != NULL) {
        (void)snprintf(response.text, sizeof(response.text), "%s", text);
        response.length = strlen(response.text);
        response.line_count = response.length == 0U ? 0U : 1U;
    }
    return response;
}

static int accept_channel_configuration(LinkEcuProbe *probe)
{
    static const char *const expected[] = {
        "ATSH7E0", "ATCRA7E8", "ATCAF1", "ATCFC1"
    };
    char command[64];
    size_t written = 0U;
    LinkElm327Response response = ok_response("OK");
    size_t index;

    for (index = 0U; index < sizeof(expected) / sizeof(expected[0]); ++index) {
        CHECK(link_ecu_probe_command(probe, command, sizeof(command), &written) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(strcmp(command, expected[index]) == 0);
        CHECK(written == strlen(expected[index]));
        CHECK(link_ecu_probe_accept(probe, &response) ==
              LINK_ECU_PROBE_RESULT_OK);
    }
    return 0;
}

int main(void)
{
    static const LinkEcuProbeDidRequest dids[] = {
        {0xF190U, "vin", "Vehicle identification number"},
        {0xF18CU, "ecu.serial", "ECU serial number"}
    };
    const LinkEcuProbeProfile profile = {
        .channel = {0x7E0U, 0x7E8U, false},
        .dids = dids,
        .did_count = sizeof(dids) / sizeof(dids[0]),
        .tester_present = true,
        .read_dtcs = true
    };
    LinkEcuProbe probe;
    char command[64];
    size_t written = 0U;
    LinkElm327Response response;
    const LinkEcuProbeDidResult *did_result;

    CHECK(link_ecu_probe_profile_is_valid(&profile));
    CHECK(link_ecu_probe_begin(&probe, &profile) == LINK_ECU_PROBE_RESULT_OK);
    CHECK(probe.stage == LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL);

    CHECK(accept_channel_configuration(&probe) == 0);
    CHECK(probe.stage == LINK_ECU_PROBE_STAGE_TESTER_PRESENT);

    CHECK(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
          LINK_ECU_PROBE_RESULT_OK);
    CHECK(strcmp(command, "3E00") == 0);
    response = ok_response("7E00");
    CHECK(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    CHECK(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
          LINK_ECU_PROBE_RESULT_OK);
    CHECK(strcmp(command, "22F190") == 0);
    response = ok_response("62F1905744443230373330323246313233343536");
    CHECK(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    did_result = link_ecu_probe_did_result_at(&probe, 0U);
    CHECK(did_result != NULL);
    CHECK(did_result->status == LINK_ECU_PROBE_READ_AVAILABLE);
    CHECK(did_result->data_length == 17U);
    CHECK(memcmp(did_result->data, "WDD2073022F123456", 17U) == 0);

    CHECK(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
          LINK_ECU_PROBE_RESULT_OK);
    CHECK(strcmp(command, "22F18C") == 0);
    response = ok_response("00B\n0:62F18C333134\n1:3932333333FFFF");
    CHECK(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    did_result = link_ecu_probe_did_result_at(&probe, 1U);
    CHECK(did_result != NULL);
    CHECK(did_result->status == LINK_ECU_PROBE_READ_AVAILABLE);
    CHECK(did_result->data_length == 8U);
    CHECK(probe.stage == LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION);

    CHECK(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
          LINK_ECU_PROBE_RESULT_OK);
    CHECK(strcmp(command, "1902FF") == 0);
    response = ok_response("7F1978\n5902FF12345609");
    CHECK(link_ecu_probe_accept(&probe, &response) ==
          LINK_ECU_PROBE_RESULT_COMPLETE);
    CHECK(probe.stage == LINK_ECU_PROBE_STAGE_COMPLETE);
    CHECK(probe.dtc_status == LINK_ECU_PROBE_READ_AVAILABLE);
    CHECK(probe.dtcs.count == 1U);
    CHECK(probe.dtcs.records[0].code == UINT32_C(0x123456));
    CHECK(probe.dtcs.records[0].status == 0x09U);
    CHECK(link_ecu_probe_did_result_count(&probe) == 2U);

    {
        LinkEcuProbe direct;
        LinkDiagnosticRequestDefinition request;
        uint8_t pdu[8];
        size_t pdu_length = 0U;
        static const uint8_t tester_response[] = { 0x7eU, 0x00U };
        static const uint8_t vin_response[] = {
            0x62U, 0xf1U, 0x90U,
            'W','D','D','2','0','7','3','0','2','2','F','1','2','3','4','5','6'
        };
        static const uint8_t serial_negative[] = { 0x7fU, 0x22U, 0x31U };
        static const uint8_t dtc_response[] = {
            0x59U, 0x02U, 0xffU, 0x12U, 0x34U, 0x56U, 0x09U
        };

        CHECK(link_ecu_probe_begin_direct(&direct, &profile) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(direct.stage == LINK_ECU_PROBE_STAGE_TESTER_PRESENT);
        CHECK(link_ecu_probe_diagnostic_request(
                  &direct, pdu, sizeof(pdu), &pdu_length, &request) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(pdu_length == 2U && pdu[0] == 0x3eU && pdu[1] == 0x00U);
        CHECK(request.request_can_id == UINT32_C(0x7e0));
        CHECK(request.response_can_id == UINT32_C(0x7e8));
        CHECK(request.response_can_id_known && !request.extended_id);
        CHECK(link_ecu_probe_accept_pdu(
                  &direct, true, tester_response,
                  sizeof(tester_response)) == LINK_ECU_PROBE_RESULT_OK);

        CHECK(link_ecu_probe_diagnostic_request(
                  &direct, pdu, sizeof(pdu), &pdu_length, &request) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(pdu_length == 3U &&
              pdu[0] == 0x22U && pdu[1] == 0xf1U && pdu[2] == 0x90U);
        CHECK(link_ecu_probe_accept_pdu(
                  &direct, true, vin_response, sizeof(vin_response)) ==
              LINK_ECU_PROBE_RESULT_OK);
        did_result = link_ecu_probe_did_result_at(&direct, 0U);
        CHECK(did_result != NULL &&
              did_result->status == LINK_ECU_PROBE_READ_AVAILABLE);
        CHECK(did_result->data_length == 17U);
        CHECK(memcmp(did_result->data, "WDD2073022F123456", 17U) == 0);

        CHECK(link_ecu_probe_diagnostic_request(
                  &direct, pdu, sizeof(pdu), &pdu_length, &request) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(link_ecu_probe_accept_pdu(
                  &direct, true, serial_negative,
                  sizeof(serial_negative)) == LINK_ECU_PROBE_RESULT_OK);
        did_result = link_ecu_probe_did_result_at(&direct, 1U);
        CHECK(did_result != NULL &&
              did_result->status ==
                  LINK_ECU_PROBE_READ_NEGATIVE_RESPONSE);
        CHECK(did_result->negative_response_code == 0x31U);

        CHECK(direct.stage == LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION);
        CHECK(link_ecu_probe_diagnostic_request(
                  &direct, pdu, sizeof(pdu), &pdu_length, &request) ==
              LINK_ECU_PROBE_RESULT_OK);
        CHECK(pdu_length == 3U &&
              pdu[0] == 0x19U && pdu[1] == 0x02U && pdu[2] == 0xffU);
        CHECK(link_ecu_probe_accept_pdu(
                  &direct, true, dtc_response, sizeof(dtc_response)) ==
              LINK_ECU_PROBE_RESULT_COMPLETE);
        CHECK(direct.stage == LINK_ECU_PROBE_STAGE_COMPLETE);
        CHECK(direct.dtc_status == LINK_ECU_PROBE_READ_AVAILABLE);
        CHECK(direct.dtcs.count == 1U);
        CHECK(direct.dtcs.records[0].code == UINT32_C(0x123456));
    }

    puts("LINK ECU probe tests passed");
    return 0;
}
