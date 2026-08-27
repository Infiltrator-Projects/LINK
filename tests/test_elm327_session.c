// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/elm327_session.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    bool connected;
    char last_write[64];
    size_t last_write_size;
    LinkTransportReceiveFn receiver;
    void *receiver_context;
} MockTransport;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; } } while (0)

static LinkTransportStatus mock_connect(void *context)
{
    ((MockTransport *)context)->connected = true;
    return LINK_TRANSPORT_OK;
}
static void mock_disconnect(void *context) { ((MockTransport *)context)->connected = false; }
static bool mock_is_connected(void *context) { return ((const MockTransport *)context)->connected; }
static LinkTransportStatus mock_write(void *context, const uint8_t *data, size_t size)
{
    MockTransport *mock = context;
    size_t copy = size < sizeof(mock->last_write) - 1U ? size : sizeof(mock->last_write) - 1U;
    memcpy(mock->last_write, data, copy);
    mock->last_write[copy] = '\0';
    mock->last_write_size = size;
    return LINK_TRANSPORT_OK;
}
static void mock_set_receiver(void *context, LinkTransportReceiveFn receiver, void *receiver_context)
{
    MockTransport *mock = context;
    mock->receiver = receiver;
    mock->receiver_context = receiver_context;
}
static LinkTransport mock_interface(MockTransport *mock)
{
    LinkTransport transport = {
        .struct_size = sizeof(LinkTransport), .abi_version = LINK_TRANSPORT_ABI,
        .context = mock, .connect = mock_connect, .disconnect = mock_disconnect,
        .is_connected = mock_is_connected, .write = mock_write, .set_receiver = mock_set_receiver
    };
    return transport;
}
static void emit(MockTransport *mock, const char *text)
{
    mock->receiver(mock->receiver_context, (const uint8_t *)text, strlen(text));
}

int main(void)
{
    MockTransport mock = {0};
    LinkTransport transport = mock_interface(&mock);
    LinkElm327Session session;

    CHECK(link_elm327_session_init(&session, &transport, NULL, NULL));
    CHECK(link_elm327_session_connect(&session) == LINK_TRANSPORT_OK);
    CHECK(link_elm327_session_begin(&session, "0100", 100U, 50U) == LINK_ELM327_SESSION_OP_OK);
    CHECK(link_elm327_session_tick(&session, 150U) == LINK_ELM327_SESSION_TIMED_OUT);
    CHECK(session.needs_resync);
    CHECK(link_elm327_session_begin(&session, "ATI", 151U, 50U) == LINK_ELM327_SESSION_OP_NEEDS_RESYNC);

    CHECK(link_elm327_session_begin_resynchronization(&session, 151U, 100U) == LINK_ELM327_SESSION_OP_OK);
    CHECK(session.status == LINK_ELM327_SESSION_RESYNCHRONIZING);
    CHECK(mock.last_write_size == 1U && mock.last_write[0] == '\r');
    emit(&mock, "late response without prompt");
    CHECK(session.status == LINK_ELM327_SESSION_RESYNCHRONIZING);
    emit(&mock, ">junk");
    CHECK(session.status == LINK_ELM327_SESSION_RESYNCHRONIZING);
    emit(&mock, "\r\n>");
    CHECK(session.status == LINK_ELM327_SESSION_RESYNCHRONIZED);
    CHECK(!session.needs_resync);

    CHECK(link_elm327_session_begin(&session, "ATI", 200U, 100U) == LINK_ELM327_SESSION_OP_OK);
    emit(&mock, "ELM327 v2.3\r>");
    CHECK(session.status == LINK_ELM327_SESSION_COMPLETE);

    link_elm327_session_mark_resynchronized(&session);
    session.needs_resync = true;
    CHECK(link_elm327_session_begin_resynchronization(&session, 300U, 20U) == LINK_ELM327_SESSION_OP_OK);
    CHECK(link_elm327_session_tick(&session, 320U) == LINK_ELM327_SESSION_TIMED_OUT);
    CHECK(session.needs_resync);
    puts("ELM327 session resynchronisation tests passed");
    return 0;
}
