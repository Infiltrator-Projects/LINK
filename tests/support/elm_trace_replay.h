// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TEST_ELM_TRACE_REPLAY_H
#define LINK_TEST_ELM_TRACE_REPLAY_H

#include "link/elm327.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    const char *command;
    LinkElm327Result result;
    const char *response_text;
    bool ok_seen;
} LinkTestElmTraceEntry;

typedef struct {
    const LinkTestElmTraceEntry *entries;
    size_t count;
    size_t index;
    bool failed;
} LinkTestElmTraceReplay;

static inline void link_test_elm_trace_replay_init(
    LinkTestElmTraceReplay *replay,
    const LinkTestElmTraceEntry *entries,
    size_t count)
{
    if (replay == NULL) {
        return;
    }
    replay->entries = entries;
    replay->count = count;
    replay->index = 0U;
    replay->failed = entries == NULL && count != 0U;
}

static inline bool link_test_elm_trace_replay_next(
    LinkTestElmTraceReplay *replay,
    const char *command,
    LinkElm327Response *response)
{
    const LinkTestElmTraceEntry *entry;
    size_t length;

    if (replay == NULL || command == NULL || response == NULL ||
        replay->failed || replay->entries == NULL ||
        replay->index >= replay->count) {
        if (replay != NULL) {
            replay->failed = true;
        }
        return false;
    }

    entry = &replay->entries[replay->index];
    if (entry->command == NULL || strcmp(entry->command, command) != 0) {
        replay->failed = true;
        return false;
    }

    memset(response, 0, sizeof(*response));
    response->result = entry->result;
    response->ok_seen = entry->ok_seen;
    if (entry->response_text != NULL) {
        length = strlen(entry->response_text);
        if (length >= sizeof(response->text)) {
            replay->failed = true;
            return false;
        }
        memcpy(response->text, entry->response_text, length);
        response->text[length] = '\0';
        response->length = length;
    }

    replay->index++;
    return true;
}

static inline bool link_test_elm_trace_replay_complete(
    const LinkTestElmTraceReplay *replay)
{
    return replay != NULL && !replay->failed && replay->index == replay->count;
}

#endif
