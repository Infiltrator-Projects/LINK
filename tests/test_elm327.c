// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/elm327.h"
#include "link/elm327_can.h"
#include "link/elm327_probe.h"
#include "link/elm327_session.h"
#include <assert.h>
#include <string.h>

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
static LinkTransportStatus mock_write(void *context, const uint8_t *data, size_t size) { MockTransport *mock = context; assert(size <= sizeof(mock->last_write)); memcpy(mock->last_write, data, size); mock->last_write_size = size; return LINK_TRANSPORT_OK; }
static void mock_set_receiver(void *context, LinkTransportReceiveFn receive, void *receive_context) { MockTransport *mock = context; mock->receive = receive; mock->receive_context = receive_context; }

int main(void)
{
    uint8_t command[16], pdu[8]; size_t written = 0U, consumed = 0U, pdu_length = 0U;
    LinkElm327Parser parser; LinkElm327Response response; LinkElm327ProbeState probe;
    LinkElm327CanChannelConfig can_config = { 0x7e0U, 0x7e8U, false };
    LinkElm327CanChannelState can_state; char can_command[32];
    MockTransport mock = { 0 }; LinkTransport transport = LINK_TRANSPORT_INIT; LinkElm327Session session;
    const uint8_t response_bytes[] = "010C\r41 0C 1A F8\r>";

    assert(link_elm327_build_command(" 010C ", command, sizeof(command), &written) == LINK_ELM327_RESULT_OK);
    assert(written == 5U && memcmp(command, "010C\r", 5U) == 0);
    assert(link_elm327_parser_begin(&parser, "010C") == LINK_ELM327_RESULT_OK);
    assert(link_elm327_parser_feed(&parser, response_bytes, sizeof(response_bytes) - 1U, &consumed) == LINK_ELM327_RESULT_OK);
    assert(link_elm327_parser_finish(&parser, &response) == LINK_ELM327_RESULT_OK);
    assert(response.echo_removed && strcmp(response.text, "41 0C 1A F8") == 0);

    link_elm327_probe_begin(&probe);
    assert(strcmp(link_elm327_probe_command(&probe), "AT@1") == 0);

    assert(link_elm327_can_channel_begin(&can_state, &can_config) == LINK_ELM327_CAN_RESULT_OK);
    assert(link_elm327_can_channel_command(&can_state, can_command, sizeof(can_command)) == LINK_ELM327_CAN_RESULT_OK);
    assert(strcmp(can_command, "ATSH7E0") == 0);
    memset(&response, 0, sizeof(response)); response.result = LINK_ELM327_RESULT_OK; strcpy(response.text, "62 F1 90 31"); response.length = strlen(response.text); response.line_count = 1U;
    assert(link_elm327_can_decode_pdu(&response, pdu, sizeof(pdu), &pdu_length) == LINK_ELM327_CAN_RESULT_OK);
    assert(pdu_length == 4U && pdu[0] == 0x62U && pdu[1] == 0xf1U);

    transport.context = &mock; transport.connect = mock_connect; transport.disconnect = mock_disconnect; transport.is_connected = mock_is_connected; transport.write = mock_write; transport.set_receiver = mock_set_receiver;
    assert(link_transport_is_valid(&transport));
    assert(link_elm327_session_init(&session, &transport, NULL, NULL));
    assert(link_elm327_session_connect(&session) == LINK_TRANSPORT_OK);
    assert(link_elm327_session_begin(&session, "010C", 100U, 500U) == LINK_ELM327_SESSION_OP_OK);
    assert(mock.last_write_size == 5U);
    assert(mock.receive != NULL);
    mock.receive(mock.receive_context, response_bytes, sizeof(response_bytes) - 1U);
    assert(session.status == LINK_ELM327_SESSION_COMPLETE);
    assert(link_elm327_session_response(&session) != NULL);
    link_elm327_session_deinit(&session);
    return 0;
}
