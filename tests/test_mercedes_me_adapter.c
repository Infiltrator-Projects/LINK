// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_adapter.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", \
                #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct Capture {
    LinkMercedesMeStreamEventKind kinds[8];
    uint8_t bytes[8][LINK_MERCEDES_ME_RX_BUFFER_CAPACITY];
    size_t sizes[8];
    size_t count;
} Capture;

static void capture_event(void *context,
                          LinkMercedesMeStreamEventKind kind,
                          const uint8_t *bytes,
                          size_t size)
{
    Capture *capture = context;
    if (capture == NULL || capture->count >= 8U) return;
    capture->kinds[capture->count] = kind;
    capture->sizes[capture->count] = size;
    if (bytes != NULL && size != 0U)
        memcpy(capture->bytes[capture->count], bytes, size);
    ++capture->count;
}

int main(void)
{
    LinkMercedesMeStreamParser parser;
    Capture capture = {0};
    uint8_t long_record[LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD];
    static const uint8_t command[] = {'A', 'B', 'C', '\r'};
    static const uint8_t bad_command[] = {'A', 'B', 'C', '\n'};
    static const uint8_t part1[] = {'A', 'B'};
    static const uint8_t part2[] = {'C', '\r', 'D', 'E', 'F', 0x07U};
    static const uint8_t after_overflow[] = {'Y', 'Z', '\r'};
    size_t index;

    CHECK(link_mercedes_me_adapter_family_from_name("MB-123456") ==
          LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-812345") ==
          LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-912345") ==
          LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-212345") ==
          LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-312345") ==
          LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-412345") ==
          LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-612345") ==
          LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-512345") ==
          LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-712345") ==
          LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("VAN-1234") ==
          LINK_MERCEDES_ME_ADAPTER_OTHER_APPS);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-012345") ==
          LINK_MERCEDES_ME_ADAPTER_UNKNOWN);

    CHECK(strcmp(LINK_MERCEDES_ME_NUS_RX_UUID,
                 "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") == 0);
    CHECK(strcmp(LINK_MERCEDES_ME_NUS_TX_UUID,
                 "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") == 0);
    CHECK(LINK_MERCEDES_ME_REFERENCE_CLASSIC_CONNECT_TIMEOUT_MS == 44000U);
    CHECK(LINK_MERCEDES_ME_REFERENCE_MIN_CONNECTION_DURATION_MS == 6000U);
    CHECK(LINK_MERCEDES_ME_REFERENCE_BLE_MTU == 512U);
    CHECK(LINK_MERCEDES_ME_RECORD_TERMINATOR == 0x0DU);
    CHECK(LINK_MERCEDES_ME_NACK_TERMINATOR == 0x07U);
    CHECK(LINK_MERCEDES_ME_RX_BUFFER_CAPACITY == 700U);
    CHECK(LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD == 698U);
    CHECK(LINK_MERCEDES_ME_STATE_CALLBACK_SLEEP_SENTINEL == 4711);

    CHECK(strcmp(link_mercedes_me_qos_state_name(2), "QOS_WEAK") == 0);
    CHECK(strcmp(link_mercedes_me_execution_state_name(3),
                 "ADAPTER_ERROR") == 0);
    CHECK(strcmp(link_mercedes_me_reason_name(11),
                 "R_SMK_NOT_AVAILABLE") == 0);
    CHECK(strcmp(link_mercedes_me_reason_name(21),
                 "R_ADAPTER_ERROR_CAN_OFF_21") == 0);
    CHECK(strcmp(link_mercedes_me_reason_name(26),
                 "R_GATT_FAILURE") == 0);
    CHECK(strcmp(link_mercedes_me_reason_name(27), "R_UNKNOWN") == 0);
    CHECK(strcmp(link_mercedes_me_comm_state_name(7),
                 "COMM_STATE_DEVICE_NO_SPP") == 0);
    CHECK(strcmp(link_mercedes_me_comm_state_name(6),
                 "COMM_STATE_UNKNOWN") == 0);
    CHECK(link_mercedes_me_connection_problem_count() == 29U);
    CHECK(strcmp(link_mercedes_me_connection_problem_at(0U)->name,
                 "NO_PROBLEM") == 0);
    CHECK(link_mercedes_me_connection_problem_at(0U)->error_code == 0);
    CHECK(strcmp(link_mercedes_me_connection_problem_at(12U)->name,
                 "SMK_NOT_AVAILABLE") == 0);
    CHECK(link_mercedes_me_connection_problem_at(12U)->error_code == 611);
    CHECK(strcmp(link_mercedes_me_connection_problem_at(28U)->name,
                 "GATT_FAILURE") == 0);
    CHECK(link_mercedes_me_connection_problem_at(28U)->error_code == 650);
    CHECK(link_mercedes_me_connection_problem_at(29U) == NULL);
    CHECK(strcmp(link_mercedes_me_connection_problem_find_error_code(647)->name,
                 "PARALLEL_OBD_ADAPTER_CONNECTED") == 0);
    CHECK(link_mercedes_me_connection_problem_find_error_code(649) == NULL);

    CHECK(link_mercedes_me_command_has_valid_terminator(
        command, sizeof(command)));
    CHECK(!link_mercedes_me_command_has_valid_terminator(
        bad_command, sizeof(bad_command)));
    CHECK(!link_mercedes_me_command_has_valid_terminator(NULL, 0U));

    link_mercedes_me_stream_parser_init(&parser);
    CHECK(link_mercedes_me_stream_parser_feed(
              &parser, part1, sizeof(part1), capture_event, &capture) == 0U);
    CHECK(parser.used == 2U);
    CHECK(link_mercedes_me_stream_parser_feed(
              &parser, part2, sizeof(part2), capture_event, &capture) == 2U);
    CHECK(capture.count == 2U);
    CHECK(capture.kinds[0] == LINK_MERCEDES_ME_STREAM_RECORD);
    CHECK(capture.sizes[0] == 4U);
    CHECK(memcmp(capture.bytes[0], "ABC\r", 4U) == 0);
    CHECK(capture.kinds[1] == LINK_MERCEDES_ME_STREAM_NACK);
    CHECK(capture.sizes[1] == 4U);
    CHECK(capture.bytes[1][3] == 0x07U);
    CHECK(parser.used == 0U);

    for (index = 0U; index < sizeof(long_record); ++index)
        long_record[index] = (uint8_t)'X';
    CHECK(link_mercedes_me_stream_parser_feed(
              &parser, long_record, sizeof(long_record),
              capture_event, &capture) == 0U);
    CHECK(parser.used == LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD);
    CHECK(link_mercedes_me_stream_parser_feed(
              &parser, after_overflow, sizeof(after_overflow),
              capture_event, &capture) == 2U);
    CHECK(capture.count == 4U);
    CHECK(capture.kinds[2] == LINK_MERCEDES_ME_STREAM_OVERFLOW);
    CHECK(capture.sizes[2] == LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD);
    CHECK(parser.overflow_count == 1U);
    CHECK(capture.kinds[3] == LINK_MERCEDES_ME_STREAM_RECORD);
    CHECK(capture.sizes[3] == 2U);
    CHECK(capture.bytes[3][0] == (uint8_t)'Z');
    CHECK(capture.bytes[3][1] == (uint8_t)'\r');

    return 0;
}
