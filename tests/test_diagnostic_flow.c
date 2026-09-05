// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_flow.h"
#include "link/fault_scan.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static bool near_value(double value, double expected, double tolerance)
{
    double difference = value - expected;
    if (difference < 0.0) difference = -difference;
    return difference <= tolerance;
}

static LinkElm327Response response_ok(const char *text, bool ok_seen)
{
    LinkElm327Response response;
    size_t length = text == NULL ? 0U : strlen(text);

    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    response.ok_seen = ok_seen;
    if (length >= sizeof(response.text)) {
        length = sizeof(response.text) - 1U;
    }
    if (length != 0U) {
        memcpy(response.text, text, length);
        response.text[length] = '\0';
        response.length = length;
        response.line_count = 1U;
    }
    return response;
}

static LinkElm327Response response_no_data(void)
{
    LinkElm327Response response;
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_NO_DATA;
    return response;
}
static int complete_optional_context(
    LinkDiagnosticFlow *flow,
    bool with_freeze)
{
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;

    CHECK(flow->stage == LINK_DIAGNOSTIC_FLOW_READING_READINESS);
    CHECK(link_diagnostic_flow_next_action(flow, 810U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0101") == 0);
    response = response_ok("410100078000", false);
    CHECK(link_diagnostic_flow_accept_response(
              flow, &response, 810U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow->readiness_attempted);
    CHECK(flow->readiness_available);
    CHECK(link_diagnostic_flow_readiness(flow) != NULL);

    if (!with_freeze) {
        CHECK(event.kind ==
              LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE);
        CHECK(flow->standard_diagnostic_context_complete);
        return 0;
    }

    while (flow->stage == LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME) {
        CHECK(link_diagnostic_flow_next_action(flow, 820U, &action) ==
              LINK_DIAGNOSTIC_FLOW_RESULT_OK);
        response = response_no_data();
        CHECK(link_diagnostic_flow_accept_response(
                  flow, &response, 820U, &event) ==
              LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    }
    CHECK(flow->standard_diagnostic_context_complete);
    return 0;
}


static int complete_initialization(LinkDiagnosticFlow *flow)
{
    static const char *commands[] = {
        "ATZ", "ATE0", "ATL0", "ATS0", "ATH0", "ATSP0", "ATI"
    };
    size_t index;

    for (index = 0U; index < sizeof(commands) / sizeof(commands[0]); ++index) {
        LinkDiagnosticFlowAction action;
        LinkDiagnosticFlowEvent event;
        LinkElm327Response response;

        CHECK(link_diagnostic_flow_next_action(flow, 100U, &action) ==
              LINK_DIAGNOSTIC_FLOW_RESULT_OK);
        CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
        CHECK(strcmp(action.command, commands[index]) == 0);
        CHECK(action.timeout_ms == LINK_DIAGNOSTIC_FLOW_DEFAULT_INIT_TIMEOUT_MS);

        if (index == 0U) {
            response = response_ok("ELM327 v1.5", false);
        } else if (index + 1U == sizeof(commands) / sizeof(commands[0])) {
            response = response_ok("ELM327 v1.5", false);
        } else {
            response = response_ok(NULL, true);
        }

        CHECK(link_diagnostic_flow_accept_response(
                  flow, &response, 100U, &event) ==
              LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    }
    flow->protocol_probe_attempted = true;
    flow->protocol_probe_pending = false;
    flow->protocol_probe_active = false;
    return 0;
}

static int test_protocol_reporting(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;
    const LinkElm327ProtocolDefinition *protocol;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);
    flow.protocol_probe_attempted = false;

    CHECK(link_diagnostic_flow_next_action(&flow, 200U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0100") == 0);
    response = response_ok("410000000000", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 200U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.protocol_probe_pending);

    CHECK(link_diagnostic_flow_next_action(&flow, 210U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATDP") == 0);
    response = response_ok("ISO 9141-2", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 210U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.protocol_probe_pending);

    CHECK(link_diagnostic_flow_next_action(&flow, 220U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATDPN") == 0);
    response = response_ok("A3", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 220U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_PROTOCOL_IDENTIFIED);
    CHECK(event.protocol != NULL);
    CHECK(event.protocol->family == LINK_ELM327_PROTOCOL_FAMILY_ISO_9141_2);
    CHECK(event.protocol_was_automatic);
    CHECK(strcmp(event.protocol_description, "ISO 9141-2") == 0);
    protocol = link_diagnostic_flow_obd_protocol(&flow);
    CHECK(protocol != NULL);
    CHECK(protocol->number == LINK_ELM327_PROTOCOL_ISO_9141_2);
    CHECK(protocol->bit_rate == 10400U);
    CHECK(protocol->init == LINK_ELM327_PROTOCOL_INIT_FIVE_BAUD);
    CHECK(link_diagnostic_flow_obd_protocol_was_automatic(&flow));
    CHECK(strcmp(link_diagnostic_flow_obd_protocol_description(&flow),
                 "ISO 9141-2") == 0);
    return 0;
}

static int test_standard_sequence(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS);
    CHECK(strcmp(link_diagnostic_flow_adapter_identifier(&flow),
                 "ELM327 v1.5") == 0);

    CHECK(link_diagnostic_flow_next_action(&flow, 500U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(strcmp(action.command, "0100") == 0);

    /* PID 0x0C supported, with no continuation block. */
    response = response_ok("410000100000", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 500U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN);
    CHECK(link_obd2_pid_set_contains(
        link_diagnostic_flow_supported_pids(&flow), 0x0cU));

    CHECK(link_diagnostic_flow_next_action(&flow, 550U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0902") == 0);
    response = response_ok(
        "49020153414A414435\n"
        "490202364C36345744\n"
        "4902033738343335", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 550U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN);
    CHECK(event.vin_available);
    CHECK(event.vin != NULL);
    CHECK(strcmp(event.vin, "SAJAD56L64WD78435") == 0);
    CHECK(strcmp(link_diagnostic_flow_standard_vin(&flow),
                 "SAJAD56L64WD78435") == 0);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS);

    CHECK(link_diagnostic_flow_next_action(&flow, 600U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "03") == 0);
    response = response_no_data();
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 600U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST);
    CHECK(event.dtc_kind == LINK_OBD2_DTC_STORED);
    CHECK(event.dtc_list != NULL && event.dtc_list->count == 0U);

    CHECK(link_diagnostic_flow_next_action(&flow, 700U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "07") == 0);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 700U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.dtc_kind == LINK_OBD2_DTC_PENDING);

    CHECK(link_diagnostic_flow_next_action(&flow, 1000U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0A") == 0);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 1000U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.dtc_kind == LINK_OBD2_DTC_PERMANENT);
    CHECK(!event.became_ready);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_READINESS);
    CHECK(complete_optional_context(&flow, false) == 0);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);

    CHECK(link_diagnostic_flow_next_action(&flow, 1000U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(action.pid == 0x0cU);
    CHECK(strcmp(action.command, "010C") == 0);

    response = response_ok("410C1AF8", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 1001U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE);
    CHECK(event.responder_samples.count == 1U);
    CHECK(event.sample.pid == 0x0cU);
    CHECK(near_value(event.sample.value, 1726.0, 0.001));
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);

    CHECK(link_diagnostic_flow_next_action(&flow, 1001U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_WAIT);
    CHECK(action.wait_ms > 0U && action.wait_ms <= 500U);
    return 0;
}

static int test_live_timeout_recovery(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_scheduler_add(
              &flow.scheduler, 0x0cU, 500U,
              LINK_SCHEDULER_PRIORITY_CRITICAL, 100U) ==
          LINK_SCHEDULER_RESULT_OK);
    flow.stage = LINK_DIAGNOSTIC_FLOW_LIVE;

    CHECK(link_diagnostic_flow_next_action(&flow, 100U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(action.pid == 0x0cU);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_LIVE);
    CHECK(flow.awaiting_response);

    CHECK(link_diagnostic_flow_recover_live_timeout(&flow, 150U) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);
    CHECK(!flow.awaiting_response);
    CHECK(flow.scheduler.items[0].next_due_ms >= 650U);

    CHECK(link_diagnostic_flow_next_action(&flow, 150U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_WAIT);
    CHECK(action.wait_ms >= 500U);

    CHECK(link_diagnostic_flow_recover_live_timeout(&flow, 150U) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    return 0;
}

static int test_manufacturer_extension_restore(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;

    config.manufacturer_extension_after_pid_discovery = true;
    config.restore_adapter_after_manufacturer_extension = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);

    CHECK(link_diagnostic_flow_next_action(&flow, 500U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0100") == 0);
    response = response_ok("410000000000", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 500U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN);
    CHECK(link_diagnostic_flow_next_action(&flow, 510U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0902") == 0);
    response = response_no_data();
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 510U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN);
    CHECK(!event.vin_available);
    CHECK(event.vin == NULL);
    CHECK(flow.standard_vin_attempted);
    CHECK(link_diagnostic_flow_standard_vin(&flow) == NULL);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);

    CHECK(link_diagnostic_flow_next_action(&flow, 520U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION);
    CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER);
    CHECK(link_diagnostic_flow_next_action(&flow, 501U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(strcmp(action.command, "ATZ") == 0);
    return 0;
}

static int test_manufacturer_extension_after_standard_vin(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;

    config.manufacturer_extension_after_standard_vin = true;
    config.restore_adapter_after_manufacturer_extension = true;
    config.preserve_pid_discovery_response_headers = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);

    /* Vehicle identity is the first standard request after ELM setup. */
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN);
    CHECK(link_diagnostic_flow_next_action(&flow, 500U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0902") == 0);
    response = response_ok(
        "49020153414A414435\n"
        "490202364C36345744\n"
        "4902033738343335", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 500U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN);
    CHECK(event.vin_available);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);

    CHECK(link_diagnostic_flow_next_action(&flow, 510U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION);
    CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER);
    CHECK(complete_initialization(&flow) == 0);

    /* After profile/manufacturer work, resume the untouched SAE capability pass. */
    CHECK(flow.stage ==
          LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS);
    CHECK(link_diagnostic_flow_next_action(&flow, 520U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATH1") == 0);
    response = response_ok(NULL, true);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 520U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS);

    CHECK(link_diagnostic_flow_next_action(&flow, 530U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0100") == 0);
    response = response_ok("410000100000", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 530U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE);
    CHECK(flow.stage ==
          LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS);

    CHECK(link_diagnostic_flow_next_action(&flow, 540U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATH0") == 0);
    response = response_ok(NULL, true);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 540U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS);
    CHECK(flow.standard_vin_attempted);
    return 0;
}

static int test_pid_capabilities_per_responder(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;
    const LinkObd2PidSet *engine;
    const LinkObd2PidSet *secondary;

    config.preserve_pid_discovery_response_headers = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);
    CHECK(flow.stage ==
          LINK_DIAGNOSTIC_FLOW_CONFIGURING_PID_DISCOVERY_HEADERS);

    CHECK(link_diagnostic_flow_next_action(&flow, 100U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATH1") == 0);
    response = response_ok(NULL, true);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 100U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS);

    CHECK(link_diagnostic_flow_next_action(&flow, 110U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0100") == 0);
    response = response_ok(
        "7E8 06 41 00 98 3B A0 13\n"
        "7E9 06 41 00 98 18 00 01", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 110U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.supported_pid_base == UINT8_C(0x20));

    CHECK(link_diagnostic_flow_next_action(&flow, 120U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0120") == 0);
    response = response_ok(
        "7E8 06 41 20 B0 03 A0 05\n"
        "7E9 06 41 20 80 01 80 01", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 120U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.supported_pid_base == UINT8_C(0x40));

    CHECK(link_diagnostic_flow_next_action(&flow, 130U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0140") == 0);
    response = response_ok(
        "7E8 06 41 40 4C D8 00 00\n"
        "7E9 06 41 40 40 80 00 00", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 130U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind ==
          LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE);
    CHECK(flow.stage ==
          LINK_DIAGNOSTIC_FLOW_RESTORING_PID_DISCOVERY_HEADERS);

    engine = link_diagnostic_flow_supported_pids_for_responder(
        &flow, UINT32_C(0x7e8), false);
    secondary = link_diagnostic_flow_supported_pids_for_responder(
        &flow, UINT32_C(0x7e9), false);
    CHECK(engine != NULL);
    CHECK(secondary != NULL);
    CHECK(link_obd2_pid_set_contains(engine, UINT8_C(0x2f)));
    CHECK(link_obd2_pid_set_contains(engine, UINT8_C(0x46)));
    CHECK(link_obd2_pid_set_contains(engine, UINT8_C(0x4d)));
    CHECK(!link_obd2_pid_set_contains(secondary, UINT8_C(0x2f)));
    CHECK(link_obd2_pid_set_contains(secondary, UINT8_C(0x42)));
    CHECK(link_obd2_pid_set_contains(secondary, UINT8_C(0x49)));

    CHECK(link_diagnostic_flow_next_action(&flow, 140U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "ATH0") == 0);
    response = response_ok(NULL, true);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 140U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN);
    return 0;
}

static int test_manufacturer_extension_after_standard_dtcs(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;

    config.manufacturer_extension_after_standard_dtcs = true;
    config.restore_adapter_after_manufacturer_extension = true;
    config.preserve_live_response_headers = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(complete_initialization(&flow) == 0);
    CHECK(link_diagnostic_flow_next_action(&flow, 500U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0100") == 0);
    response = response_ok("410000000000", false);
    CHECK(link_diagnostic_flow_accept_response(&flow, &response, 500U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN);
    CHECK(link_diagnostic_flow_next_action(&flow, 550U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0902") == 0);
    response = response_no_data();
    CHECK(link_diagnostic_flow_accept_response(&flow, &response, 550U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN);
    CHECK(!event.vin_available);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS);
    CHECK(link_diagnostic_flow_next_action(&flow, 600U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "03") == 0);

    /*
     * Real C207/Vgate evidence returns one empty-DTC sentinel from each EOBD
     * responder as "4300\n4300". The shared flow must treat that as an
     * empty stored-fault list and continue to pending/permanent faults.
     */
    response = response_ok("4300\n4300", false);
    CHECK(link_diagnostic_flow_accept_response(&flow, &response, 600U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST);
    CHECK(event.dtc_kind == LINK_OBD2_DTC_STORED);
    CHECK(event.dtc_list != NULL && event.dtc_list->count == 0U);

    CHECK(link_diagnostic_flow_next_action(&flow, 700U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "07") == 0);
    response = response_no_data();
    CHECK(link_diagnostic_flow_accept_response(&flow, &response, 700U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_next_action(&flow, 800U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0A") == 0);
    /*
     * Captured C207/Vgate evidence returns 7F 0A 22 for optional permanent
     * DTC Mode 0A.  It is unavailable evidence, not a fatal shared-flow error.
     */
    response = response_ok("7F0A22", false);
    CHECK(link_diagnostic_flow_accept_response(&flow, &response, 800U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(!event.dtc_response_available);
    CHECK(event.dtc_negative_response);
    CHECK(event.dtc_negative_response_code == UINT8_C(0x22));
    CHECK(flow.standard_dtc_inventory_complete);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_READINESS);
    CHECK(!event.became_ready);
    CHECK(complete_optional_context(&flow, false) == 0);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);
    CHECK(link_diagnostic_flow_next_action(&flow, 811U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION);
    CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER);
    CHECK(complete_initialization(&flow) == 0);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS);
    CHECK(link_diagnostic_flow_next_action(&flow, 900U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(strcmp(action.command, "ATH1") == 0);
    response = response_ok(NULL, true);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 900U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);
    return 0;
}

static int test_live_manufacturer_extension(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;

    config.restore_adapter_after_manufacturer_extension = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);

    flow.standard_diagnostic_context_complete = true;
    flow.stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    flow.awaiting_response = false;

    CHECK(link_diagnostic_flow_begin_live_manufacturer_extension(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);

    CHECK(link_diagnostic_flow_next_action(&flow, 1000U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION);

    CHECK(link_diagnostic_flow_begin_live_manufacturer_extension(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);

    CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER);

    flow.stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    flow.awaiting_response = true;
    CHECK(link_diagnostic_flow_begin_live_manufacturer_extension(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);

    flow.awaiting_response = false;
    flow.standard_diagnostic_context_complete = false;
    CHECK(link_diagnostic_flow_begin_live_manufacturer_extension(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    return 0;
}

static int test_invalid_manufacturer_extension_configuration(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;

    config.manufacturer_extension_after_pid_discovery = true;
    config.manufacturer_extension_after_standard_dtcs = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT);

    config = (LinkDiagnosticFlowConfig)LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    config.manufacturer_extension_after_standard_vin = true;
    config.manufacturer_extension_after_pid_discovery = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT);

    config = (LinkDiagnosticFlowConfig)LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    config.manufacturer_extension_after_standard_vin = true;
    config.manufacturer_extension_after_standard_dtcs = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_ARGUMENT);
    return 0;
}

static int test_default_cold_acquisition_timeout(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.config.query_timeout_ms == UINT64_C(8000));
    return 0;
}

static int test_invalid_response_order(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response = response_ok("OK", true);

    CHECK(link_diagnostic_flow_init(&flow, NULL) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 0U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_IDLE);
    return 0;
}

static int test_readiness_and_freeze_context(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowEvent event;
    LinkElm327Response response;
    size_t freeze_count = 0U;
    const LinkObd2Sample *freeze_samples;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    link_obd2_pid_set_clear(&flow.supported_pids);
    flow.supported_pids.bits[UINT8_C(0x05) >> 3U] |=
        (uint8_t)(1U << (UINT8_C(0x05) & 7U));
    flow.supported_pids.bits[UINT8_C(0x0c) >> 3U] |=
        (uint8_t)(1U << (UINT8_C(0x0c) & 7U));
    flow.stored_dtcs.count = 1U;
    flow.stage = LINK_DIAGNOSTIC_FLOW_READING_READINESS;

    CHECK(link_diagnostic_flow_next_action(&flow, 100U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "0101") == 0);
    response = response_ok("4101810F8000", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 100U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_READINESS);
    CHECK(flow.readiness_available);
    CHECK(flow.readiness.mil_on);
    CHECK(flow.readiness.confirmed_dtc_count == 1U);
    CHECK(flow.freeze_frame_requested);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME);

    CHECK(link_diagnostic_flow_next_action(&flow, 110U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "020500") == 0);
    response = response_ok("4205005A", false);
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 110U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE);
    CHECK(event.context_response_available);
    CHECK(event.sample.pid == UINT8_C(0x05));
    CHECK(event.sample.value == 50.0);

    CHECK(link_diagnostic_flow_next_action(&flow, 120U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(strcmp(action.command, "020C00") == 0);
    response = response_no_data();
    CHECK(link_diagnostic_flow_accept_response(
              &flow, &response, 120U, &event) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(event.kind ==
          LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE);
    CHECK(flow.standard_diagnostic_context_complete);
    CHECK(flow.freeze_frame_complete);
    freeze_samples =
        link_diagnostic_flow_freeze_frame_samples(&flow, &freeze_count);
    CHECK(freeze_samples != NULL);
    CHECK(freeze_count == 1U);
    CHECK(freeze_samples[0].pid == UINT8_C(0x05));
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);
    return 0;
}


static int test_scheduled_manufacturer_job(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlowAction action;

    CHECK(link_diagnostic_flow_init(&flow, &config) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    flow.standard_diagnostic_context_complete = true;
    flow.stage = LINK_DIAGNOSTIC_FLOW_LIVE;
    CHECK(link_scheduler_add(
              &flow.scheduler, UINT8_C(0x0c), 500U,
              LINK_SCHEDULER_PRIORITY_CRITICAL, 100U) ==
          LINK_SCHEDULER_RESULT_OK);
    CHECK(link_diagnostic_flow_register_live_manufacturer_job(
              &flow, UINT32_C(0x4d420001), 750U,
              LINK_SCHEDULER_PRIORITY_HIGH, 50U) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);

    CHECK(link_diagnostic_flow_next_action(&flow, 50U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind ==
          LINK_DIAGNOSTIC_FLOW_ACTION_SCHEDULED_MANUFACTURER_JOB);
    CHECK(action.manufacturer_job_token == UINT32_C(0x4d420001));
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);
    CHECK(flow.scheduled_manufacturer_job_active);

    CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);
    CHECK(!flow.scheduled_manufacturer_job_active);

    CHECK(link_diagnostic_flow_next_action(&flow, 100U, &action) ==
          LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND);
    CHECK(strcmp(action.command, "010C") == 0);
    return 0;
}


static int test_fault_scan_presentation_state(void)
{
    CHECK(link_fault_scan_presentation_state(
              false, false, false, false, 0U) ==
          LINK_FAULT_SCAN_NOT_SCANNED);
    CHECK(link_fault_scan_presentation_state(
              true, false, false, false, 0U) ==
          LINK_FAULT_SCAN_IN_PROGRESS);
    CHECK(link_fault_scan_presentation_state(
              true, true, false, false, 0U) ==
          LINK_FAULT_SCAN_IN_PROGRESS);
    CHECK(link_fault_scan_presentation_state(
              true, false, false, true, 0U) ==
          LINK_FAULT_SCAN_FAILED);
    CHECK(link_fault_scan_presentation_state(
              true, false, true, false, 0U) ==
          LINK_FAULT_SCAN_CLEAN);
    CHECK(link_fault_scan_presentation_state(
              true, false, true, false, 2U) ==
          LINK_FAULT_SCAN_FAULTS_PRESENT);
    CHECK(strcmp(
              link_fault_scan_presentation_state_name(
                  LINK_FAULT_SCAN_CLEAN),
              "clean") == 0);
    return 0;
}

int main(void)
{
    if (test_scheduled_manufacturer_job() != 0) return 1;
    if (test_fault_scan_presentation_state() != 0) return 1;
    if (test_protocol_reporting() != 0) return 1;
    if (test_standard_sequence() != 0) return 1;
    if (test_live_timeout_recovery() != 0) return 1;
    if (test_readiness_and_freeze_context() != 0) return 1;
    if (test_manufacturer_extension_restore() != 0) return 1;
    if (test_manufacturer_extension_after_standard_vin() != 0) return 1;
    if (test_pid_capabilities_per_responder() != 0) return 1;
    if (test_manufacturer_extension_after_standard_dtcs() != 0) return 1;
    if (test_live_manufacturer_extension() != 0) return 1;
    if (test_invalid_manufacturer_extension_configuration() != 0) return 1;
    if (test_default_cold_acquisition_timeout() != 0) return 1;
    if (test_invalid_response_order() != 0) return 1;
    puts("diagnostic flow tests passed");
    return 0;
}
