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

static int test_raw_vin_pdu(void)
{
    static const uint8_t pdu[] = {
        0x49U, 0x02U, 0x01U,
        'S','A','J','A','D','5','6','L','6','4','W','D','7','8','4','3','5'
    };
    char vin[LINK_OBD2_VIN_LENGTH + 1U];
    uint8_t bad[sizeof(pdu)];

    check(link_obd2_decode_vin_pdu(pdu, sizeof(pdu), vin) ==
              LINK_OBD2_RESULT_OK &&
              strcmp(vin, "SAJAD56L64WD78435") == 0,
          "decode raw VIN PDU");
    check(link_obd2_decode_vin_pdu(
              pdu, sizeof(pdu) - 1U, vin) ==
              LINK_OBD2_RESULT_UNEXPECTED_RESPONSE,
          "reject truncated raw VIN PDU");

    memcpy(bad, pdu, sizeof(bad));
    bad[3U] = (uint8_t)'I';
    check(link_obd2_decode_vin_pdu(bad, sizeof(bad), vin) ==
              LINK_OBD2_RESULT_MALFORMED_RESPONSE &&
              vin[0] == '\0',
          "reject invalid raw VIN character");

    bad[0] = 0x48U;
    check(link_obd2_decode_vin_pdu(bad, sizeof(bad), vin) ==
              LINK_OBD2_RESULT_UNEXPECTED_RESPONSE,
          "reject non-VIN raw OBD response");
    return 0;
}

int main(void)
{
    if (test_raw_vin_pdu() != 0) return 1;
    char command[16];
    LinkObd2Sample sample;
    LinkObd2ClearAuthorization authorization = LINK_OBD2_CLEAR_AUTHORIZATION_INIT;
    LinkElm327Response response;
    uint8_t negative_response_code = 0U;

    check(link_obd2_service_definition_count() == 10U,
          "standard service catalogue contains modes 01 through 0A");
    check(link_obd2_service_definition(0x01U) != NULL &&
              link_obd2_service_definition(0x01U)->read_only,
          "Mode 01 is catalogued read-only");
    check(link_obd2_service_definition(0x04U) != NULL &&
              !link_obd2_service_definition(0x04U)->read_only,
          "Mode 04 clear operation is not misclassified read-only");
    check(link_obd2_service_definition(0x08U) != NULL &&
              !link_obd2_service_definition(0x08U)->read_only,
          "Mode 08 control operation is not misclassified read-only");

    check(link_obd2_pid_definition_count() == 132U,
          "pinned OBDex catalogue contains 132 standard Mode 01/09 definitions");
    check(strcmp(link_obd2_pid_catalogue_snapshot(),
                 "bc58b0eb7273226a1aabae98e956b70b8362bda1") == 0,
          "PID catalogue snapshot provenance");
    {
        const LinkObd2PidDefinition *odometer =
            link_obd2_pid_definition(0x01U, 0xa6U);
        LinkObd2DecodedPid decoded;
        static const uint8_t odometer_payload[] = {0x00U, 0x01U, 0x86U, 0xa0U};
        static const uint8_t o2_payload[] = {0x40U, 0x00U, 0x20U, 0x00U};
        static const uint8_t vin_payload[] = {
            'S','A','J','A','D','5','6','L','6','4','W','D','7','8','4','3','5'
        };
        static const uint8_t fuel_status[] = {0x02U, 0x00U};

        check(odometer != NULL && odometer->bytes == 4U &&
                  strcmp(odometer->name, "Odometer") == 0,
              "catalogue exposes modern standard odometer PID");
        check(link_obd2_decode_pid_payload(
                  0x01U, 0xa6U, odometer_payload,
                  sizeof(odometer_payload), &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.signal_count == 1U &&
                  decoded.signals[0].value == 10000.0,
              "decode catalogue-backed 32-bit odometer");
        check(link_obd2_decode_pid_payload(
                  0x01U, 0x24U, o2_payload,
                  sizeof(o2_payload), &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.signal_count == 2U &&
                  decoded.signals[0].value == 0.5 &&
                  decoded.signals[1].value == 1.0,
              "preserve both values from structured oxygen-sensor PID");
        check(link_obd2_decode_pid_payload(
                  0x09U, 0x02U, vin_payload,
                  sizeof(vin_payload), &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.text_available &&
                  strcmp(decoded.text, "SAJAD56L64WD78435") == 0,
              "decode catalogue-backed Mode 09 ASCII value");
        check(link_obd2_decode_pid_payload(
                  0x01U, 0x03U, fuel_status,
                  sizeof(fuel_status), &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.raw_length == 2U && decoded.signal_count == 0U,
              "preserve encoded standard PID without inventing one scalar");
    }

    check(link_obd2_build_standard_read_request(
              0x01U, 0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "generic Mode 01 read request");
    check(link_obd2_build_standard_read_request(
              0x05U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0501") == 0,
          "generic Mode 05 read request");
    check(link_obd2_build_standard_read_request(
              0x06U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0600") == 0,
          "generic Mode 06 read request");
    check(link_obd2_build_standard_read_request(
              0x09U, 0x02U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0902") == 0,
          "generic Mode 09 read request");
    check(link_obd2_build_standard_read_request(
              0x04U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 04 clear operation");
    check(link_obd2_build_standard_read_request(
              0x08U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 08 control operation");
    check(link_obd2_build_standard_read_request(
              0x02U, 0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_INVALID_ARGUMENT && command[0] == '\0',
          "Mode 02 uses explicit freeze-frame request with frame number");

    {
        LinkObd2PidSet genericSupport = {{0}};
        bool more = false;
        static const uint8_t supportPayload[] = {
            0x98U, 0x3bU, 0xa0U, 0x13U
        };
        check(link_obd2_decode_support_bitmap_payload(
                  0x00U, supportPayload, sizeof(supportPayload),
                  &genericSupport, &more) == LINK_OBD2_RESULT_OK && more,
              "generic support bitmap decoder");
        check(link_obd2_pid_set_contains(&genericSupport, 0x04U) &&
                  link_obd2_pid_set_contains(&genericSupport, 0x0cU) &&
                  link_obd2_pid_set_contains(&genericSupport, 0x20U),
              "generic support bitmap retains advertised identifiers");
    }

    check(link_obd2_build_live_pid_request(0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "build RPM request");

    response = parse_response("010C", "410C1AF8\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RPM &&
              sample.value == 1726.0,
          "decode RPM");

    response = parse_response(
        "010C", "7E8 04 41 0C 1A F8\r7E9 04 41 0C 1A F4\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RPM &&
              sample.value == 1726.0,
          "decode RPM while preserving spaced responder CAN headers");

    {
        LinkObd2ResponderSampleList responders;
        response = parse_response(
            "010C", "7E804410C0CF9\r7E904410C0CFC\r>");
        check(link_obd2_decode_live_pid_responders(
                  &response, 0x0cU, &responders) == LINK_OBD2_RESULT_OK &&
                  responders.count == 2U && !responders.truncated,
              "decode both captured C207 RPM responders");
        check(responders.samples[0].responder_id_available &&
                  !responders.samples[0].extended_id &&
                  responders.samples[0].responder_id == 0x7e8U &&
                  responders.samples[0].sample.value == 830.25,
              "retain captured engine responder identity and RPM");
        check(responders.samples[1].responder_id_available &&
                  !responders.samples[1].extended_id &&
                  responders.samples[1].responder_id == 0x7e9U &&
                  responders.samples[1].sample.value == 831.0,
              "retain captured secondary responder identity and RPM");

        response = parse_response("010D", "410D5C\r>");
        check(link_obd2_decode_live_pid_responders(
                  &response, 0x0dU, &responders) == LINK_OBD2_RESULT_OK &&
                  responders.count == 1U &&
                  !responders.samples[0].responder_id_available &&
                  responders.samples[0].sample.value == 92.0,
              "retain one headerless OBD value without inventing a module");
    }

    {
        LinkObd2PidSet unionSet = {{0}};
        LinkObd2ResponderPidSetList responderSets = {0};
        const LinkObd2PidSet *engineSet;
        const LinkObd2PidSet *secondarySet;
        bool hasMore = false;

        response = parse_response(
            "0100",
            "7E8 06 41 00 98 3B A0 13\r"
            "7E9 06 41 00 98 18 00 01\r>");
        check(link_obd2_accept_supported_pid_responders(
                  &response, 0x00U, &unionSet, &responderSets, &hasMore) ==
                  LINK_OBD2_RESULT_OK && hasMore,
              "decode captured C207 0x00 capability block by responder");

        response = parse_response(
            "0120",
            "7E8 06 41 20 B0 03 A0 05\r"
            "7E9 06 41 20 80 01 80 01\r>");
        check(link_obd2_accept_supported_pid_responders(
                  &response, 0x20U, &unionSet, &responderSets, &hasMore) ==
                  LINK_OBD2_RESULT_OK && hasMore,
              "decode captured C207 0x20 capability block by responder");

        response = parse_response(
            "0140",
            "7E8 06 41 40 4C D8 00 00\r"
            "7E9 06 41 40 40 80 00 00\r>");
        check(link_obd2_accept_supported_pid_responders(
                  &response, 0x40U, &unionSet, &responderSets, &hasMore) ==
                  LINK_OBD2_RESULT_OK && !hasMore,
              "decode captured C207 0x40 capability block by responder");

        engineSet = link_obd2_responder_pid_set_find(
            &responderSets, 0x7e8U, false);
        secondarySet = link_obd2_responder_pid_set_find(
            &responderSets, 0x7e9U, false);
        check(engineSet != NULL && pid_count(engineSet) == 29U,
              "retain all 29 captured engine responder capability bits");
        check(secondarySet != NULL && pid_count(secondarySet) == 12U,
              "retain all 12 captured secondary responder capability bits");
        check(link_obd2_pid_set_contains(engineSet, 0x2fU) &&
                  link_obd2_pid_set_contains(engineSet, 0x46U) &&
                  link_obd2_pid_set_contains(engineSet, 0x4dU),
              "engine responder keeps later-block supported PIDs");
        check(!link_obd2_pid_set_contains(secondarySet, 0x2fU) &&
                  link_obd2_pid_set_contains(secondarySet, 0x42U) &&
                  link_obd2_pid_set_contains(secondarySet, 0x49U),
              "secondary responder capability set stays independent");
    }

    response = parse_response("010C", "7E804410C1AF8\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.value == 1726.0,
          "decode RPM while preserving unspaced 11-bit CAN header");

    response = parse_response("0111", "41117A\r>");
    check(link_obd2_decode_live_pid(&response, 0x11U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value > 47.84 && sample.value < 47.85,
          "decode absolute throttle valve near captured 48 percent");

    response = parse_response("0149", "414900\r>");
    check(link_obd2_decode_live_pid(&response, 0x49U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value == 0.0,
          "decode accelerator pedal D at rest");

    response = parse_response("014A", "414A80\r>");
    check(link_obd2_decode_live_pid(&response, 0x4aU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value > 50.19 && sample.value < 50.20,
          "decode accelerator pedal E");

    response = parse_response("014C", "414CFF\r>");
    check(link_obd2_decode_live_pid(&response, 0x4cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value == 100.0,
          "decode commanded throttle actuator");

    response = parse_response("0123", "41233039\r>");
    check(link_obd2_decode_live_pid(&response, 0x23U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_KPA &&
              sample.value == 123450.0,
          "keep fuel rail pressure canonical in kPa");

    response = parse_response("011F", "411F012C\r>");
    check(link_obd2_decode_live_pid(&response, 0x1fU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_SECONDS &&
              sample.value == 300.0,
          "decode engine run time");

    response = parse_response("0121", "41210064\r>");
    check(link_obd2_decode_live_pid(&response, 0x21U, &sample) ==
              LINK_OBD2_RESULT_OK &&
              sample.unit == LINK_OBD2_UNIT_KILOMETRES &&
              sample.value == 100.0,
          "decode distance with MIL on");

    response = parse_response("0124", "412440000000\r>");
    check(link_obd2_decode_live_pid(&response, 0x24U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RATIO &&
              sample.value == 0.5,
          "decode oxygen sensor 1 equivalence ratio");

    response = parse_response("0130", "413005\r>");
    check(link_obd2_decode_live_pid(&response, 0x30U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_COUNT &&
              sample.value == 5.0,
          "decode warm-ups since codes cleared");

    response = parse_response("0131", "413101F4\r>");
    check(link_obd2_decode_live_pid(&response, 0x31U, &sample) ==
              LINK_OBD2_RESULT_OK &&
              sample.unit == LINK_OBD2_UNIT_KILOMETRES &&
              sample.value == 500.0,
          "decode distance since codes cleared");

    response = parse_response("013E", "413E1234\r>");
    check(link_obd2_decode_live_pid(&response, 0x3eU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_CELSIUS,
          "decode catalyst temperature B1S2");

    response = parse_response("014D", "414D003C\r>");
    check(link_obd2_decode_live_pid(&response, 0x4dU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_MINUTES &&
              sample.value == 60.0,
          "decode time run with MIL on");

    response = parse_response("012F", "412F80\r>");
    check(link_obd2_decode_live_pid(&response, 0x2fU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value > 50.19 && sample.value < 50.20,
          "decode SAE fuel tank level input");

    response = parse_response("0106", "410680\r>");
    check(link_obd2_decode_live_pid(&response, 0x06U, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_PERCENT &&
              sample.value == 0.0,
          "decode newly catalogued short-term fuel trim");

    response = parse_response("01A6", "41A6000186A0\r>");
    check(link_obd2_decode_live_pid(&response, 0xa6U, &sample) ==
              LINK_OBD2_RESULT_OK &&
              sample.unit == LINK_OBD2_UNIT_KILOMETRES &&
              sample.value == 10000.0,
          "decode modern standard odometer through legacy scalar ABI");

    check(strcmp(link_obd2_pid_name(0x11U),
                 "Absolute throttle valve position") == 0,
          "distinguish throttle valve from accelerator pedal");
    check(strcmp(link_obd2_pid_name(0x49U),
                 "Accelerator pedal position D") == 0,
          "name accelerator pedal D");

    /* Captured C207 behaviour: optional permanent-DTC Mode 0A is unavailable. */
    response = parse_response("0A", "7F0A22\r>");
    check(link_obd2_is_negative_response(
              &response, UINT8_C(0x0a), &negative_response_code) &&
              negative_response_code == UINT8_C(0x22),
          "recognize captured Mode 0A negative response");
    check(!link_obd2_is_negative_response(
              &response, UINT8_C(0x07), &negative_response_code),
          "reject negative response for another service");

    response = parse_response("0A", "7F0A22\r7F0A22\r>");
    check(link_obd2_is_negative_response(
              &response, UINT8_C(0x0a), &negative_response_code),
          "recognize matching multi-ECU Mode 0A negative responses");

    response = parse_response("0A", "4A00\r7F0A22\r>");
    check(!link_obd2_is_negative_response(
              &response, UINT8_C(0x0a), &negative_response_code),
          "do not collapse mixed positive and negative traffic");

    memcpy(command, "sentinel", sizeof("sentinel"));
    check(link_obd2_build_clear_dtc_request(&authorization, command,
                                             sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "clear DTC deny by default");
    authorization.confirmed = true;
    authorization.acknowledge_readiness_reset = true;
    check(link_obd2_build_clear_dtc_request(&authorization, command,
                                             sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "04") == 0,
          "clear DTC explicit authorization");

    if (failures != 0) {
        fprintf(stderr, "%d OBD-II test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
