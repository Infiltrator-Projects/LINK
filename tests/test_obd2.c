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

    CHECK(link_obd2_decode_vin_pdu(pdu, sizeof(pdu), vin) ==
          LINK_OBD2_RESULT_OK);
    CHECK(strcmp(vin, "SAJAD56L64WD78435") == 0);
    CHECK(link_obd2_decode_vin_pdu(pdu, sizeof(pdu) - 1U, vin) ==
          LINK_OBD2_RESULT_UNEXPECTED_RESPONSE);

    memcpy(bad, pdu, sizeof(bad));
    bad[3U] = (uint8_t)'I';
    CHECK(link_obd2_decode_vin_pdu(bad, sizeof(bad), vin) ==
          LINK_OBD2_RESULT_MALFORMED_RESPONSE);
    CHECK(vin[0] == '\0');

    bad[0] = 0x48U;
    CHECK(link_obd2_decode_vin_pdu(bad, sizeof(bad), vin) ==
          LINK_OBD2_RESULT_UNEXPECTED_RESPONSE);
    return 0;
}

int main(void)
{
    if (test_raw_vin_pdu() != 0) return 1;
    char command[16];
    LinkObd2Sample sample;
    LinkObd2ClearAuthorization authorization = LINK_OBD2_CLEAR_AUTHORIZATION_INIT;
    LinkElm327Response response;

    check(link_obd2_build_live_pid_request(0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "build RPM request");

    response = parse_response("010C", "410C1AF8\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RPM &&
              sample.value == 1726.0,
          "decode RPM");

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
