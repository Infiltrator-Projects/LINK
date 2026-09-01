// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/isotp.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR_MAX_PAYLOAD 4096U

static int hex_nibble(int value)
{
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

static bool parse_payload(const char *text, uint8_t *payload, size_t *length)
{
    int high = -1;
    size_t count = 0U;
    size_t index;
    if (text == NULL || payload == NULL || length == NULL) return false;
    for (index = 0U; text[index] != '\0'; ++index) {
        const int nibble = hex_nibble((unsigned char)text[index]);
        if (nibble < 0) {
            if (isspace((unsigned char)text[index]) ||
                text[index] == ':' || text[index] == '-') continue;
            return false;
        }
        if (high < 0) high = nibble;
        else {
            if (count >= VECTOR_MAX_PAYLOAD) return false;
            payload[count++] = (uint8_t)((high << 4) | nibble);
            high = -1;
        }
    }
    if (high >= 0 || count == 0U) return false;
    *length = count;
    return true;
}

static bool parse_stmin(const char *text, uint8_t *stmin)
{
    char *end = NULL;
    unsigned long value;
    if (text == NULL || stmin == NULL) return false;
    errno = 0;
    value = strtoul(text, &end, 0);
    if (errno != 0 || end == text || *end != '\0' || value > UINT8_MAX)
        return false;
    *stmin = (uint8_t)value;
    return true;
}

static void print_frame(const LinkIsoTpCanFrame *frame)
{
    size_t index;
    printf("%03X#", (unsigned int)frame->can_id);
    for (index = 0U; index < frame->length; ++index)
        printf("%02X", (unsigned int)frame->data[index]);
    putchar('\n');
}

int main(int argc, char **argv)
{
    uint8_t payload[VECTOR_MAX_PAYLOAD];
    size_t payload_length = 0U;
    uint8_t stmin = 0U;
    uint32_t stmin_us = 0U;
    uint64_t now_us = UINT64_C(1000);
    LinkIsoTpTx tx;
    LinkIsoTpTxConfig config;
    LinkIsoTpCanFrame frame;
    LinkIsoTpCanFrame flow_control;
    LinkIsoTpResult result;

    if (argc < 2 || argc > 3 ||
        !parse_payload(argv[1], payload, &payload_length) ||
        (argc == 3 && !parse_stmin(argv[2], &stmin))) {
        fprintf(stderr, "usage: %s PAYLOAD_HEX [STMIN]\n", argv[0]);
        return 2;
    }
    if (!link_isotp_stmin_to_us(stmin, &stmin_us)) {
        fprintf(stderr, "invalid ISO-TP STmin: 0x%02X\n", stmin);
        return 2;
    }

    memset(&config, 0, sizeof(config));
    config.address.tx_can_id = UINT32_C(0x123);
    config.address.rx_can_id = UINT32_C(0x321);
    config.address.addressing_mode = LINK_ISOTP_ADDRESSING_NORMAL;
    config.address.target_type = LINK_ISOTP_TARGET_PHYSICAL;
    config.flow_control_timeout_us = UINT64_C(1000000);
    config.max_wait_frames = 3U;

    result = link_isotp_tx_init(&tx, &config, payload, payload_length);
    if (result != LINK_ISOTP_RESULT_OK) {
        fprintf(stderr, "tx init failed: %s\n", link_isotp_result_name(result));
        return 3;
    }

    result = link_isotp_tx_start(&tx, now_us, &frame);
    if (result != LINK_ISOTP_RESULT_COMPLETE &&
        result != LINK_ISOTP_RESULT_WAIT_FLOW_CONTROL) {
        fprintf(stderr, "tx start failed: %s\n", link_isotp_result_name(result));
        return 3;
    }
    print_frame(&frame);
    if (result == LINK_ISOTP_RESULT_COMPLETE) return 0;

    memset(&flow_control, 0, sizeof(flow_control));
    flow_control.can_id = UINT32_C(0x321);
    flow_control.length = 3U;
    flow_control.data[0] = UINT8_C(0x30);
    flow_control.data[1] = 0U;
    flow_control.data[2] = stmin;
    result = link_isotp_tx_accept_flow_control(&tx, &flow_control, now_us);
    if (result != LINK_ISOTP_RESULT_OK) {
        fprintf(stderr, "flow control failed: %s\n",
                link_isotp_result_name(result));
        return 3;
    }

    for (;;) {
        now_us += stmin_us == 0U ? UINT64_C(1) : (uint64_t)stmin_us;
        result = link_isotp_tx_next(&tx, now_us, &frame);
        if (result == LINK_ISOTP_RESULT_WAIT_SEPARATION) continue;
        if (result != LINK_ISOTP_RESULT_OK &&
            result != LINK_ISOTP_RESULT_COMPLETE) {
            fprintf(stderr, "tx next failed: %s\n",
                    link_isotp_result_name(result));
            return 3;
        }
        print_frame(&frame);
        if (result == LINK_ISOTP_RESULT_COMPLETE) break;
    }
    return 0;
}
