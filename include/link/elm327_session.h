// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file elm327_session.h
 * @brief Transport-backed, single-command ELM327 session engine.
 *
 * Sessions are intentionally single-threaded. The caller supplies monotonic
 * time and must serialize access. Provider resources referenced by the copied
 * transport descriptor must outlive link_elm327_session_deinit().
 */
#ifndef LINK_ELM327_SESSION_H
#define LINK_ELM327_SESSION_H

#include "link/elm327.h"
#include "link/transport.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkElm327SessionStatus {
    LINK_ELM327_SESSION_IDLE = 0,
    LINK_ELM327_SESSION_WAITING,
    LINK_ELM327_SESSION_COMPLETE,
    LINK_ELM327_SESSION_TIMED_OUT,
    LINK_ELM327_SESSION_RESYNCHRONIZING,
    LINK_ELM327_SESSION_RESYNCHRONIZED,
    LINK_ELM327_SESSION_CANCELLED,
    LINK_ELM327_SESSION_FAILED
} LinkElm327SessionStatus;

typedef enum LinkElm327SessionOpResult {
    LINK_ELM327_SESSION_OP_OK = 0,
    LINK_ELM327_SESSION_OP_INVALID_ARGUMENT,
    LINK_ELM327_SESSION_OP_BUSY,
    LINK_ELM327_SESSION_OP_NOT_CONNECTED,
    LINK_ELM327_SESSION_OP_NEEDS_RESYNC,
    LINK_ELM327_SESSION_OP_DEADLINE_OVERFLOW,
    LINK_ELM327_SESSION_OP_TRANSPORT_ERROR
} LinkElm327SessionOpResult;

struct LinkElm327Session;
typedef struct LinkElm327Session LinkElm327Session;
typedef void (*LinkElm327SessionEventFn)(void *context,
                                         const LinkElm327Session *session);

struct LinkElm327Session {
    LinkTransport transport;
    LinkElm327Parser parser;
    LinkElm327Response response;
    LinkElm327SessionStatus status;
    LinkElm327Result elm_result;
    LinkTransportStatus transport_status;
    uint64_t deadline_ms;
    uint64_t sequence;
    size_t unexpected_input_bytes;
    bool needs_resync;
    bool callback_active;
    LinkElm327SessionEventFn event;
    void *event_context;
};

bool link_elm327_session_init(LinkElm327Session *session,
                              const LinkTransport *transport,
                              LinkElm327SessionEventFn event,
                              void *event_context);
void link_elm327_session_deinit(LinkElm327Session *session);
LinkTransportStatus link_elm327_session_connect(LinkElm327Session *session);
void link_elm327_session_disconnect(LinkElm327Session *session);
bool link_elm327_session_is_connected(const LinkElm327Session *session);

/** Begin one command. Timeout zero is invalid; callbacks may inspect only. */
LinkElm327SessionOpResult link_elm327_session_begin(
    LinkElm327Session *session,
    const char *command,
    uint64_t now_ms,
    uint64_t timeout_ms);
LinkElm327SessionStatus link_elm327_session_tick(
    LinkElm327Session *session,
    uint64_t now_ms);

/**
 * Begin bounded prompt-based resynchronisation after timeout/cancellation.
 * A bare carriage return requests a fresh prompt; stale input is discarded
 * until a prompt with only trailing whitespace is observed.
 */
LinkElm327SessionOpResult link_elm327_session_begin_resynchronization(
    LinkElm327Session *session,
    uint64_t now_ms,
    uint64_t timeout_ms);

/** Cancel locally and require explicit resynchronisation before reuse. */
bool link_elm327_session_cancel(LinkElm327Session *session);
void link_elm327_session_mark_resynchronized(LinkElm327Session *session);
const LinkElm327Response *link_elm327_session_response(
    const LinkElm327Session *session);
const char *link_elm327_session_op_result_name(LinkElm327SessionOpResult result);

#ifdef __cplusplus
}
#endif

#endif
