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

static int test_ecu_reset_semantics(void)
{
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[16U];
    uint8_t pending = 0U;
    size_t length = 0U;
    const uint8_t hard[] = {0x11U, 0x01U};
    const uint8_t key_cycle[] = {0x11U, 0x02U};
    const uint8_t soft[] = {0x11U, 0x03U};
    const uint8_t rapid_on[] = {0x11U, 0x04U};
    const uint8_t rapid_off[] = {0x11U, 0x05U};

    /*
     * This target advertises hard + soft reset, but not key-off/on. Rapid
     * shutdown is enabled separately with a configured one-byte powerDownTime.
     */
    config.supported_ecu_reset_types =
        LINK_UDS_ECU_RESET_SUPPORT_HARD |
        LINK_UDS_ECU_RESET_SUPPORT_SOFT;
    config.rapid_power_shutdown_supported = true;
    config.rapid_power_shutdown_time_seconds = UINT8_C(5);
    CHECK(link_uds_server_init(&server, &config));

    CHECK(link_uds_server_handle(
              &server, hard, sizeof(hard),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 2U && response[0] == 0x51U && response[1] == 0x01U);
    CHECK(link_uds_server_take_pending_ecu_reset(&server, &pending));
    CHECK(pending == 0x01U);

    CHECK(link_uds_server_handle(
              &server, key_cycle, sizeof(key_cycle),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(length == 3U && response[0] == 0x7fU &&
          response[1] == 0x11U &&
          response[2] == LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    CHECK(!link_uds_server_take_pending_ecu_reset(&server, &pending));

    CHECK(link_uds_server_handle(
              &server, soft, sizeof(soft),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 2U && response[0] == 0x51U && response[1] == 0x03U);
    CHECK(link_uds_server_take_pending_ecu_reset(&server, &pending));
    CHECK(pending == 0x03U);

    CHECK(!link_uds_server_rapid_power_shutdown_enabled(&server));
    CHECK(link_uds_server_handle(
              &server, rapid_on, sizeof(rapid_on),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 3U && response[0] == 0x51U &&
          response[1] == 0x04U && response[2] == 0x05U);
    CHECK(link_uds_server_rapid_power_shutdown_enabled(&server));
    CHECK(!link_uds_server_take_pending_ecu_reset(&server, &pending));

    CHECK(link_uds_server_handle(
              &server, rapid_off, sizeof(rapid_off),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 2U && response[0] == 0x51U && response[1] == 0x05U);
    CHECK(!link_uds_server_rapid_power_shutdown_enabled(&server));
    CHECK(!link_uds_server_take_pending_ecu_reset(&server, &pending));

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
        records, 2U, LINK_UDS_DTC_STATUS_MASK_ALL, 0xffU, 0x01U,
        NULL, 0U, 0x04U
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
            LinkUdsDtcInformationResponse decoded;
            CHECK(length >= 2U);
            CHECK(response[0] == 0x59U);
            CHECK((response[1] & 0x7fU) ==
                  (cases[index].pdu[1] & 0x7fU));
            CHECK(link_uds_decode_read_dtc_information_response(
                      (uint8_t)(cases[index].pdu[1] & 0x7fU),
                      response, length, &decoded) == LINK_UDS_RESULT_OK);
        } else {
            CHECK(length == 3U);
            CHECK(response[0] == 0x7fU && response[1] == 0x19U);
            CHECK(response[2] == LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
        }
    }

    CHECK(link_uds_server_handle(
              &server, r02, sizeof(r02),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == sizeof(expected_02));
    CHECK(memcmp(response, expected_02, sizeof(expected_02)) == 0);

    CHECK(link_uds_server_handle(
              &server, r0c, sizeof(r0c),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U);
    CHECK(response[2] == LINK_UDS_DTC_STATUS_MASK_ALL);
    CHECK(response[3] == 0x12U && response[4] == 0x34U &&
          response[5] == 0x56U);

    CHECK(link_uds_server_handle(
              &server, r0e, sizeof(r0e),
              response, sizeof(response), &length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U);
    CHECK(response[2] == LINK_UDS_DTC_STATUS_MASK_ALL);
    CHECK(response[3] == 0xabU && response[4] == 0xcdU &&
          response[5] == 0xefU);
    return 0;
}

static int test_dtc_rich_all_subfunctions(void)
{
    static const LinkUdsDtcRecord records[] = {
        { UINT32_C(0x123456),
          LINK_UDS_DTC_STATUS_TEST_FAILED |
          LINK_UDS_DTC_STATUS_CONFIRMED_DTC },
        { UINT32_C(0xabcdef),
          LINK_UDS_DTC_STATUS_CONFIRMED_DTC }
    };
    static const uint8_t snapshot_1[] = {0x12U,0x34U,0x56U,0x78U};
    static const uint8_t snapshot_2[] = {0x12U,0x35U,0x9aU};
    static const uint8_t stored_1[] = {0x22U,0x01U,0x55U};
    static const uint8_t stored_2[] = {0x22U,0x02U,0x66U};
    static const uint8_t ext_1[] = {0x05U,0x09U};
    static const uint8_t ext_2[] = {0x03U,0x08U};
    static const LinkUdsServerDtcDetail details[] = {
        {
            UINT32_C(0x123456),0x20U,0x01U,0x20U,1U,1U,
            true,true,true,0x33U,0x01U,
            0x01U,0x01U,snapshot_1,sizeof(snapshot_1),
            0x01U,0x01U,stored_1,sizeof(stored_1),
            0x01U,ext_1,sizeof(ext_1)
        },
        {
            UINT32_C(0xabcdef),0x40U,0x02U,0x10U,0U,2U,
            true,true,false,0x33U,0x01U,
            0x01U,0x01U,snapshot_2,sizeof(snapshot_2),
            0x01U,0x01U,stored_2,sizeof(stored_2),
            0x01U,ext_2,sizeof(ext_2)
        }
    };
    LinkUdsServerDtcStore store = {
        records,2U,LINK_UDS_DTC_STATUS_MASK_ALL,0xffU,0x01U,
        details,2U,0x04U
    };
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[256U];
    size_t length = 0U;
    size_t index;
    static const uint8_t requests[][7] = {
        {0x19U,0x01U,0xffU},
        {0x19U,0x02U,0xffU},
        {0x19U,0x03U},
        {0x19U,0x04U,0x12U,0x34U,0x56U,0x01U},
        {0x19U,0x05U,0x01U},
        {0x19U,0x06U,0x12U,0x34U,0x56U,0x01U},
        {0x19U,0x07U,0xffU,0xffU},
        {0x19U,0x08U,0xffU,0xffU},
        {0x19U,0x09U,0x12U,0x34U,0x56U},
        {0x19U,0x0aU},
        {0x19U,0x0bU},
        {0x19U,0x0cU},
        {0x19U,0x0dU},
        {0x19U,0x0eU},
        {0x19U,0x0fU,0xffU},
        {0x19U,0x10U,0x12U,0x34U,0x56U,0x01U},
        {0x19U,0x11U,0xffU},
        {0x19U,0x12U,0xffU},
        {0x19U,0x13U,0xffU},
        {0x19U,0x14U},
        {0x19U,0x15U},
        {0x19U,0x16U,0x01U},
        {0x19U,0x17U,0xffU,0x01U},
        {0x19U,0x18U,0x12U,0x34U,0x56U,0x01U,0x01U},
        {0x19U,0x19U,0x12U,0x34U,0x56U,0x01U,0x01U},
        {0x19U,0x42U,0x33U,0x09U,0x20U},
        {0x19U,0x55U,0x33U}
    };
    static const uint8_t lengths[] = {
        3U,3U,2U,6U,3U,6U,4U,4U,5U,
        2U,2U,2U,2U,2U,3U,6U,3U,3U,
        3U,2U,2U,3U,4U,7U,7U,5U,3U
    };

    CHECK(link_uds_server_init(&server,&config));
    CHECK(link_uds_server_set_handler(
        &server,0x19U,link_uds_server_dtc_handler,&store));

    for(index=0U;index<sizeof(lengths);++index){
        LinkUdsDtcInformationResponse decoded;
        CHECK(link_uds_server_handle(
                  &server,requests[index],lengths[index],
                  response,sizeof(response),&length) ==
              LINK_UDS_SERVER_RESULT_POSITIVE);
        CHECK(length >= 2U);
        CHECK(response[0] == 0x59U && response[1] == requests[index][1]);
        CHECK(link_uds_decode_read_dtc_information_response(
                  requests[index][1],response,length,&decoded) ==
              LINK_UDS_RESULT_OK);
    }

    CHECK(link_uds_server_handle(
              &server,requests[2],lengths[2],
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 10U);
    CHECK(response[0] == 0x59U && response[1] == 0x03U);
    CHECK(response[2] == 0x12U && response[3] == 0x34U &&
          response[4] == 0x56U && response[5] == 0x01U);

    CHECK(link_uds_server_handle(
              &server,requests[3],lengths[3],
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(response[0] == 0x59U && response[1] == 0x04U);
    CHECK(response[2] == 0x12U && response[3] == 0x34U &&
          response[4] == 0x56U && response[5] == 0x09U);
    CHECK(response[6] == 0x01U && response[7] == 0x01U);

    CHECK(link_uds_server_handle(
              &server,requests[7],lengths[7],
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(response[0] == 0x59U && response[1] == 0x08U &&
          response[2] == 0xffU);
    CHECK(response[3] == 0x20U && response[4] == 0x01U &&
          response[5] == 0x12U && response[6] == 0x34U &&
          response[7] == 0x56U && response[8] == 0x09U);

    CHECK(link_uds_server_handle(
              &server,requests[25],lengths[25],
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 11U);
    CHECK(response[0] == 0x59U && response[1] == 0x42U &&
          response[2] == 0x33U && response[3] == 0xffU &&
          response[4] == 0xffU && response[5] == 0x04U);
    CHECK(response[6] == 0x20U && response[7] == 0x12U &&
          response[8] == 0x34U && response[9] == 0x56U &&
          response[10] == 0x09U);
    return 0;
}

static int test_dtc_empty_supported_and_history(void)
{
    static const LinkUdsDtcRecord records[] = {
        { UINT32_C(0x111111), UINT8_C(0x00) },
        { UINT32_C(0x222222), LINK_UDS_DTC_STATUS_CONFIRMED_DTC }
    };
    static const uint8_t obd_ext[] = { UINT8_C(0xaa) };
    static const LinkUdsServerDtcDetail details[] = {
        {
            UINT32_C(0x111111),0x20U,0x01U,0x20U,1U,1U,
            true,true,true,0x33U,0x01U,
            0x01U,0U,NULL,0U,
            0x01U,0U,NULL,0U,
            0x01U,NULL,0U
        },
        {
            UINT32_C(0x222222),0x40U,0x02U,0U,2U,2U,
            false,true,false,0x33U,0x01U,
            0U,0U,NULL,0U,
            0U,0U,NULL,0U,
            0x90U,obd_ext,sizeof(obd_ext)
        }
    };
    LinkUdsServerDtcStore store = {
        records,2U,LINK_UDS_DTC_STATUS_MASK_ALL,0xffU,0x01U,
        details,2U,0x04U
    };
    LinkUdsServer server;
    LinkUdsServerConfig config = LINK_UDS_SERVER_CONFIG_INIT;
    uint8_t response[64U];
    size_t length = 0U;
    const uint8_t r0a[] = {0x19U,0x0aU};
    const uint8_t r15[] = {0x19U,0x15U};
    const uint8_t r0b[] = {0x19U,0x0bU};
    const uint8_t r04[] = {0x19U,0x04U,0x11U,0x11U,0x11U,0x01U};
    const uint8_t r05[] = {0x19U,0x05U,0x01U};
    const uint8_t r06[] = {0x19U,0x06U,0x11U,0x11U,0x11U,0x01U};
    const uint8_t r10[] = {0x19U,0x10U,0x11U,0x11U,0x11U,0x01U};
    const uint8_t r16[] = {0x19U,0x16U,0x01U};
    const uint8_t r18[] = {
        0x19U,0x18U,0x11U,0x11U,0x11U,0x01U,0x01U
    };
    const uint8_t r19[] = {
        0x19U,0x19U,0x11U,0x11U,0x11U,0x01U,0x01U
    };
    const uint8_t r06_fe_non_obd[] = {
        0x19U,0x06U,0x11U,0x11U,0x11U,0xfeU
    };
    const uint8_t r06_fe_obd[] = {
        0x19U,0x06U,0x22U,0x22U,0x22U,0xfeU
    };

    CHECK(link_uds_server_init(&server,&config));
    CHECK(link_uds_server_set_handler(
        &server,0x19U,link_uds_server_dtc_handler,&store));

    /* reportSupportedDTC includes supported DTCs even at status 0x00. */
    CHECK(link_uds_server_handle(
              &server,r0a,sizeof(r0a),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 11U);
    CHECK(response[0] == 0x59U && response[1] == 0x0aU &&
          response[2] == 0xffU);
    CHECK(response[3] == 0x11U && response[4] == 0x11U &&
          response[5] == 0x11U && response[6] == 0x00U);

    /* Permanent status is independent of current ordinary status bits. */
    CHECK(link_uds_server_handle(
              &server,r15,sizeof(r15),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U);
    CHECK(response[3] == 0x11U && response[4] == 0x11U &&
          response[5] == 0x11U && response[6] == 0x00U);

    /* Historical first-failed information survives current status healing. */
    CHECK(link_uds_server_handle(
              &server,r0b,sizeof(r0b),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U);
    CHECK(response[3] == 0x11U && response[4] == 0x11U &&
          response[5] == 0x11U && response[6] == 0x00U);

    /* Supported snapshot/extended records with no data stay positive. */
    CHECK(link_uds_server_handle(
              &server,r04,sizeof(r04),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 6U && response[1] == 0x04U &&
          response[2] == 0x11U && response[5] == 0x00U);

    CHECK(link_uds_server_handle(
              &server,r05,sizeof(r05),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 3U && response[0] == 0x59U &&
          response[1] == 0x05U && response[2] == 0x01U);

    CHECK(link_uds_server_handle(
              &server,r06,sizeof(r06),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 6U && response[1] == 0x06U &&
          response[2] == 0x11U && response[5] == 0x00U);

    CHECK(link_uds_server_handle(
              &server,r10,sizeof(r10),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 6U && response[1] == 0x10U);

    CHECK(link_uds_server_handle(
              &server,r16,sizeof(r16),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 3U && response[1] == 0x16U &&
          response[2] == 0x01U);

    CHECK(link_uds_server_handle(
              &server,r18,sizeof(r18),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U && response[1] == 0x18U &&
          response[2] == 0x01U && response[6] == 0x00U);

    CHECK(link_uds_server_handle(
              &server,r19,sizeof(r19),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 7U && response[1] == 0x19U &&
          response[2] == 0x01U && response[6] == 0x00U);

    /*
     * 0xFE means all OBD extended records (0x90..0xEF), not every
     * extended-data record. Ordinary 0x01 must not match it.
     */
    CHECK(link_uds_server_handle(
              &server,r06_fe_non_obd,sizeof(r06_fe_non_obd),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(length == 3U && response[0] == 0x7fU &&
          response[1] == 0x19U &&
          response[2] == LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    CHECK(link_uds_server_handle(
              &server,r06_fe_obd,sizeof(r06_fe_obd),
              response,sizeof(response),&length) ==
          LINK_UDS_SERVER_RESULT_POSITIVE);
    CHECK(length == 8U && response[0] == 0x59U &&
          response[1] == 0x06U &&
          response[2] == 0x22U && response[3] == 0x22U &&
          response[4] == 0x22U && response[5] == 0x08U &&
          response[6] == 0x90U && response[7] == 0xaaU);

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
    if (test_ecu_reset_semantics() != 0) return EXIT_FAILURE;
    if (test_custom_handlers() != 0) return EXIT_FAILURE;
    if (test_dtc_all_subfunctions() != 0) return EXIT_FAILURE;
    if (test_dtc_rich_all_subfunctions() != 0) return EXIT_FAILURE;
    if (test_dtc_empty_supported_and_history() != 0) return EXIT_FAILURE;
    if (test_negative_paths() != 0) return EXIT_FAILURE;
    puts("uds server tests passed");
    return EXIT_SUCCESS;
}
