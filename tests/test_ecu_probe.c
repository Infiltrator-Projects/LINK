// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/ecu_probe.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static LinkElm327Response ok_response(const char *text)
{
    LinkElm327Response response;
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    response.ok_seen = true;
    if (text != NULL) {
        (void)snprintf(response.text, sizeof(response.text), "%s", text);
    }
    return response;
}

static void accept_channel_configuration(LinkEcuProbe *probe)
{
    char command[64];
    size_t written = 0U;
    LinkElm327Response response = ok_response("OK");

    assert(link_ecu_probe_command(probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "ATSH7E0") == 0);
    assert(link_ecu_probe_accept(probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    assert(link_ecu_probe_command(probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "ATCRA7E8") == 0);
    assert(link_ecu_probe_accept(probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    assert(link_ecu_probe_command(probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "ATCAF1") == 0);
    assert(link_ecu_probe_accept(probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    assert(link_ecu_probe_command(probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "ATCFC1") == 0);
    assert(link_ecu_probe_accept(probe, &response) == LINK_ECU_PROBE_RESULT_OK);
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

    assert(link_ecu_probe_profile_is_valid(&profile));
    assert(link_ecu_probe_begin(&probe, &profile) == LINK_ECU_PROBE_RESULT_OK);
    assert(probe.stage == LINK_ECU_PROBE_STAGE_CONFIGURE_CHANNEL);

    accept_channel_configuration(&probe);
    assert(probe.stage == LINK_ECU_PROBE_STAGE_TESTER_PRESENT);

    assert(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "3E00") == 0);
    response = ok_response("7E00");
    assert(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    assert(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "22F190") == 0);
    response = ok_response("62F1905744443230373330323246313233343536");
    assert(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    did_result = link_ecu_probe_did_result_at(&probe, 0U);
    assert(did_result != NULL);
    assert(did_result->status == LINK_ECU_PROBE_READ_AVAILABLE);
    assert(did_result->data_length == 17U);
    assert(memcmp(did_result->data, "WDD2073022F123456", 17U) == 0);

    assert(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "22F18C") == 0);
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_NO_DATA;
    assert(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_OK);

    did_result = link_ecu_probe_did_result_at(&probe, 1U);
    assert(did_result != NULL);
    assert(did_result->status == LINK_ECU_PROBE_READ_NO_RESPONSE);
    assert(probe.stage == LINK_ECU_PROBE_STAGE_READ_DTC_INFORMATION);

    assert(link_ecu_probe_command(&probe, command, sizeof(command), &written) ==
           LINK_ECU_PROBE_RESULT_OK);
    assert(strcmp(command, "1902FF") == 0);
    response = ok_response("5902FF0112345609");
    assert(link_ecu_probe_accept(&probe, &response) == LINK_ECU_PROBE_RESULT_COMPLETE);
    assert(probe.stage == LINK_ECU_PROBE_STAGE_COMPLETE);
    assert(probe.dtc_status == LINK_ECU_PROBE_READ_AVAILABLE);
    assert(probe.dtcs.count == 1U);
    assert(probe.dtcs.records[0].code == 0x123456U);
    assert(probe.dtcs.records[0].status == 0x09U);
    assert(link_ecu_probe_did_result_count(&probe) == 2U);

    puts("LINK ECU probe tests passed");
    return 0;
}
