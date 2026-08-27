// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file session.c
 * @brief Transport-backed ELM327 command execution.
 */
#include "link/elm327_session.h"

#include "infiltratr/core.h"

#include <string.h>

static bool elm327_session_bytes_are_whitespace(
    const uint8_t *data, size_t size)
{
    if (data == NULL && size != 0U) {
        return false;
    }
    for (size_t index = 0U; index < size; ++index) {
        const uint8_t value = data[index];
        if (value != (uint8_t)' ' && value != (uint8_t)'\t' &&
            value != (uint8_t)'\r' && value != (uint8_t)'\n' &&
            value != (uint8_t)'\v' && value != (uint8_t)'\f') {
            return false;
        }
    }
    return true;
}

static void elm327_session_add_unexpected(LinkElm327Session *session,
                                          size_t amount)
{
    if (session == NULL || amount == 0U) {
        return;
    }
    if (SIZE_MAX - session->unexpected_input_bytes < amount) {
        session->unexpected_input_bytes = SIZE_MAX;
    } else {
        session->unexpected_input_bytes += amount;
    }
}

static void elm327_session_notify(LinkElm327Session *session)
{
    if (session != NULL && session->event != NULL) {
        session->callback_active = true;
        session->event(session->event_context, session);
        session->callback_active = false;
    }
}

static void elm327_session_fail(LinkElm327Session *session,
                                LinkElm327Result elm_result,
                                LinkTransportStatus transport_status,
                                bool needs_resync)
{
    session->status = LINK_ELM327_SESSION_FAILED;
    session->elm_result = elm_result;
    session->transport_status = transport_status;
    session->needs_resync = needs_resync;
    elm327_session_notify(session);
}

static void elm327_session_receive(void *context,
                                   const uint8_t *data,
                                   size_t size)
{
    LinkElm327Session *session = context;
    size_t offset = 0U;

    if (session == NULL || (data == NULL && size != 0U)) {
        return;
    }

    if (session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        size_t last_prompt = SIZE_MAX;
        for (size_t index = 0U; index < size; ++index) {
            if (data[index] == (uint8_t)'>') last_prompt = index;
        }
        elm327_session_add_unexpected(session, size);
        if (last_prompt == SIZE_MAX ||
            !elm327_session_bytes_are_whitespace(
                data + last_prompt + 1U, size - last_prompt - 1U)) {
            return;
        }
        session->status = LINK_ELM327_SESSION_RESYNCHRONIZED;
        session->needs_resync = false;
        session->elm_result = LINK_ELM327_RESULT_OK;
        session->transport_status = LINK_TRANSPORT_OK;
        memset(&session->parser, 0, sizeof(session->parser));
        memset(&session->response, 0, sizeof(session->response));
        elm327_session_notify(session);
        return;
    }

    if (session->status != LINK_ELM327_SESSION_WAITING) {
        elm327_session_add_unexpected(session, size);
        if (!elm327_session_bytes_are_whitespace(data, size)) {
            session->needs_resync = true;
        }
        return;
    }

    while (offset < size &&
           session->status == LINK_ELM327_SESSION_WAITING) {
        size_t consumed = 0U;
        LinkElm327Result result = link_elm327_parser_feed(
            &session->parser, data + offset, size - offset, &consumed);

        offset += consumed;
        if (result == LINK_ELM327_RESULT_MORE_DATA) {
            break;
        }
        if (result != LINK_ELM327_RESULT_OK) {
            elm327_session_add_unexpected(session, size - offset);
            elm327_session_fail(session, result, LINK_TRANSPORT_OK, true);
            return;
        }

        result = link_elm327_parser_finish(&session->parser,
                                           &session->response);
        session->elm_result = result;
        session->transport_status = LINK_TRANSPORT_OK;
        session->status = LINK_ELM327_SESSION_COMPLETE;
        elm327_session_add_unexpected(session, size - offset);
        if (!elm327_session_bytes_are_whitespace(data + offset,
                                                 size - offset)) {
            session->needs_resync = true;
        }
        elm327_session_notify(session);
    }
}

bool link_elm327_session_init(LinkElm327Session *session,
                              const LinkTransport *transport,
                              LinkElm327SessionEventFn event,
                              void *event_context)
{
    if (session == NULL || !link_transport_is_valid(transport)) {
        return false;
    }

    memset(session, 0, sizeof(*session));
    session->transport = *transport;
    session->status = LINK_ELM327_SESSION_IDLE;
    session->elm_result = LINK_ELM327_RESULT_OK;
    session->transport_status = LINK_TRANSPORT_OK;
    session->event = event;
    session->event_context = event_context;
    session->transport.set_receiver(session->transport.context,
                                    elm327_session_receive, session);
    return true;
}

void link_elm327_session_deinit(LinkElm327Session *session)
{
    if (session == NULL || session->callback_active) {
        return;
    }
    if (link_transport_is_valid(&session->transport)) {
        session->transport.set_receiver(session->transport.context, NULL, NULL);
    }
    memset(session, 0, sizeof(*session));
}

LinkTransportStatus link_elm327_session_connect(LinkElm327Session *session)
{
    LinkTransportStatus result;

    if (session == NULL ||
        !link_transport_is_valid(&session->transport)) {
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    }
    if (session->callback_active ||
        session->status == LINK_ELM327_SESSION_WAITING ||
        session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        return LINK_TRANSPORT_BUSY;
    }

    session->transport.set_receiver(session->transport.context,
                                    elm327_session_receive, session);
    result = session->transport.connect(session->transport.context);
    session->transport_status = result;
    if (result == LINK_TRANSPORT_OK) {
        session->status = LINK_ELM327_SESSION_IDLE;
        session->elm_result = LINK_ELM327_RESULT_OK;
        session->needs_resync = false;
        memset(&session->parser, 0, sizeof(session->parser));
        memset(&session->response, 0, sizeof(session->response));
    }
    return result;
}

void link_elm327_session_disconnect(LinkElm327Session *session)
{
    if (session == NULL ||
        !link_transport_is_valid(&session->transport) ||
        session->callback_active) {
        return;
    }

    session->transport.disconnect(session->transport.context);
    session->transport.set_receiver(session->transport.context, NULL, NULL);
    if (session->status == LINK_ELM327_SESSION_WAITING ||
        session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        session->status = LINK_ELM327_SESSION_CANCELLED;
        session->needs_resync = false;
        elm327_session_notify(session);
    } else {
        session->status = LINK_ELM327_SESSION_IDLE;
        session->needs_resync = false;
    }
    session->transport_status = LINK_TRANSPORT_NOT_CONNECTED;
}

bool link_elm327_session_is_connected(const LinkElm327Session *session)
{
    if (session == NULL ||
        !link_transport_is_valid(&session->transport)) {
        return false;
    }
    return session->transport.is_connected(session->transport.context);
}

LinkElm327SessionOpResult link_elm327_session_begin(
    LinkElm327Session *session,
    const char *command,
    uint64_t now_ms,
    uint64_t timeout_ms)
{
    uint8_t frame[LINK_ELM327_MAX_COMMAND + 1U];
    size_t frame_size = 0U;
    uint64_t deadline;
    LinkElm327Result parser_result;
    LinkTransportStatus write_result;

    if (session == NULL || command == NULL || timeout_ms == 0U ||
        !link_transport_is_valid(&session->transport)) {
        return LINK_ELM327_SESSION_OP_INVALID_ARGUMENT;
    }
    if (session->callback_active ||
        session->status == LINK_ELM327_SESSION_WAITING ||
        session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        return LINK_ELM327_SESSION_OP_BUSY;
    }
    if (session->needs_resync) {
        return LINK_ELM327_SESSION_OP_NEEDS_RESYNC;
    }
    if (!session->transport.is_connected(session->transport.context)) {
        return LINK_ELM327_SESSION_OP_NOT_CONNECTED;
    }
    if (!infiltratr_u64_add_checked(now_ms, timeout_ms, &deadline)) {
        return LINK_ELM327_SESSION_OP_DEADLINE_OVERFLOW;
    }

    parser_result = link_elm327_build_command(
        command, frame, sizeof(frame), &frame_size);
    if (parser_result != LINK_ELM327_RESULT_OK) {
        session->elm_result = parser_result;
        return LINK_ELM327_SESSION_OP_INVALID_ARGUMENT;
    }
    parser_result = link_elm327_parser_begin(&session->parser, command);
    if (parser_result != LINK_ELM327_RESULT_OK) {
        session->elm_result = parser_result;
        return LINK_ELM327_SESSION_OP_INVALID_ARGUMENT;
    }

    memset(&session->response, 0, sizeof(session->response));
    session->status = LINK_ELM327_SESSION_WAITING;
    session->elm_result = LINK_ELM327_RESULT_MORE_DATA;
    session->transport_status = LINK_TRANSPORT_OK;
    session->deadline_ms = deadline;
    session->sequence++;

    write_result = session->transport.write(
        session->transport.context, frame, frame_size);
    if (write_result != LINK_TRANSPORT_OK) {
        elm327_session_fail(session, LINK_ELM327_RESULT_MORE_DATA,
                            write_result, true);
        return LINK_ELM327_SESSION_OP_TRANSPORT_ERROR;
    }

    return LINK_ELM327_SESSION_OP_OK;
}

LinkElm327SessionStatus link_elm327_session_tick(
    LinkElm327Session *session,
    uint64_t now_ms)
{
    if (session == NULL) {
        return LINK_ELM327_SESSION_FAILED;
    }

    if ((session->status == LINK_ELM327_SESSION_WAITING ||
         session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) &&
        now_ms >= session->deadline_ms) {
        session->status = LINK_ELM327_SESSION_TIMED_OUT;
        session->transport_status = LINK_TRANSPORT_TIMEOUT;
        session->needs_resync = true;
        elm327_session_notify(session);
    }
    return session->status;
}

LinkElm327SessionOpResult link_elm327_session_begin_resynchronization(
    LinkElm327Session *session,
    uint64_t now_ms,
    uint64_t timeout_ms)
{
    static const uint8_t prompt_request[] = {'\r'};
    uint64_t deadline;
    LinkTransportStatus write_result;

    if (session == NULL || timeout_ms == 0U ||
        !link_transport_is_valid(&session->transport)) {
        return LINK_ELM327_SESSION_OP_INVALID_ARGUMENT;
    }
    if (session->callback_active ||
        session->status == LINK_ELM327_SESSION_WAITING ||
        session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        return LINK_ELM327_SESSION_OP_BUSY;
    }
    if (!session->needs_resync) {
        return LINK_ELM327_SESSION_OP_INVALID_ARGUMENT;
    }
    if (!session->transport.is_connected(session->transport.context)) {
        return LINK_ELM327_SESSION_OP_NOT_CONNECTED;
    }
    if (!infiltratr_u64_add_checked(now_ms, timeout_ms, &deadline)) {
        return LINK_ELM327_SESSION_OP_DEADLINE_OVERFLOW;
    }

    session->status = LINK_ELM327_SESSION_RESYNCHRONIZING;
    session->deadline_ms = deadline;
    session->elm_result = LINK_ELM327_RESULT_MORE_DATA;
    session->transport_status = LINK_TRANSPORT_OK;
    memset(&session->parser, 0, sizeof(session->parser));
    memset(&session->response, 0, sizeof(session->response));

    write_result = session->transport.write(
        session->transport.context, prompt_request, sizeof(prompt_request));
    if (write_result != LINK_TRANSPORT_OK) {
        elm327_session_fail(session, LINK_ELM327_RESULT_MORE_DATA,
                            write_result, true);
        return LINK_ELM327_SESSION_OP_TRANSPORT_ERROR;
    }
    return LINK_ELM327_SESSION_OP_OK;
}

bool link_elm327_session_cancel(LinkElm327Session *session)
{
    if (session == NULL ||
        (session->status != LINK_ELM327_SESSION_WAITING &&
         session->status != LINK_ELM327_SESSION_RESYNCHRONIZING)) {
        return false;
    }

    session->status = LINK_ELM327_SESSION_CANCELLED;
    session->needs_resync = true;
    elm327_session_notify(session);
    return true;
}

void link_elm327_session_mark_resynchronized(LinkElm327Session *session)
{
    if (session == NULL ||
        session->status == LINK_ELM327_SESSION_WAITING ||
        session->status == LINK_ELM327_SESSION_RESYNCHRONIZING) {
        return;
    }

    session->needs_resync = false;
    session->status = LINK_ELM327_SESSION_IDLE;
    session->elm_result = LINK_ELM327_RESULT_OK;
    session->transport_status = LINK_TRANSPORT_OK;
    memset(&session->parser, 0, sizeof(session->parser));
    memset(&session->response, 0, sizeof(session->response));
}

const LinkElm327Response *link_elm327_session_response(
    const LinkElm327Session *session)
{
    if (session == NULL ||
        session->status != LINK_ELM327_SESSION_COMPLETE) {
        return NULL;
    }
    return &session->response;
}

const char *link_elm327_session_op_result_name(
    LinkElm327SessionOpResult result)
{
    switch (result) {
    case LINK_ELM327_SESSION_OP_OK:
        return "ok";
    case LINK_ELM327_SESSION_OP_INVALID_ARGUMENT:
        return "invalid-argument";
    case LINK_ELM327_SESSION_OP_BUSY:
        return "busy";
    case LINK_ELM327_SESSION_OP_NOT_CONNECTED:
        return "not-connected";
    case LINK_ELM327_SESSION_OP_NEEDS_RESYNC:
        return "needs-resync";
    case LINK_ELM327_SESSION_OP_DEADLINE_OVERFLOW:
        return "deadline-overflow";
    case LINK_ELM327_SESSION_OP_TRANSPORT_ERROR:
        return "transport-error";
    }
    return "unknown";
}
