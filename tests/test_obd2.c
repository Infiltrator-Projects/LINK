// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/elm327.h"
#include "link/obd2.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static size_t pid_count(const LinkObd2PidSet *set)
{
    size_t count = 0U;
    unsigned int pid;
    if (set == NULL) return 0U;
    for (pid = 1U; pid <= 0xffU; ++pid) {
        if (link_obd2_pid_set_contains(set, (uint8_t)pid)) count++;
    }
    return count;
}

static LinkElm327Response parse_response(const char *command, const char *wire)
{
    LinkElm327Parser parser;
    LinkElm327Response response;
    size_t consumed = 0U;
    memset(&response, 0, sizeof(response));
    check(link_elm327_parser_begin(&parser, command) == LINK_ELM327_RESULT_OK,
          "parser begin");
    check(link_elm327_parser_feed(&parser, (const uint8_t *)wire,
                                  strlen(wire), &consumed) == LINK_ELM327_RESULT_OK,
          "parser feed");
    check(link_elm327_parser_finish(&parser, &response) == LINK_ELM327_RESULT_OK,
          "parser finish");
    return response;
}

static void test_catalogue(void)
{
    const LinkObd2PidDefinition *definition;
    LinkObd2DecodedPid decoded;
    static const uint8_t odometer_payload[] = {0x00U, 0x01U, 0x86U, 0xa0U};
    static const uint8_t o2_payload[] = {0x40U, 0x00U, 0x20U, 0x00U};
    static const uint8_t vin_payload[] = {
        'S','A','J','A','D','5','6','L','6','4','W','D','7','8','4','3','5'
    };
    static const uint8_t fuel_status[] = {0x02U, 0x00U};
    static const uint8_t throttle_g[] = {0x80U};
    static const uint8_t reflash_distance[] = {0x01U, 0xf4U};

    check(link_obd2_service_definition_count() == 10U,
          "service catalogue contains modes 01 through 0A");
    check(link_obd2_service_definition(0x01U) != NULL &&
              link_obd2_service_definition(0x01U)->read_only,
          "Mode 01 catalogued read-only");
    check(link_obd2_service_definition(0x04U) != NULL &&
              !link_obd2_service_definition(0x04U)->read_only,
          "Mode 04 remains write/control");
    check(link_obd2_service_definition(0x08U) != NULL &&
              !link_obd2_service_definition(0x08U)->read_only,
          "Mode 08 remains write/control");

    check(link_obd2_pid_definition_count() == 188U,
          "shared standards catalogue contains 188 Mode 01/09 definitions");
    check(strcmp(link_obd2_pid_catalogue_snapshot(),
                 "bc58b0eb7273226a1aabae98e956b70b8362bda1+link-standard-supplement-v1") == 0,
          "catalogue provenance includes base snapshot and supplement");

    definition = link_obd2_pid_definition(0x01U, 0x69U);
    check(definition != NULL && definition->bytes == 7U,
          "modern EGR standard PID is catalogued");
    definition = link_obd2_pid_definition(0x01U, 0x7aU);
    check(definition != NULL && definition->bytes == 7U,
          "DPF bank 1 pressure standard PID is catalogued");
    definition = link_obd2_pid_definition(0x01U, 0xc8U);
    check(definition != NULL && definition->bytes == 1U,
          "late standard warning-lamp PID is catalogued");
    definition = link_obd2_pid_definition(0x09U, 0x09U);
    check(definition != NULL && definition->bytes == 1U,
          "Mode 09 ECU-name message count is catalogued");

    check(link_obd2_decode_pid_payload(
              0x01U, 0xa6U, odometer_payload,
              sizeof(odometer_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 10000.0,
          "decode catalogue-backed odometer");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x24U, o2_payload,
              sizeof(o2_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 0.5 &&
              decoded.signals[1].value == 1.0,
          "decode both structured oxygen-sensor values");
    check(link_obd2_decode_pid_payload(
              0x09U, 0x02U, vin_payload,
              sizeof(vin_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.text_available &&
              strcmp(decoded.text, "SAJAD56L64WD78435") == 0,
          "decode catalogue-backed Mode 09 VIN");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x03U, fuel_status,
              sizeof(fuel_status), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.raw_length == 2U && decoded.signal_count == 0U,
          "preserve encoded standard PID raw payload");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x8dU, throttle_g,
              sizeof(throttle_g), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value > 50.19 &&
              decoded.signals[0].value < 50.20,
          "decode supplemented standard throttle G");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xc7U, reflash_distance,
              sizeof(reflash_distance), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 500.0,
          "decode supplemented reflash distance");
}

static void test_requests(void)
{
    char command[16];
    LinkObd2ClearAuthorization authorization = LINK_OBD2_CLEAR_AUTHORIZATION_INIT;

    check(link_obd2_build_standard_read_request(
              0x01U, 0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "generic Mode 01 request");
    check(link_obd2_build_standard_read_request(
              0x05U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0501") == 0,
          "generic Mode 05 request");
    check(link_obd2_build_standard_read_request(
              0x06U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0600") == 0,
          "generic Mode 06 request");
    check(link_obd2_build_standard_read_request(
              0x09U, 0x02U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0902") == 0,
          "generic Mode 09 request");
    check(link_obd2_build_standard_read_request(
              0x04U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 04");
    check(link_obd2_build_standard_read_request(
              0x08U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 08");

    check(link_obd2_build_live_pid_request(0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "legacy RPM request remains compatible");

    check(link_obd2_build_clear_dtc_request(
              &authorization, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "clear DTC deny by default");
    authorization.confirmed = true;
    authorization.acknowledge_readiness_reset = true;
    check(link_obd2_build_clear_dtc_request(
              &authorization, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "04") == 0,
          "clear DTC explicit authorization");
}

static void test_live_and_capabilities(void)
{
    LinkElm327Response response;
    LinkObd2Sample sample;
    LinkObd2ResponderSampleList responders;
    LinkObd2PidSet union_set = {{0}};
    LinkObd2ResponderPidSetList responder_sets = {0};
    const LinkObd2PidSet *engine_set;
    const LinkObd2PidSet *secondary_set;
    bool has_more = false;

    response = parse_response("010C", "410C1AF8\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RPM &&
              sample.value == 1726.0,
          "decode RPM");

    response = parse_response(
        "010C", "7E804410C0CF9\r7E904410C0CFC\r>");
    check(link_obd2_decode_live_pid_responders(
              &response, 0x0cU, &responders) == LINK_OBD2_RESULT_OK &&
              responders.count == 2U && !responders.truncated,
          "decode both C207 RPM responders");
    check(responders.samples[0].responder_id_available &&
              responders.samples[0].responder_id == 0x7e8U,
          "retain engine responder identity");
    check(responders.samples[1].responder_id_available &&
              responders.samples[1].responder_id == 0x7e9U,
          "retain secondary responder identity");

    response = parse_response(
        "0100",
        "7E8 06 41 00 98 3B A0 13\r"
        "7E9 06 41 00 98 18 00 01\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x00U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && has_more,
          "decode C207 0x00 capability block by responder");
    response = parse_response(
        "0120",
        "7E8 06 41 20 B0 03 A0 05\r"
        "7E9 06 41 20 80 01 80 01\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x20U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && has_more,
          "decode C207 0x20 capability block by responder");
    response = parse_response(
        "0140",
        "7E8 06 41 40 4C D8 00 00\r"
        "7E9 06 41 40 40 80 00 00\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x40U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && !has_more,
          "decode C207 0x40 capability block by responder");

    engine_set = link_obd2_responder_pid_set_find(
        &responder_sets, 0x7e8U, false);
    secondary_set = link_obd2_responder_pid_set_find(
        &responder_sets, 0x7e9U, false);
    check(engine_set != NULL && pid_count(engine_set) == 29U,
          "retain all C207 engine capability bits");
    check(secondary_set != NULL && pid_count(secondary_set) == 12U,
          "retain all C207 secondary capability bits");
    check(link_obd2_pid_set_contains(engine_set, 0x2fU) &&
              link_obd2_pid_set_contains(engine_set, 0x46U) &&
              link_obd2_pid_set_contains(engine_set, 0x4dU),
          "engine set preserves later capability pages");
    check(!link_obd2_pid_set_contains(secondary_set, 0x2fU) &&
              link_obd2_pid_set_contains(secondary_set, 0x42U),
          "secondary capability set remains independent");
}

static void test_negative_response(void)
{
    LinkElm327Response response;
    uint8_t nrc = 0U;

    response = parse_response("0A", "7F0A22\r>");
    check(link_obd2_is_negative_response(
              &response, UINT8_C(0x0a), &nrc) && nrc == UINT8_C(0x22),
          "recognize unavailable permanent-DTC response");
    check(!link_obd2_is_negative_response(
              &response, UINT8_C(0x07), &nrc),
          "do not attribute negative response to another service");
}

int main(void)
{
    test_catalogue();
    test_requests();
    test_live_and_capabilities();
    test_negative_response();

    if (failures != 0) {
        fprintf(stderr, "%d OBD-II test(s) failed\n", failures);
        return 1;
    }
    puts("OBD-II standards tests passed");
    return 0;
}
