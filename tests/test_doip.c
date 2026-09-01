// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/doip.h"

#include <stdio.h>
#include <string.h>

#define REQUIRE(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "Requirement failed at %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    uint8_t frame[64U];
    size_t written = 0U;
    const uint8_t uds[] = {0x22U, 0xF1U, 0x90U};
    const uint8_t expected[] = {
        0x02U, 0xFDU, 0x80U, 0x01U, 0x00U, 0x00U, 0x00U, 0x07U,
        0x0EU, 0x00U, 0x10U, 0x00U, 0x22U, 0xF1U, 0x90U
    };
    LinkDoipHeader header;
    const uint8_t *payload = NULL;
    const uint8_t *decoded_uds = NULL;
    size_t decoded_uds_length = 0U;
    uint16_t source = 0U, target = 0U;

    REQUIRE(link_doip_build_diagnostic_message(
        0x02U, 0x0E00U, 0x1000U, uds, sizeof(uds),
        frame, sizeof(frame), &written) == LINK_DOIP_RESULT_OK);
    REQUIRE(written == sizeof(expected));
    REQUIRE(memcmp(frame, expected, sizeof(expected)) == 0);

    REQUIRE(link_doip_decode_frame(
        frame, written, &header, &payload) == LINK_DOIP_RESULT_OK);
    REQUIRE(header.payload_type == LINK_DOIP_PAYLOAD_DIAGNOSTIC_MESSAGE);
    REQUIRE(header.payload_length == 7U);

    REQUIRE(link_doip_decode_diagnostic_message(
        frame, written, &source, &target,
        &decoded_uds, &decoded_uds_length) == LINK_DOIP_RESULT_OK);
    REQUIRE(source == 0x0E00U && target == 0x1000U);
    REQUIRE(decoded_uds_length == sizeof(uds));
    REQUIRE(memcmp(decoded_uds, uds, sizeof(uds)) == 0);

    REQUIRE(link_doip_build_routing_activation_request(
        0x02U, 0x0E00U, 0x00U,
        frame, sizeof(frame), &written) == LINK_DOIP_RESULT_OK);
    REQUIRE(written == 15U && frame[3] == 0x05U && frame[7] == 0x07U);

    REQUIRE(link_doip_build_alive_check_response(
        0x02U, 0x0E00U,
        frame, sizeof(frame), &written) == LINK_DOIP_RESULT_OK);
    REQUIRE(written == 10U && frame[3] == 0x08U);

    frame[1] ^= 0x01U;
    REQUIRE(link_doip_decode_frame(
        frame, written, &header, &payload) == LINK_DOIP_RESULT_MALFORMED_FRAME);
    frame[1] ^= 0x01U;
    REQUIRE(link_doip_decode_frame(
        frame, written - 1U, &header, &payload) == LINK_DOIP_RESULT_MALFORMED_FRAME);
    return 0;
}
