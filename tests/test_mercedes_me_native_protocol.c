// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_native_protocol.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", \
                #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    uint8_t smk[32];
    uint8_t random1[16];
    uint8_t random2[16];
    uint8_t key[32];
    uint8_t wire[700];
    uint8_t decoded[600];
    uint8_t command[100];
    size_t wire_size;
    size_t decoded_size;
    size_t command_size;
    unsigned int index;
    static const uint8_t plaintext[] = {'A','B','C','\r'};
    static const uint8_t expected_key[32] = {
        0xfd,0xea,0xb9,0xac,0xf3,0x71,0x03,0x62,
        0xbd,0x26,0x58,0xcd,0xc9,0xa2,0x9e,0x8f,
        0x9c,0x75,0x7f,0xcf,0x98,0x11,0x60,0x3a,
        0x8c,0x44,0x7c,0xd1,0xd9,0x15,0x11,0x08
    };
    static const uint8_t expected_wire[] =
        "aq/UUM2JUmn6X/ABmJ//LrQ==\r";

    CHECK(LINK_MERCEDES_ME_DEFAULT_BAUD_ORDINAL == 6U);
    CHECK(LINK_MERCEDES_ME_ADAPTER_VIN_MIN_LENGTH == 1U);
    CHECK(LINK_MERCEDES_ME_ADAPTER_VIN_MAX_LENGTH == 17U);
    CHECK(link_mercedes_me_crc16_ccitt(
              (const uint8_t *)"123456789", 9U) == UINT16_C(0x31c3));

    for (index = 0U; index < 32U; ++index) smk[index] = (uint8_t)index;
    for (index = 0U; index < 16U; ++index) {
        random1[index] = (uint8_t)(0x20U + index);
        random2[index] = (uint8_t)(0x30U + index);
    }
    CHECK(link_mercedes_me_derive_session_key(
              smk, random1, random2, key) == LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(memcmp(key, expected_key, sizeof(key)) == 0);

    CHECK(link_mercedes_me_secure_encode(
              key, plaintext, sizeof(plaintext),
              wire, sizeof(wire), &wire_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(wire_size == sizeof(expected_wire) - 1U);
    CHECK(memcmp(wire, expected_wire, wire_size) == 0);
    CHECK(link_mercedes_me_secure_decode(
              key, wire, wire_size, decoded, sizeof(decoded),
              &decoded_size) == LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(decoded_size == sizeof(plaintext));
    CHECK(memcmp(decoded, plaintext, sizeof(plaintext)) == 0);

    CHECK(link_mercedes_me_secure_ciphertext_size(505U) == 512U);
    CHECK(link_mercedes_me_secure_ciphertext_size(506U) == 0U);

    CHECK(link_mercedes_me_build_can_open(
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "O\r", 2U) == 0);
    CHECK(link_mercedes_me_build_can_close(
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "C\r", 2U) == 0);
    CHECK(link_mercedes_me_build_status(
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "V\r", 2U) == 0);
    CHECK(link_mercedes_me_build_hw_info(
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "N\r", 2U) == 0);
    CHECK(link_mercedes_me_build_get_passkey(
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "p\r", 2U) == 0);
    CHECK(link_mercedes_me_build_set_baudrate(
              LINK_MERCEDES_ME_DEFAULT_BAUD_ORDINAL,
              command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 3U && memcmp(command, "S6\r", 3U) == 0);
    CHECK(link_mercedes_me_build_set_baudrate(
              9U, command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_RANGE);

    CHECK(link_mercedes_me_build_get_seed(
              NULL, 0U, command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "y\r", 2U) == 0);
    CHECK(link_mercedes_me_build_set_key(
              NULL, 0U, command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 2U && memcmp(command, "Y\r", 2U) == 0);
    {
        static const uint8_t payload[] = {1U, 2U, 3U};
        CHECK(link_mercedes_me_build_get_seed(
                  payload, sizeof(payload), command, sizeof(command),
                  &command_size) == LINK_MERCEDES_ME_NATIVE_OK);
        CHECK(command_size == 6U &&
              memcmp(command, "yAQID\r", 6U) == 0);
        CHECK(link_mercedes_me_build_set_key(
                  payload, sizeof(payload), command, sizeof(command),
                  &command_size) == LINK_MERCEDES_ME_NATIVE_OK);
        CHECK(command_size == 6U &&
              memcmp(command, "YAQID\r", 6U) == 0);
    }

    CHECK(link_mercedes_me_build_get_x(
              0x19U, command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_OK);
    CHECK(command_size == 4U && memcmp(command, "X19\r", 4U) == 0);
    CHECK(link_mercedes_me_build_get_x(
              0x2aU, command, sizeof(command), &command_size) ==
          LINK_MERCEDES_ME_NATIVE_RANGE);
    {
        static const uint8_t payload[] = {'1'};
        CHECK(link_mercedes_me_build_set_x(
                  0x29U, payload, sizeof(payload),
                  command, sizeof(command), &command_size) ==
              LINK_MERCEDES_ME_NATIVE_OK);
        CHECK(command_size == 5U &&
              memcmp(command, "X291\r", 5U) == 0);
    }

    CHECK(strcmp(link_mercedes_me_native_result_name(
                     LINK_MERCEDES_ME_NATIVE_CRC_MISMATCH),
                 "crc-mismatch") == 0);
    return 0;
}
