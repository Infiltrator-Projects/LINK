// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds.h"

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

int main(void)
{
    uint8_t request[8];
    size_t written = 0U;
    LinkUdsSessionResponse session;
    LinkUdsClient client;
    LinkUdsClientConfig config = { 50000U, 500000U };
    LinkUdsResponse response;
    const uint8_t session_reply[] = { 0x50U, 0x03U, 0x00U, 0x32U, 0x00U, 0x0aU };
    const uint8_t pending_reply[] = { 0x7fU, 0x22U, 0x78U };
    const uint8_t did_request[] = { 0x22U, 0xf1U, 0x90U };

    check(link_uds_build_session_control_request(
              LINK_UDS_SESSION_EXTENDED, false,
              request, sizeof(request), &written) == LINK_UDS_RESULT_OK &&
              written == 2U && request[0] == 0x10U && request[1] == 0x03U,
          "build session request");

    memset(&session, 0, sizeof(session));
    check(link_uds_decode_session_control_response(
              session_reply, sizeof(session_reply), LINK_UDS_SESSION_EXTENDED,
              &session) == LINK_UDS_RESULT_OK && session.timing_present &&
              session.p2_server_max_ms == 50U &&
              session.p2_star_server_max_10ms == 10U,
          "decode session timing");

    check(link_uds_client_init(&client, &config) == LINK_UDS_RESULT_OK,
          "client init");
    check(link_uds_client_begin(&client, did_request, sizeof(did_request), 1000U) ==
              LINK_UDS_RESULT_OK,
          "client begin");
    check(link_uds_client_accept(&client, pending_reply, sizeof(pending_reply),
                                 2000U, &response) ==
              LINK_UDS_RESULT_RESPONSE_PENDING &&
              client.state == LINK_UDS_CLIENT_RESPONSE_PENDING,
          "response pending extends client wait");
    check(link_uds_client_tick(&client, 2001U) == LINK_UDS_RESULT_WAITING,
          "client waiting after pending");

    if (failures != 0) {
        fprintf(stderr, "%d UDS test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
