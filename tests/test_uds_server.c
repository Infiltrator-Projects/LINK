// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do {     if (!(c)) {         fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c);         return 1;     } } while (0)

typedef struct {
    uint32_t now_ms;
} TestClock;

static uint32_t test_clock_ms(void *context)
{
    return ((TestClock *)context)->now_ms;
}

static LinkUdsServerHandlerResult read_did_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    const char *value = (const char *)context;
    size_t length = strlen(value);

    if (request->pdu_length != 3U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    if (request->pdu[1] != 0xf1U || request->pdu[2] != 0x90U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
    }
    if (capacity < length + 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }

    data[0] = 0xf1U;
    data[1] = 0x90U;
    memcpy(data + 2U, value, length);
    return link_uds_server_handler_positive(length + 2U);
}

static LinkUdsServerHandlerResult programming_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    (void)context;

    if (request->service != LINK_UDS_SERVICE_REQUEST_DOWNLOAD ||
        capacity < 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_REJECT);
    }
    data[0] = 0x20U;
    data[1] = 0x10U;
    return link_uds_server_handler_positive(2U);
}

static int test_session_and_tester_present(void)
{
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[64U];
    size_t length = 0U;
    const uint8_t session[] = {0x10U, 0x01U};
    const uint8_t suppressed[] = {0x10U, 0x83U};
    const uint8_t tester[] = {0x3eU, 0x00U};
    const uint8_t expected_session[] = {
        0x50U, 0x01U, 0x00U, 0x32U, 0x01U, 0xf4U
    };

    CHECK(link_uds_server_init(&server, &config));
    CHECK(link_uds_server_handle(
              &server, session, sizeof(session),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == sizeof(expected_session));
    CHECK(memcmp(response, expected_session, sizeof(expected_session)) == 0);
    CHECK(link_uds_server_active_session(&server) ==
          LINK_UDS_SESSION_DEFAULT);

    CHECK(link_uds_server_handle(
              &server, suppressed, sizeof(suppressed),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_SUPPRESSED);
    CHECK(length == 0U);
    CHECK(link_uds_server_active_session(&server) ==
          LINK_UDS_SESSION_EXTENDED);

    CHECK(link_uds_server_handle(
              &server, tester, sizeof(tester),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 2U);
    CHECK(response[0] == 0x7eU && response[1] == 0x00U);
    return 0;
}

static int test_session_state_machine_and_ecu_reset(void)
{
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    TestClock clock = {0U};
    uint8_t response[16U];
    uint8_t reset_type = 0U;
    size_t length = 0U;
    const uint8_t programming[] = {0x10U, 0x02U};
    const uint8_t extended[] = {0x10U, 0x03U};
    const uint8_t default_session[] = {0x10U, 0x01U};
    const uint8_t reset[] = {0x11U, LINK_UDS_ECU_RESET_HARD};
    const uint8_t invalid_reset[] = {0x11U, 0x06U};

    config.enforce_session_sequence = true;
    config.s3_server_timeout_ms = 5000U;
    config.clock_ms = test_clock_ms;
    config.clock_context = &clock;

    CHECK(link_uds_server_init(&server, &config));
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_DEFAULT);

    /* Programming cannot be entered directly from Default. */
    CHECK(link_uds_server_handle(
              &server, programming, sizeof(programming),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(response[0] == 0x7fU && response[1] == 0x10U &&
          response[2] ==
              LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_DEFAULT);

    /* Default -> Extended -> Programming is permitted. */
    CHECK(link_uds_server_handle(
              &server, extended, sizeof(extended),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_EXTENDED);
    CHECK(link_uds_server_handle(
              &server, programming, sizeof(programming),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(link_uds_server_active_session(&server) ==
          LINK_UDS_SESSION_PROGRAMMING);

    /* Programming can leave only through Default. */
    CHECK(link_uds_server_handle(
              &server, extended, sizeof(extended),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(link_uds_server_active_session(&server) ==
          LINK_UDS_SESSION_PROGRAMMING);
    CHECK(link_uds_server_handle(
              &server, default_session, sizeof(default_session),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_DEFAULT);

    /* S3 timeout restores Default session. */
    CHECK(link_uds_server_handle(
              &server, extended, sizeof(extended),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    clock.now_ms = 5001U;
    link_uds_server_tick(&server);
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_DEFAULT);

    /* ECUReset returns 0x51, resets the session and defers platform action. */
    CHECK(link_uds_server_handle(
              &server, reset, sizeof(reset),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 2U);
    CHECK(response[0] == 0x51U && response[1] == LINK_UDS_ECU_RESET_HARD);
    CHECK(link_uds_server_active_session(&server) == LINK_UDS_SESSION_DEFAULT);
    CHECK(link_uds_server_take_pending_ecu_reset(&server, &reset_type));
    CHECK(reset_type == LINK_UDS_ECU_RESET_HARD);
    CHECK(!link_uds_server_take_pending_ecu_reset(&server, &reset_type));

    CHECK(link_uds_server_handle(
              &server, invalid_reset, sizeof(invalid_reset),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(response[2] == LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    return 0;
}

static int test_custom_handlers(void)
{
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[64U];
    size_t length = 0U;
    const uint8_t did[] = {0x22U, 0xf1U, 0x90U};
    const uint8_t download[] = {0x34U, 0x00U, 0x11U, 0x00U, 0x10U};

    CHECK(link_uds_server_init(&server, &config));
    CHECK(link_uds_server_set_handler(
        &server, 0x22U, read_did_handler,
        (void *)"12345678901234567"));
    CHECK(link_uds_server_set_handler(
        &server, 0x34U, programming_handler, NULL));

    CHECK(link_uds_server_handle(
              &server, did, sizeof(did),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 20U);
    CHECK(response[0] == 0x62U &&
          response[1] == 0xf1U &&
          response[2] == 0x90U);

    CHECK(link_uds_server_handle(
              &server, download, sizeof(download),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 3U);
    CHECK(response[0] == 0x74U &&
          response[1] == 0x20U &&
          response[2] == 0x10U);
    return 0;
}

static int test_dtc_all_subfunctions(void)
{
    static const LinkUdsDtcRecord records[] = {
        {
            UINT32_C(0x123456),
            LINK_UDS_DTC_STATUS_TEST_FAILED |
                LINK_UDS_DTC_STATUS_CONFIRMED_DTC
        },
        {
            UINT32_C(0xabcdef),
            LINK_UDS_DTC_STATUS_CONFIRMED_DTC
        }
    };
    LinkUdsServerDtcStore store = {
        records, 2U, LINK_UDS_DTC_STATUS_MASK_ALL, 0xffU, 0x01U
    };
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[256U];
    size_t length = 0U;
    size_t index;
    const uint8_t r01[] = {0x19U, 0x01U, 0xffU};
    const uint8_t r02[] = {0x19U, 0x02U, 0xffU};
    const uint8_t r03[] = {0x19U, 0x03U};
    const uint8_t r04[] = {0x19U, 0x04U, 0x12U, 0x34U, 0x56U, 0x01U};
    const uint8_t r05[] = {0x19U, 0x05U, 0x01U};
    const uint8_t r06[] = {0x19U, 0x06U, 0x12U, 0x34U, 0x56U, 0x01U};
    const uint8_t r07[] = {0x19U, 0x07U, 0xffU, 0xffU};
    const uint8_t r08[] = {0x19U, 0x08U, 0xffU, 0xffU};
    const uint8_t r09[] = {0x19U, 0x09U, 0x12U, 0x34U, 0x56U};
    const uint8_t r0a[] = {0x19U, 0x0aU};
    const uint8_t r0b[] = {0x19U, 0x0bU};
    const uint8_t r0c[] = {0x19U, 0x0cU};
    const uint8_t r0d[] = {0x19U, 0x0dU};
    const uint8_t r0e[] = {0x19U, 0x0eU};
    const uint8_t r0f[] = {0x19U, 0x0fU, 0xffU};
    const uint8_t r10[] = {0x19U, 0x10U, 0x12U, 0x34U, 0x56U, 0x01U};
    const uint8_t r11[] = {0x19U, 0x11U, 0xffU};
    const uint8_t r12[] = {0x19U, 0x12U, 0xffU};
    const uint8_t r13[] = {0x19U, 0x13U, 0xffU};
    const uint8_t r14[] = {0x19U, 0x14U};
    const uint8_t r15[] = {0x19U, 0x15U};
    const uint8_t r16[] = {0x19U, 0x16U, 0x01U};
    const uint8_t r17[] = {0x19U, 0x17U, 0xffU, 0x01U};
    const uint8_t r18[] = {
        0x19U, 0x18U, 0x12U, 0x34U, 0x56U, 0x01U, 0x01U
    };
    const uint8_t r19[] = {
        0x19U, 0x19U, 0x12U, 0x34U, 0x56U, 0x01U, 0x01U
    };
    const uint8_t r42[] = {0x19U, 0x42U, 0x01U, 0xffU, 0xffU};
    const uint8_t r55[] = {0x19U, 0x55U, 0x01U};
    struct DtcCase {
        const uint8_t *pdu;
        size_t length;
    };
    const struct DtcCase cases[] = {
        {r01, sizeof(r01)}, {r02, sizeof(r02)}, {r03, sizeof(r03)},
        {r04, sizeof(r04)}, {r05, sizeof(r05)}, {r06, sizeof(r06)},
        {r07, sizeof(r07)}, {r08, sizeof(r08)}, {r09, sizeof(r09)},
        {r0a, sizeof(r0a)}, {r0b, sizeof(r0b)}, {r0c, sizeof(r0c)},
        {r0d, sizeof(r0d)}, {r0e, sizeof(r0e)}, {r0f, sizeof(r0f)},
        {r10, sizeof(r10)}, {r11, sizeof(r11)}, {r12, sizeof(r12)},
        {r13, sizeof(r13)}, {r14, sizeof(r14)}, {r15, sizeof(r15)},
        {r16, sizeof(r16)}, {r17, sizeof(r17)}, {r18, sizeof(r18)},
        {r19, sizeof(r19)}, {r42, sizeof(r42)}, {r55, sizeof(r55)}
    };
    const uint8_t expected_02[] = {
        0x59U, 0x02U, 0xffU,
        0x12U, 0x34U, 0x56U, 0x09U,
        0xabU, 0xcdU, 0xefU, 0x08U
    };

    CHECK(link_uds_server_init(&server, &config));
    CHECK(link_uds_server_set_handler(
        &server, 0x19U, link_uds_server_dtc_handler, &store));

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        LinkUdsServerResult result = link_uds_server_handle(
            &server, cases[index].pdu, cases[index].length,
            response, sizeof(response), &length);
        CHECK(result == LINK_UDS_SERVER_RESULT_POSITIVE ||
              result == LINK_UDS_SERVER_RESULT_NEGATIVE);
        if (result == LINK_UDS_SERVER_RESULT_POSITIVE) {
            CHECK(length >= 2U);
            CHECK(response[0] == 0x59U);
            CHECK((response[1] & 0x7fU) ==
                  (cases[index].pdu[1] & 0x7fU));
        } else {
            CHECK(length == 3U);
            CHECK(response[0] == 0x7fU && response[1] == 0x19U);
            CHECK(response[2] != LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
    }

    CHECK(link_uds_server_handle(
              &server, r02, sizeof(r02),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == sizeof(expected_02));
    CHECK(memcmp(response, expected_02, sizeof(expected_02)) == 0);
    return 0;
}

static int test_negative_paths(void)
{
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[8U];
    size_t length = 0U;
    const uint8_t unknown[] = {0x99U};
    const uint8_t short_session[] = {0x10U};

    CHECK(link_uds_server_init(&server, &config));
    CHECK(link_uds_server_handle(
              &server, unknown, sizeof(unknown),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(length == 3U);
    CHECK(response[0] == 0x7fU &&
          response[1] == 0x99U &&
          response[2] == 0x11U);

    CHECK(link_uds_server_handle(
              &server, short_session, sizeof(short_session),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(response[2] == 0x13U);
    return 0;
}

int main(void)
{
    if (test_session_and_tester_present() != 0) return EXIT_FAILURE;
    if (test_session_state_machine_and_ecu_reset() != 0) return EXIT_FAILURE;
    if (test_custom_handlers() != 0) return EXIT_FAILURE;
    if (test_dtc_all_subfunctions() != 0) return EXIT_FAILURE;
    if (test_negative_paths() != 0) return EXIT_FAILURE;
    puts("uds server tests passed");
    return EXIT_SUCCESS;
}
