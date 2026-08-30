// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_flow.h"

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
    CHECK(event.became_ready);
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
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_MANUFACTURER_EXTENSION);
    CHECK(!event.became_ready);
    CHECK(link_diagnostic_flow_next_action(&flow, 801U, &action) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
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

static int test_invalid_manufacturer_extension_configuration(void)
{
    LinkDiagnosticFlow flow;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;

    config.manufacturer_extension_after_pid_discovery = true;
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

int main(void)
{
    if (test_standard_sequence() != 0) return 1;
    if (test_live_timeout_recovery() != 0) return 1;
    if (test_manufacturer_extension_restore() != 0) return 1;
    if (test_manufacturer_extension_after_standard_dtcs() != 0) return 1;
    if (test_invalid_manufacturer_extension_configuration() != 0) return 1;
    if (test_default_cold_acquisition_timeout() != 0) return 1;
    if (test_invalid_response_order() != 0) return 1;
    puts("diagnostic flow tests passed");
    return 0;
}
