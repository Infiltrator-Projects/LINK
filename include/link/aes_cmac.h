// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file aes_cmac.h
 * @brief Portable allocation-free AES-128 and RFC 4493 AES-CMAC primitives.
 *
 * This module provides protocol-neutral cryptographic mechanics only. It does
 * not define any vehicle, ECU, UDS security level or seed/key derivation rule.
 */
#ifndef LINK_AES_CMAC_H
#define LINK_AES_CMAC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_AES128_KEY_BYTES 16U
#define LINK_AES128_BLOCK_BYTES 16U
#define LINK_AES_CMAC_TAG_BYTES 16U

bool link_aes128_encrypt_block(
    const uint8_t key[LINK_AES128_KEY_BYTES],
    const uint8_t input[LINK_AES128_BLOCK_BYTES],
    uint8_t output[LINK_AES128_BLOCK_BYTES]);

bool link_aes_cmac_128(
    const uint8_t key[LINK_AES128_KEY_BYTES],
    const uint8_t *message,
    size_t message_length,
    uint8_t tag[LINK_AES_CMAC_TAG_BYTES]);

bool link_aes_cmac_128_verify(
    const uint8_t key[LINK_AES128_KEY_BYTES],
    const uint8_t *message,
    size_t message_length,
    const uint8_t expected_tag[LINK_AES_CMAC_TAG_BYTES]);

#ifdef __cplusplus
}
#endif

#endif
