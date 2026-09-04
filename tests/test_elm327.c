// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/elm327.h"
#include "link/elm327_can.h"
#include "link/elm327_probe.h"
#include "link/elm327_session.h"
#include "link/elm327_simulator.h"
#include "link/obd2.h"
#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Requirement failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

typedef struct MockTransport {
    bool connected;
    LinkTransportReceiveFn receive;
    void *receive_context;
    uint8_t last_write[128];
    size_t last_write_size;
} MockTransport;

static LinkTransportStatus mock_connect(void *context) { ((MockTransport *)context)->connected = true; return LINK_TRANSPORT_OK; }
static void mock_disconnect(void *context) { ((MockTransport *)context)->connected = false; }
static bool mock_is_connected(void *context) { return ((MockTransport *)context)->connected; }
static LinkTransportStatus mock_write(void *context, const uint8_t *data, size_t size) { MockTransport *mock = context; REQUIRE(size <= sizeof(mock->last_write)); memcpy(mock->last_write, data, size); mock->last_write_size = size; return LINK_TRANSPORT_OK; }
static void mock_set_receiver(void *context, LinkTransportReceiveFn receive, void *receive_context) { MockTransport *mock = context; mock->receive = receive; mock->receive_context = receive_context; }

static bool simulator_custom_response(
    void *context,
    const char *command,
    char *response,
    size_t response_size)
{
    (void)context;
    if (strcmp(command, "22F190") != 0) return false;
    if (response_size < sizeof("62F1905744443230373330323246313233343536")) return false;
    memcpy(response,
           "62F1905744443230373330323246313233343536",
           sizeof("62F1905744443230373330323246313233343536"));
    return true;
}

int main(void)
{
    uint8_t command[16], pdu[8]; size_t written = 0U, consumed = 0U, pdu_length = 0U;
    LinkElm327Parser parser; LinkElm327Response response; LinkElm327ProbeState probe;
    LinkElm327CanChannelConfig can_config = { 0x7e0U, 0x7e8U, false };
    LinkElm327CanChannelState can_state; char can_command[32];
    MockTransport mock = { 0 }; LinkTransport transport = LINK_TRANSPORT_INIT; LinkElm327Session session;
    const uint8_t response_bytes[] = "010C\r41 0C 1A F8\r>";
    const LinkElm327ProtocolDefinition *protocol_definition;
    char protocol_command[8U];

    REQUIRE(link_elm327_build_command(" 010C ", command, sizeof(command), &written) == LINK_ELM327_RESULT_OK);
    REQUIRE(written == 5U && memcmp(command, "010C\r", 5U) == 0);
    REQUIRE(link_elm327_parser_begin(&parser, "010C") == LINK_ELM327_RESULT_OK);
    REQUIRE(link_elm327_parser_feed(&parser, response_bytes, sizeof(response_bytes) - 1U, &consumed) == LINK_ELM327_RESULT_OK);
    REQUIRE(link_elm327_parser_finish(&parser, &response) == LINK_ELM327_RESULT_OK);
    REQUIRE(response.echo_removed && strcmp(response.text, "41 0C 1A F8") == 0);

    REQUIRE(link_elm327_protocol_definition_count() == 13U);
    protocol_definition = link_elm327_protocol_definition(LINK_ELM327_PROTOCOL_SAE_J1850_PWM);
    REQUIRE(protocol_definition != NULL &&
            protocol_definition->family == LINK_ELM327_PROTOCOL_FAMILY_SAE_J1850 &&
            protocol_definition->bit_rate == 41600U &&
            protocol_definition->classic_j1979_obd);
    protocol_definition = link_elm327_protocol_definition(LINK_ELM327_PROTOCOL_ISO_9141_2);
    REQUIRE(protocol_definition != NULL &&
            protocol_definition->init == LINK_ELM327_PROTOCOL_INIT_FIVE_BAUD);
    protocol_definition = link_elm327_protocol_definition(LINK_ELM327_PROTOCOL_ISO_14230_4_FAST);
    REQUIRE(protocol_definition != NULL &&
            protocol_definition->init == LINK_ELM327_PROTOCOL_INIT_FAST);
    protocol_definition = link_elm327_protocol_definition(LINK_ELM327_PROTOCOL_ISO_15765_4_29_250);
    REQUIRE(protocol_definition != NULL &&
            protocol_definition->extended_can_id &&
            protocol_definition->bit_rate == 250000U);
    REQUIRE(link_elm327_build_set_protocol_command(
                LINK_ELM327_PROTOCOL_ISO_9141_2,
                protocol_command, sizeof(protocol_command)) == LINK_ELM327_RESULT_OK);
    REQUIRE(strcmp(protocol_command, "ATSP3") == 0);
    REQUIRE(link_elm327_build_set_protocol_command(
                LINK_ELM327_PROTOCOL_SAE_J1939,
                protocol_command, sizeof(protocol_command)) == LINK_ELM327_RESULT_OK);
    REQUIRE(strcmp(protocol_command, "ATSPA") == 0);

    link_elm327_probe_begin(&probe);
    REQUIRE(strcmp(link_elm327_probe_command(&probe), "AT@1") == 0);

    REQUIRE(link_elm327_can_channel_begin(&can_state, &can_config) == LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(link_elm327_can_channel_command(&can_state, can_command, sizeof(can_command)) == LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(strcmp(can_command, "ATSH7E0") == 0);
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    memcpy(response.text, "62 F1 90 31", sizeof("62 F1 90 31"));
    response.length = strlen(response.text);
    response.line_count = 1U;
    REQUIRE(link_elm327_can_decode_pdu(&response, pdu, sizeof(pdu), &pdu_length) == LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(pdu_length == 4U && pdu[0] == 0x62U && pdu[1] == 0xf1U);

    /*
     * C207/Vgate evidence: some indexed replies carry a three-hex-digit
     * preamble larger than the actual emitted payload.  The positive indexed
     * ECU payload must survive rather than being rejected as malformed.
     */
    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    memcpy(response.text, "011\n0:622001061A06",
           sizeof("011\n0:622001061A06"));
    response.length = strlen(response.text);
    response.line_count = 2U;
    REQUIRE(link_elm327_can_decode_pdu(
                &response, pdu, sizeof(pdu), &pdu_length) ==
            LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(pdu_length == 6U &&
            pdu[0] == 0x62U && pdu[1] == 0x20U &&
            pdu[2] == 0x01U && pdu[3] == 0x06U &&
            pdu[4] == 0x1aU && pdu[5] == 0x06U);

    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    memcpy(response.text, "012\n0:610110102210",
           sizeof("012\n0:610110102210"));
    response.length = strlen(response.text);
    response.line_count = 2U;
    REQUIRE(link_elm327_can_decode_pdu(
                &response, pdu, sizeof(pdu), &pdu_length) ==
            LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(pdu_length == 6U &&
            pdu[0] == 0x61U && pdu[1] == 0x01U &&
            pdu[2] == 0x10U && pdu[3] == 0x10U &&
            pdu[4] == 0x22U && pdu[5] == 0x10U);

    memset(&response, 0, sizeof(response));
    response.result = LINK_ELM327_RESULT_OK;
    memcpy(response.text, "7F1978\n5902FF", sizeof("7F1978\n5902FF"));
    response.length = strlen(response.text);
    response.line_count = 2U;
    REQUIRE(link_elm327_can_decode_pdu(&response, pdu, sizeof(pdu), &pdu_length) ==
            LINK_ELM327_CAN_RESULT_OK);
    REQUIRE(pdu_length == 3U && pdu[0] == 0x59U && pdu[1] == 0x02U && pdu[2] == 0xffU);

    transport.context = &mock; transport.connect = mock_connect; transport.disconnect = mock_disconnect; transport.is_connected = mock_is_connected; transport.write = mock_write; transport.set_receiver = mock_set_receiver;
    REQUIRE(link_transport_is_valid(&transport));
    REQUIRE(link_elm327_session_init(&session, &transport, NULL, NULL));
    REQUIRE(link_elm327_session_connect(&session) == LINK_TRANSPORT_OK);
    REQUIRE(link_elm327_session_begin(&session, "010C", 100U, 500U) == LINK_ELM327_SESSION_OP_OK);
    REQUIRE(mock.last_write_size == 5U);
    REQUIRE(mock.receive != NULL);
    mock.receive(mock.receive_context, response_bytes, sizeof(response_bytes) - 1U);
    REQUIRE(session.status == LINK_ELM327_SESSION_COMPLETE);
    REQUIRE(link_elm327_session_response(&session) != NULL);
    link_elm327_session_deinit(&session);

    {
        LinkElm327Simulator simulator;
        LinkElm327SimulatorConfig config = LINK_ELM327_SIMULATOR_CONFIG_INIT;
        LinkTransport simulated_transport;
        LinkElm327Session simulated_session;
        const LinkElm327Response *simulated_response;
        LinkObd2PidSet supported;
        LinkObd2Sample sample;
        LinkObd2DtcList dtcs;
        LinkObd2Readiness readiness;
        bool has_more = false;

        config.adapter_identifier = "ELM327 v2.3 LINK TEST";
        config.vin = "WDD2073022F123456";
        config.custom_responder = simulator_custom_response;
        link_elm327_simulator_init(&simulator, &config);
        simulated_transport = link_elm327_simulator_transport(&simulator);
        REQUIRE(link_transport_is_valid(&simulated_transport));
        REQUIRE(link_elm327_session_init(&simulated_session, &simulated_transport, NULL, NULL));
        REQUIRE(link_elm327_session_connect(&simulated_session) == LINK_TRANSPORT_OK);

        REQUIRE(link_elm327_session_begin(&simulated_session, "ATZ", 1000U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        REQUIRE(strcmp(simulated_response->text, "ELM327 v2.3 LINK TEST") == 0);
        REQUIRE(simulated_response->echo_removed);

        REQUIRE(link_elm327_session_begin(&simulated_session, "ATE0", 1100U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL && simulated_response->ok_seen);

        REQUIRE(link_elm327_session_begin(&simulated_session, "0100", 1200U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        link_obd2_pid_set_clear(&supported);
        REQUIRE(link_obd2_accept_supported_pids(simulated_response, 0x00U, &supported, &has_more) == LINK_OBD2_RESULT_OK);
        REQUIRE(has_more);
        REQUIRE(link_obd2_pid_set_contains(&supported, 0x01U));
        REQUIRE(link_obd2_pid_set_contains(&supported, 0x0cU));

        /*
         * The shared diagnostic flow always requests Mode 01 PID 01 after
         * its DTC inventory. The simulator must return a valid readiness
         * payload or every product using simulated data fails before live mode.
         */
        REQUIRE(link_elm327_session_begin(&simulated_session, "0101", 1250U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        REQUIRE(link_obd2_decode_readiness(simulated_response, &readiness) == LINK_OBD2_RESULT_OK);
        REQUIRE(readiness.mil_on);
        REQUIRE(readiness.confirmed_dtc_count == 1U);
        REQUIRE(readiness.compression_ignition);

        REQUIRE(link_elm327_session_begin(&simulated_session, "010C", 1300U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        REQUIRE(link_obd2_decode_live_pid(simulated_response, 0x0cU, &sample) == LINK_OBD2_RESULT_OK);
        REQUIRE(sample.unit == LINK_OBD2_UNIT_RPM && sample.value > 0.0);

        REQUIRE(link_elm327_session_begin(&simulated_session, "03", 1400U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        REQUIRE(link_obd2_decode_dtcs(simulated_response, LINK_OBD2_DTC_STORED, &dtcs) == LINK_OBD2_RESULT_OK);
        REQUIRE(dtcs.count == 2U);
        REQUIRE(strcmp(dtcs.entries[0].code, "P0401") == 0);

        REQUIRE(link_elm327_session_begin(&simulated_session, "22F190", 1500U, 500U) == LINK_ELM327_SESSION_OP_OK);
        simulated_response = link_elm327_session_response(&simulated_session);
        REQUIRE(simulated_response != NULL);
        REQUIRE(strcmp(simulated_response->text,
                       "62F1905744443230373330323246313233343536") == 0);

        link_elm327_session_deinit(&simulated_session);
    }
    return 0;
}
