// SPDX-License-Identifier: GPL-3.0-or-later
#include "support/elm_trace_replay.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
    return 1; \
} } while (0)

static int test_successful_replay(void)
{
    static const LinkTestElmTraceEntry entries[] = {
        { "ATZ", LINK_ELM327_RESULT_OK, "ELM327 v2.3", false },
        { "0100", LINK_ELM327_RESULT_OK, "410098180001", false },
        { "ATCAF1", LINK_ELM327_RESULT_OK, "OK", true }
    };
    LinkTestElmTraceReplay replay;
    LinkElm327Response response;

    link_test_elm_trace_replay_init(
        &replay, entries, sizeof(entries) / sizeof(entries[0]));

    CHECK(link_test_elm_trace_replay_next(&replay, "ATZ", &response));
    CHECK(response.result == LINK_ELM327_RESULT_OK);
    CHECK(strcmp(response.text, "ELM327 v2.3") == 0);
    CHECK(response.length == strlen("ELM327 v2.3"));
    CHECK(!response.ok_seen);

    CHECK(link_test_elm_trace_replay_next(&replay, "0100", &response));
    CHECK(strcmp(response.text, "410098180001") == 0);

    CHECK(link_test_elm_trace_replay_next(&replay, "ATCAF1", &response));
    CHECK(response.ok_seen);
    CHECK(link_test_elm_trace_replay_complete(&replay));
    return 0;
}

static int test_mismatch_fails_replay(void)
{
    static const LinkTestElmTraceEntry entries[] = {
        { "0100", LINK_ELM327_RESULT_OK, "410000000000", false }
    };
    LinkTestElmTraceReplay replay;
    LinkElm327Response response;

    link_test_elm_trace_replay_init(&replay, entries, 1U);
    CHECK(!link_test_elm_trace_replay_next(&replay, "0120", &response));
    CHECK(replay.failed);
    CHECK(!link_test_elm_trace_replay_complete(&replay));
    return 0;
}

static int test_invalid_fixture_is_rejected(void)
{
    LinkTestElmTraceReplay replay;
    LinkElm327Response response;

    link_test_elm_trace_replay_init(&replay, NULL, 1U);
    CHECK(replay.failed);
    CHECK(!link_test_elm_trace_replay_next(&replay, "0100", &response));
    return 0;
}

int main(void)
{
    CHECK(test_successful_replay() == 0);
    CHECK(test_mismatch_fails_replay() == 0);
    CHECK(test_invalid_fixture_is_rejected() == 0);
    return 0;
}
