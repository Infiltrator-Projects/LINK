// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/diagnostic_flow.h"
#include "link/elm327_simulator.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at line %d\n", #expr, __LINE__); \
    return 1; \
} } while (0)

typedef struct {
    LinkElm327Simulator simulator;
    LinkElm327Parser parser;
    LinkTransport transport;
} Fixture;

static void receive(void *context, const uint8_t *data, size_t length)
{
    Fixture *fixture = context;
    size_t consumed = 0U;
    (void)link_elm327_parser_feed(&fixture->parser, data, length, &consumed);
}

static LinkElm327Response exchange(Fixture *fixture, const char *command)
{
    char wire[LINK_ELM327_MAX_COMMAND + 2U];
    LinkElm327Response response = {0};
    (void)link_elm327_parser_begin(&fixture->parser, command);
    (void)snprintf(wire, sizeof(wire), "%s\r", command);
    (void)fixture->transport.write(fixture->transport.context,
        (const uint8_t *)wire, strlen(wire));
    (void)link_elm327_parser_finish(&fixture->parser, &response);
    return response;
}

int main(void)
{
    Fixture fixture;
    LinkElm327SimulatorConfig simulator_config = LINK_ELM327_SIMULATOR_CONFIG_INIT;
    LinkDiagnosticFlowConfig config = LINK_DIAGNOSTIC_FLOW_CONFIG_INIT;
    LinkDiagnosticFlow flow;
    LinkPollingPolicy policy;
    unsigned live_samples = 0U;
    unsigned unavailable_samples = 0U;
    unsigned readiness_requests = 0U;
    bool fuel_no_data = false;
    bool long_sample = false;

    memset(&fixture, 0, sizeof(fixture));
    simulator_config.vin = "WDD2073022F123456";
    link_elm327_simulator_init(&fixture.simulator, &simulator_config);
    fixture.transport = link_elm327_simulator_transport(&fixture.simulator);
    fixture.transport.set_receiver(fixture.transport.context, receive, &fixture);
    CHECK(fixture.transport.connect(fixture.transport.context) == LINK_TRANSPORT_OK);
    CHECK(exchange(&fixture, "ATE0").result == LINK_ELM327_RESULT_OK);
    CHECK(exchange(&fixture, "012F").result == LINK_ELM327_RESULT_NO_DATA);
    CHECK(exchange(&fixture, "022F00").result == LINK_ELM327_RESULT_NO_DATA);
    CHECK(exchange(&fixture, "ATNOTREAL").result == LINK_ELM327_RESULT_UNSUPPORTED_COMMAND);

    config.manufacturer_extension_after_standard_vin = true;
    config.restore_adapter_after_manufacturer_extension = true;
    config.preserve_pid_discovery_response_headers = true;
    config.preserve_live_response_headers = true;
    CHECK(link_diagnostic_flow_init(&flow, &config) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
    CHECK(link_diagnostic_flow_start(&flow) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);

    /* A real user can select every documented live reading, including ones
     * without a canned simulator sample. Never restrict this test to RPM/speed. */
    link_polling_policy_init(&policy, false);
    for (size_t i = 0U; i < link_obd2_pid_definition_count(); ++i) {
        const LinkObd2PidDefinition *definition = link_obd2_pid_definition_at(i);
        if (definition->mode == 0x01U && definition->pid != 0x01U &&
            (definition->pid & 0x1fU) != 0U) {
            link_polling_policy_set_enabled(&policy, definition->pid, true);
        }
    }
    for (uint64_t now = 0U; now < 120000U; now += 100U) {
        LinkDiagnosticFlowAction action;
        LinkDiagnosticFlowEvent event;
        CHECK(link_diagnostic_flow_next_action(&flow, now, &action) ==
              LINK_DIAGNOSTIC_FLOW_RESULT_OK);
        if (action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION) {
            CHECK(link_diagnostic_flow_resume_after_manufacturer(&flow) ==
                  LINK_DIAGNOSTIC_FLOW_RESULT_OK);
        } else if (action.kind == LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND) {
            LinkElm327Response response = exchange(&fixture, action.command);
            if (strcmp(action.command, "0101") == 0) ++readiness_requests;
            CHECK(link_diagnostic_flow_accept_response(&flow, &response,
                      now + 1U, &event) == LINK_DIAGNOSTIC_FLOW_RESULT_OK);
            if (flow.standard_diagnostic_context_complete) {
                (void)link_polling_policy_apply_to_scheduler(&policy, &flow.scheduler);
            }
            if (event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA) {
                ++unavailable_samples;
                if (event.sample.pid == 0x2fU) fuel_no_data = true;
            } else if (event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE ||
                       event.kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED) {
                ++live_samples;
                if (event.sample.pid == 0x78U) long_sample = true;
            }
        }
    }
    CHECK(flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE);
    CHECK(readiness_requests == 1U);
    CHECK(live_samples > 50U && unavailable_samples > 50U);
    CHECK(fuel_no_data && long_sample);
    printf("Full-catalogue simulation passed: %u samples, %u unavailable reads\n",
           live_samples, unavailable_samples);
    return 0;
}
