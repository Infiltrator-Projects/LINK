// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_native_protocol.h"

#include <stdbool.h>
#include <string.h>

#define AES_NK 8U
#define AES_NR 14U
#define AES_ROUND_KEY_SIZE 240U

typedef struct Aes256Context {
    uint8_t round_keys[AES_ROUND_KEY_SIZE];
    uint8_t sbox[256];
    uint8_t inverse_sbox[256];
} Aes256Context;

/*
 * Clean-room interoperability constant recovered from the literal pool used
 * identically by SeedKeyAction::logIntoAdapter() and
 * ConfigureSecureModeAction::logIntoAdapter() in archived GDK 4.7.61.
 */
static const uint8_t mercedes_me_adapter_auth_key[
    LINK_MERCEDES_ME_ADAPTER_AUTH_KEY_SIZE] = {
    UINT8_C(0x4f), UINT8_C(0xf3), UINT8_C(0x64), UINT8_C(0xf1),
    UINT8_C(0x14), UINT8_C(0xd4), UINT8_C(0x8b), UINT8_C(0xe9),
    UINT8_C(0x36), UINT8_C(0xf9), UINT8_C(0x7b), UINT8_C(0xe6),
    UINT8_C(0x48), UINT8_C(0x2f), UINT8_C(0xa5), UINT8_C(0x6e),
    UINT8_C(0x3d), UINT8_C(0x9c), UINT8_C(0x60), UINT8_C(0x80),
    UINT8_C(0x71), UINT8_C(0xce), UINT8_C(0xfc), UINT8_C(0x20),
    UINT8_C(0x2f), UINT8_C(0xb6), UINT8_C(0xf7), UINT8_C(0xf9),
    UINT8_C(0xa5), UINT8_C(0x86), UINT8_C(0xd1), UINT8_C(0x57)
};

static uint8_t gf_xtime(uint8_t value)
{
    return (uint8_t)((value << 1U) ^
        ((value & UINT8_C(0x80)) != 0U ? UINT8_C(0x1b) : 0U));
}

static uint8_t gf_mul(uint8_t a, uint8_t b)
{
    uint8_t result = 0U;
    while (b != 0U) {
        if ((b & 1U) != 0U) result ^= a;
        a = gf_xtime(a);
        b >>= 1U;
    }
    return result;
}

static uint8_t rotl8(uint8_t value, unsigned int shift)
{
    return (uint8_t)((value << shift) | (value >> (8U - shift)));
}

static uint8_t aes_sbox(uint8_t value)
{
    uint8_t inverse;
    uint8_t result;
    unsigned int exponent;

    if (value == 0U) {
        inverse = 0U;
    } else {
        uint8_t base = value;
        inverse = 1U;
        for (exponent = 254U; exponent != 0U; exponent >>= 1U) {
            if ((exponent & 1U) != 0U) inverse = gf_mul(inverse, base);
            base = gf_mul(base, base);
        }
    }
    result = (uint8_t)(inverse ^ rotl8(inverse, 1U) ^
                       rotl8(inverse, 2U) ^ rotl8(inverse, 3U) ^
                       rotl8(inverse, 4U) ^ UINT8_C(0x63));
    return result;
}

static uint32_t rot_word(uint32_t value)
{
    return (value << 8U) | (value >> 24U);
}

static uint32_t sub_word(uint32_t value, const uint8_t sbox[256])
{
    return ((uint32_t)sbox[(uint8_t)(value >> 24U)] << 24U) |
           ((uint32_t)sbox[(uint8_t)(value >> 16U)] << 16U) |
           ((uint32_t)sbox[(uint8_t)(value >> 8U)] << 8U) |
           (uint32_t)sbox[(uint8_t)value];
}

static uint32_t load_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U) |
           ((uint32_t)bytes[1] << 16U) |
           ((uint32_t)bytes[2] << 8U) |
           (uint32_t)bytes[3];
}

static void store_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static void aes256_init(Aes256Context *context, const uint8_t key[32])
{
    uint32_t words[60];
    unsigned int index;
    uint8_t rcon = 1U;

    for (index = 0U; index < 256U; ++index) {
        const uint8_t substituted = aes_sbox((uint8_t)index);
        context->sbox[index] = substituted;
        context->inverse_sbox[substituted] = (uint8_t)index;
    }
    for (index = 0U; index < AES_NK; ++index)
        words[index] = load_be32(key + index * 4U);
    for (index = AES_NK; index < 60U; ++index) {
        uint32_t temp = words[index - 1U];
        if ((index % AES_NK) == 0U) {
            temp = sub_word(rot_word(temp), context->sbox) ^
                   ((uint32_t)rcon << 24U);
            rcon = gf_xtime(rcon);
        } else if ((index % AES_NK) == 4U) {
            temp = sub_word(temp, context->sbox);
        }
        words[index] = words[index - AES_NK] ^ temp;
    }
    for (index = 0U; index < 60U; ++index)
        store_be32(context->round_keys + index * 4U, words[index]);
    memset(words, 0, sizeof(words));
}

static void add_round_key(uint8_t state[16], const uint8_t *round_key)
{
    unsigned int index;
    for (index = 0U; index < 16U; ++index) state[index] ^= round_key[index];
}

static void sub_bytes(uint8_t state[16], const uint8_t sbox[256])
{
    unsigned int index;
    for (index = 0U; index < 16U; ++index) state[index] = sbox[state[index]];
}

static void inverse_sub_bytes(
    uint8_t state[16],
    const uint8_t inverse_sbox[256])
{
    unsigned int index;
    for (index = 0U; index < 16U; ++index)
        state[index] = inverse_sbox[state[index]];
}

static void shift_rows(uint8_t state[16])
{
    uint8_t copy[16];
    unsigned int row;
    unsigned int column;

    memcpy(copy, state, sizeof(copy));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            state[row + 4U * column] =
                copy[row + 4U * ((column + row) % 4U)];
}

static void inverse_shift_rows(uint8_t state[16])
{
    uint8_t copy[16];
    unsigned int row;
    unsigned int column;

    memcpy(copy, state, sizeof(copy));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            state[row + 4U * column] =
                copy[row + 4U * ((column + 4U - row) % 4U)];
}

static void mix_columns(uint8_t state[16])
{
    unsigned int column;
    for (column = 0U; column < 4U; ++column) {
        const unsigned int i = column * 4U;
        const uint8_t a0 = state[i];
        const uint8_t a1 = state[i + 1U];
        const uint8_t a2 = state[i + 2U];
        const uint8_t a3 = state[i + 3U];
        state[i] = (uint8_t)(gf_mul(a0, 2U) ^ gf_mul(a1, 3U) ^ a2 ^ a3);
        state[i + 1U] =
            (uint8_t)(a0 ^ gf_mul(a1, 2U) ^ gf_mul(a2, 3U) ^ a3);
        state[i + 2U] =
            (uint8_t)(a0 ^ a1 ^ gf_mul(a2, 2U) ^ gf_mul(a3, 3U));
        state[i + 3U] =
            (uint8_t)(gf_mul(a0, 3U) ^ a1 ^ a2 ^ gf_mul(a3, 2U));
    }
}

static void inverse_mix_columns(uint8_t state[16])
{
    unsigned int column;
    for (column = 0U; column < 4U; ++column) {
        const unsigned int i = column * 4U;
        const uint8_t a0 = state[i];
        const uint8_t a1 = state[i + 1U];
        const uint8_t a2 = state[i + 2U];
        const uint8_t a3 = state[i + 3U];
        state[i] = (uint8_t)(gf_mul(a0, 14U) ^ gf_mul(a1, 11U) ^
                             gf_mul(a2, 13U) ^ gf_mul(a3, 9U));
        state[i + 1U] =
            (uint8_t)(gf_mul(a0, 9U) ^ gf_mul(a1, 14U) ^
                      gf_mul(a2, 11U) ^ gf_mul(a3, 13U));
        state[i + 2U] =
            (uint8_t)(gf_mul(a0, 13U) ^ gf_mul(a1, 9U) ^
                      gf_mul(a2, 14U) ^ gf_mul(a3, 11U));
        state[i + 3U] =
            (uint8_t)(gf_mul(a0, 11U) ^ gf_mul(a1, 13U) ^
                      gf_mul(a2, 9U) ^ gf_mul(a3, 14U));
    }
}

static void aes256_encrypt_block(
    const Aes256Context *context,
    const uint8_t input[16],
    uint8_t output[16])
{
    uint8_t state[16];
    unsigned int round;

    memcpy(state, input, sizeof(state));
    add_round_key(state, context->round_keys);
    for (round = 1U; round < AES_NR; ++round) {
        sub_bytes(state, context->sbox);
        shift_rows(state);
        mix_columns(state);
        add_round_key(state, context->round_keys + round * 16U);
    }
    sub_bytes(state, context->sbox);
    shift_rows(state);
    add_round_key(state, context->round_keys + AES_NR * 16U);
    memcpy(output, state, sizeof(state));
}

static void aes256_decrypt_block(
    const Aes256Context *context,
    const uint8_t input[16],
    uint8_t output[16])
{
    uint8_t state[16];
    unsigned int round;

    memcpy(state, input, sizeof(state));
    add_round_key(state, context->round_keys + AES_NR * 16U);
    for (round = AES_NR; round > 1U; --round) {
        inverse_shift_rows(state);
        inverse_sub_bytes(state, context->inverse_sbox);
        add_round_key(state, context->round_keys + (round - 1U) * 16U);
        inverse_mix_columns(state);
    }
    inverse_shift_rows(state);
    inverse_sub_bytes(state, context->inverse_sbox);
    add_round_key(state, context->round_keys);
    memcpy(output, state, sizeof(state));
}

static uint32_t sha_rotr(uint32_t value, unsigned int bits)
{
    return (value >> bits) | (value << (32U - bits));
}

static void sha256_transform(uint32_t state[8], const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
        0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
        0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
        0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
        0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
        0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
        0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
        0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
        0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t words[64];
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
    uint32_t e;
    uint32_t f;
    uint32_t g;
    uint32_t h;
    unsigned int index;

    for (index = 0U; index < 16U; ++index)
        words[index] = load_be32(block + index * 4U);
    for (index = 16U; index < 64U; ++index) {
        const uint32_t s0 =
            sha_rotr(words[index - 15U], 7U) ^
            sha_rotr(words[index - 15U], 18U) ^
            (words[index - 15U] >> 3U);
        const uint32_t s1 =
            sha_rotr(words[index - 2U], 17U) ^
            sha_rotr(words[index - 2U], 19U) ^
            (words[index - 2U] >> 10U);
        words[index] =
            words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];
    for (index = 0U; index < 64U; ++index) {
        const uint32_t s1 =
            sha_rotr(e, 6U) ^ sha_rotr(e, 11U) ^ sha_rotr(e, 25U);
        const uint32_t choose = (e & f) ^ ((~e) & g);
        const uint32_t t1 =
            h + s1 + choose + constants[index] + words[index];
        const uint32_t s0 =
            sha_rotr(a, 2U) ^ sha_rotr(a, 13U) ^ sha_rotr(a, 22U);
        const uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = s0 + majority;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    memset(words, 0, sizeof(words));
}

static void sha256_bytes(
    const uint8_t *bytes,
    size_t size,
    uint8_t digest[32])
{
    uint32_t state[8] = {
        0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U
    };
    uint8_t block[64];
    size_t offset = 0U;
    const uint64_t bit_length = (uint64_t)size * UINT64_C(8);
    unsigned int index;

    while (size - offset >= 64U) {
        sha256_transform(state, bytes + offset);
        offset += 64U;
    }
    memset(block, 0, sizeof(block));
    if (size > offset) memcpy(block, bytes + offset, size - offset);
    block[size - offset] = UINT8_C(0x80);
    if (size - offset >= 56U) {
        sha256_transform(state, block);
        memset(block, 0, sizeof(block));
    }
    for (index = 0U; index < 8U; ++index)
        block[63U - index] =
            (uint8_t)(bit_length >> (index * 8U));
    sha256_transform(state, block);
    for (index = 0U; index < 8U; ++index)
        store_be32(digest + index * 4U, state[index]);
    memset(block, 0, sizeof(block));
    memset(state, 0, sizeof(state));
}

static const char base64_alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t base64_encoded_size(size_t input_size)
{
    return ((input_size + 2U) / 3U) * 4U;
}

static bool base64_encode(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t capacity,
    size_t *output_size)
{
    size_t input_offset = 0U;
    size_t output_offset = 0U;
    const size_t needed = base64_encoded_size(input_size);

    if (capacity < needed) return false;
    while (input_offset < input_size) {
        const size_t remaining = input_size - input_offset;
        const uint32_t a = input[input_offset++];
        const uint32_t b =
            remaining > 1U ? input[input_offset++] : 0U;
        const uint32_t c =
            remaining > 2U ? input[input_offset++] : 0U;
        const uint32_t triple = (a << 16U) | (b << 8U) | c;
        output[output_offset++] =
            (uint8_t)base64_alphabet[(triple >> 18U) & 0x3fU];
        output[output_offset++] =
            (uint8_t)base64_alphabet[(triple >> 12U) & 0x3fU];
        output[output_offset++] = remaining > 1U
            ? (uint8_t)base64_alphabet[(triple >> 6U) & 0x3fU]
            : (uint8_t)'=';
        output[output_offset++] = remaining > 2U
            ? (uint8_t)base64_alphabet[triple & 0x3fU]
            : (uint8_t)'=';
    }
    if (output_size != NULL) *output_size = output_offset;
    return true;
}

static int base64_value(uint8_t value)
{
    if (value >= (uint8_t)'A' && value <= (uint8_t)'Z')
        return (int)(value - (uint8_t)'A');
    if (value >= (uint8_t)'a' && value <= (uint8_t)'z')
        return (int)(value - (uint8_t)'a') + 26;
    if (value >= (uint8_t)'0' && value <= (uint8_t)'9')
        return (int)(value - (uint8_t)'0') + 52;
    if (value == (uint8_t)'+') return 62;
    if (value == (uint8_t)'/') return 63;
    return -1;
}

static bool base64_decode(
    const uint8_t *input,
    size_t input_size,
    uint8_t *output,
    size_t capacity,
    size_t *output_size)
{
    size_t input_offset = 0U;
    size_t output_offset = 0U;

    if (input_size == 0U || (input_size % 4U) != 0U) return false;
    while (input_offset < input_size) {
        const int v0 = base64_value(input[input_offset]);
        const int v1 = base64_value(input[input_offset + 1U]);
        const int v2 = input[input_offset + 2U] == (uint8_t)'='
            ? -2 : base64_value(input[input_offset + 2U]);
        const int v3 = input[input_offset + 3U] == (uint8_t)'='
            ? -2 : base64_value(input[input_offset + 3U]);
        const bool final = input_offset + 4U == input_size;
        uint32_t triple;

        if (v0 < 0 || v1 < 0 || v2 == -1 || v3 == -1) return false;
        if (v2 == -2 && (v3 != -2 || !final)) return false;
        if (v3 == -2 && !final) return false;
        triple = ((uint32_t)v0 << 18U) |
                 ((uint32_t)v1 << 12U) |
                 ((uint32_t)(v2 < 0 ? 0 : v2) << 6U) |
                 (uint32_t)(v3 < 0 ? 0 : v3);
        if (output_offset >= capacity) return false;
        output[output_offset++] = (uint8_t)(triple >> 16U);
        if (v2 >= 0) {
            if (output_offset >= capacity) return false;
            output[output_offset++] = (uint8_t)(triple >> 8U);
        }
        if (v3 >= 0) {
            if (output_offset >= capacity) return false;
            output[output_offset++] = (uint8_t)triple;
        }
        input_offset += 4U;
    }
    if (output_size != NULL) *output_size = output_offset;
    return true;
}

const char *link_mercedes_me_native_result_name(
    LinkMercedesMeNativeResult result)
{
    switch (result) {
    case LINK_MERCEDES_ME_NATIVE_OK: return "ok";
    case LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT:
        return "invalid-argument";
    case LINK_MERCEDES_ME_NATIVE_RANGE: return "range";
    case LINK_MERCEDES_ME_NATIVE_CAPACITY: return "capacity";
    case LINK_MERCEDES_ME_NATIVE_MALFORMED: return "malformed";
    case LINK_MERCEDES_ME_NATIVE_BASE64: return "base64";
    case LINK_MERCEDES_ME_NATIVE_CRC_MISMATCH: return "crc-mismatch";
    }
    return "unknown";
}

uint16_t link_mercedes_me_crc16_ccitt(
    const uint8_t *bytes,
    size_t size)
{
    uint16_t crc = 0U;
    size_t index;
    unsigned int bit;

    if (bytes == NULL && size != 0U) return 0U;
    for (index = 0U; index < size; ++index) {
        crc ^= (uint16_t)((uint16_t)bytes[index] << 8U);
        for (bit = 0U; bit < 8U; ++bit) {
            if ((crc & UINT16_C(0x8000)) != 0U)
                crc = (uint16_t)((uint16_t)(crc << 1U) ^
                                 UINT16_C(0x1021));
            else
                crc = (uint16_t)(crc << 1U);
        }
    }
    return crc;
}

size_t link_mercedes_me_secure_ciphertext_size(size_t plaintext_size)
{
    if (plaintext_size > LINK_MERCEDES_ME_SECURE_MAX_PLAINTEXT)
        return 0U;
    return (plaintext_size + 22U) & ~(size_t)15U;
}

LinkMercedesMeNativeResult link_mercedes_me_derive_session_key(
    const uint8_t session_master_key[LINK_MERCEDES_ME_SESSION_MASTER_KEY_SIZE],
    const uint8_t random_argument_1[LINK_MERCEDES_ME_SESSION_RANDOM_SIZE],
    const uint8_t random_argument_2[LINK_MERCEDES_ME_SESSION_RANDOM_SIZE],
    uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE])
{
    uint8_t material[64];

    if (session_master_key == NULL || random_argument_1 == NULL ||
        random_argument_2 == NULL || session_key == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    memcpy(material, session_master_key, 32U);
    memcpy(material + 32U, random_argument_1, 16U);
    memcpy(material + 48U, random_argument_2, 16U);
    sha256_bytes(material, sizeof(material), session_key);
    memset(material, 0, sizeof(material));
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_derive_secure_session_key(
    const uint8_t session_master_key[LINK_MERCEDES_ME_SESSION_MASTER_KEY_SIZE],
    const uint8_t device_random[LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE],
    const uint8_t app_random[LINK_MERCEDES_ME_APP_RANDOM_SIZE],
    uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE])
{
    return link_mercedes_me_derive_session_key(
        session_master_key, device_random, app_random, session_key);
}

LinkMercedesMeNativeResult link_mercedes_me_authentication_response(
    const uint8_t device_random[LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE],
    uint8_t response[LINK_MERCEDES_ME_AUTH_RESPONSE_SIZE])
{
    Aes256Context aes;

    if (device_random == NULL || response == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    aes256_init(&aes, mercedes_me_adapter_auth_key);
    aes256_encrypt_block(&aes, device_random, response);
    memset(&aes, 0, sizeof(aes));
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_secure_encode(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *plaintext,
    size_t plaintext_size,
    uint8_t *wire,
    size_t wire_capacity,
    size_t *wire_size)
{
    Aes256Context aes;
    uint8_t inner[LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT];
    uint8_t encrypted[LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT];
    size_t ciphertext_size;
    size_t encoded_size;
    size_t offset;
    uint16_t crc;

    if (wire_size != NULL) *wire_size = 0U;
    if (session_key == NULL || wire == NULL ||
        (plaintext == NULL && plaintext_size != 0U))
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    ciphertext_size =
        link_mercedes_me_secure_ciphertext_size(plaintext_size);
    if (ciphertext_size == 0U)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    encoded_size = base64_encoded_size(ciphertext_size);
    if (wire_capacity < 1U + encoded_size + 1U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;

    memset(inner, 0, ciphertext_size);
    crc = link_mercedes_me_crc16_ccitt(plaintext, plaintext_size);
    inner[0] = (uint8_t)(plaintext_size >> 8U);
    inner[1] = (uint8_t)plaintext_size;
    inner[2] = (uint8_t)(crc >> 8U);
    inner[3] = (uint8_t)crc;
    if (plaintext_size != 0U)
        memcpy(inner + LINK_MERCEDES_ME_SECURE_HEADER_SIZE,
               plaintext, plaintext_size);
    aes256_init(&aes, session_key);
    for (offset = 0U; offset < ciphertext_size; offset += 16U)
        aes256_encrypt_block(&aes, inner + offset, encrypted + offset);

    wire[0] = (uint8_t)LINK_MERCEDES_ME_CMD_SECURE;
    if (!base64_encode(encrypted, ciphertext_size, wire + 1U,
                       wire_capacity - 2U, &encoded_size))
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    wire[1U + encoded_size] = UINT8_C(0x0d);
    if (wire_size != NULL) *wire_size = encoded_size + 2U;
    memset(&aes, 0, sizeof(aes));
    memset(inner, 0, sizeof(inner));
    memset(encrypted, 0, sizeof(encrypted));
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_secure_decode(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *wire,
    size_t wire_size,
    uint8_t *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_size)
{
    Aes256Context aes;
    uint8_t encrypted[LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT];
    uint8_t inner[LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT];
    size_t ciphertext_size;
    size_t offset;
    size_t payload_size;
    uint16_t expected_crc;
    uint16_t actual_crc;

    if (plaintext_size != NULL) *plaintext_size = 0U;
    if (session_key == NULL || wire == NULL || plaintext == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (wire_size < 6U ||
        wire[0] != (uint8_t)LINK_MERCEDES_ME_CMD_SECURE ||
        wire[wire_size - 1U] != UINT8_C(0x0d))
        return LINK_MERCEDES_ME_NATIVE_MALFORMED;
    if (!base64_decode(wire + 1U, wire_size - 2U, encrypted,
                       sizeof(encrypted), &ciphertext_size))
        return LINK_MERCEDES_ME_NATIVE_BASE64;
    if (ciphertext_size == 0U ||
        ciphertext_size > LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT ||
        (ciphertext_size % LINK_MERCEDES_ME_AES_BLOCK_SIZE) != 0U)
        return LINK_MERCEDES_ME_NATIVE_MALFORMED;

    aes256_init(&aes, session_key);
    for (offset = 0U; offset < ciphertext_size; offset += 16U)
        aes256_decrypt_block(&aes, encrypted + offset, inner + offset);
    payload_size = ((size_t)inner[0] << 8U) | (size_t)inner[1];
    expected_crc =
        (uint16_t)(((uint16_t)inner[2] << 8U) | inner[3]);
    if (payload_size > LINK_MERCEDES_ME_SECURE_MAX_PLAINTEXT ||
        payload_size + LINK_MERCEDES_ME_SECURE_HEADER_SIZE >
            ciphertext_size)
        return LINK_MERCEDES_ME_NATIVE_MALFORMED;
    actual_crc = link_mercedes_me_crc16_ccitt(
        inner + LINK_MERCEDES_ME_SECURE_HEADER_SIZE, payload_size);
    if (actual_crc != expected_crc)
        return LINK_MERCEDES_ME_NATIVE_CRC_MISMATCH;
    if (plaintext_capacity < payload_size)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    if (payload_size != 0U)
        memcpy(plaintext,
               inner + LINK_MERCEDES_ME_SECURE_HEADER_SIZE,
               payload_size);
    if (plaintext_size != NULL) *plaintext_size = payload_size;
    memset(&aes, 0, sizeof(aes));
    memset(inner, 0, sizeof(inner));
    memset(encrypted, 0, sizeof(encrypted));
    return LINK_MERCEDES_ME_NATIVE_OK;
}

static LinkMercedesMeNativeResult build_simple(
    uint8_t identifier,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    if (out_size != NULL) *out_size = 0U;
    if (out == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (capacity < 2U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    out[0] = identifier;
    out[1] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = 2U;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_can_open(
    uint8_t *out, size_t capacity, size_t *out_size)
{
    return build_simple(
        (uint8_t)LINK_MERCEDES_ME_CMD_CAN_OPEN,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_can_close(
    uint8_t *out, size_t capacity, size_t *out_size)
{
    return build_simple(
        (uint8_t)LINK_MERCEDES_ME_CMD_CAN_CLOSE,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_status(
    uint8_t *out, size_t capacity, size_t *out_size)
{
    return build_simple(
        (uint8_t)LINK_MERCEDES_ME_CMD_STATUS,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_hw_info(
    uint8_t *out, size_t capacity, size_t *out_size)
{
    return build_simple(
        (uint8_t)LINK_MERCEDES_ME_CMD_HW_INFO,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_get_passkey(
    uint8_t *out, size_t capacity, size_t *out_size)
{
    return build_simple(
        (uint8_t)LINK_MERCEDES_ME_CMD_GET_PASSKEY,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_set_baudrate(
    unsigned int baud_ordinal,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    if (out_size != NULL) *out_size = 0U;
    if (out == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (baud_ordinal > 8U)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    if (capacity < 3U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    out[0] = (uint8_t)LINK_MERCEDES_ME_CMD_SET_BAUD;
    out[1] = (uint8_t)((unsigned int)'0' + baud_ordinal);
    out[2] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = 3U;
    return LINK_MERCEDES_ME_NATIVE_OK;
}


static const char native_hex_upper[] = "0123456789ABCDEF";

static void write_fixed_hex(
    unsigned int value,
    unsigned int width,
    uint8_t *out)
{
    unsigned int index;
    for (index = 0U; index < width; ++index) {
        const unsigned int shift = (width - index - 1U) * 4U;
        out[index] = (uint8_t)native_hex_upper[(value >> shift) & 0x0fU];
    }
}

LinkMercedesMeNativeResult link_mercedes_me_build_raw_can(
    unsigned int can_id,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    size_t needed;
    size_t index;
    size_t offset;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL || (payload == NULL && payload_size != 0U))
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (can_id > LINK_MERCEDES_ME_CAN_ID_MAX ||
        payload_size > LINK_MERCEDES_ME_RAW_CAN_MAX_PAYLOAD)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    needed = 1U + 3U + 1U + payload_size * 2U + 1U;
    if (capacity < needed)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;

    offset = 0U;
    out[offset++] = (uint8_t)LINK_MERCEDES_ME_CMD_RAW_CAN;
    write_fixed_hex(can_id, 3U, out + offset);
    offset += 3U;
    out[offset++] = (uint8_t)native_hex_upper[payload_size & 0x0fU];
    for (index = 0U; index < payload_size; ++index) {
        out[offset++] = (uint8_t)native_hex_upper[payload[index] >> 4U];
        out[offset++] = (uint8_t)native_hex_upper[payload[index] & 0x0fU];
    }
    out[offset++] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = offset;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_isotp_config(
    unsigned int request_can_id,
    unsigned int response_can_id,
    int allow_raw_can_responses,
    int padding,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    unsigned int flags;
    unsigned int wire_padding;
    size_t offset = 0U;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (request_can_id > LINK_MERCEDES_ME_CAN_ID_MAX ||
        response_can_id > LINK_MERCEDES_ME_CAN_ID_MAX ||
        padding < LINK_MERCEDES_ME_ISOTP_PADDING_OFF ||
        padding > 255)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    if (capacity < 16U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;

    flags = allow_raw_can_responses != 0 ? 0U : 0x80U;
    if (padding == LINK_MERCEDES_ME_ISOTP_PADDING_OFF) {
        wire_padding = LINK_MERCEDES_ME_ISOTP_PADDING_OFF_WIRE;
    } else {
        flags |= 0x01U;
        wire_padding = (unsigned int)padding;
    }

    out[offset++] = (uint8_t)LINK_MERCEDES_ME_CMD_ISOTP_CONFIG;
    out[offset++] = (uint8_t)LINK_MERCEDES_ME_ISOTP_COMMAND_VERSION[0];
    out[offset++] = (uint8_t)LINK_MERCEDES_ME_ISOTP_COMMAND_VERSION[1];
    write_fixed_hex(request_can_id, 4U, out + offset);
    offset += 4U;
    write_fixed_hex(response_can_id, 4U, out + offset);
    offset += 4U;
    write_fixed_hex(flags, 2U, out + offset);
    offset += 2U;
    write_fixed_hex(wire_padding, 2U, out + offset);
    offset += 2U;
    out[offset++] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = offset;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_isotp_transceive(
    unsigned int request_can_id,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    size_t encoded_size;
    size_t needed;
    size_t offset = 0U;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL || (payload == NULL && payload_size != 0U))
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (request_can_id > LINK_MERCEDES_ME_CAN_ID_MAX ||
        payload_size > LINK_MERCEDES_ME_ISOTP_MAX_PAYLOAD)
        return LINK_MERCEDES_ME_NATIVE_RANGE;

    encoded_size = base64_encoded_size(payload_size);
    needed = 1U + 2U + 4U + encoded_size + 1U;
    if (capacity < needed)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;

    out[offset++] = (uint8_t)LINK_MERCEDES_ME_CMD_ISOTP_TRANSCEIVE;
    out[offset++] = (uint8_t)LINK_MERCEDES_ME_ISOTP_COMMAND_VERSION[0];
    out[offset++] = (uint8_t)LINK_MERCEDES_ME_ISOTP_COMMAND_VERSION[1];
    write_fixed_hex(request_can_id, 4U, out + offset);
    offset += 4U;
    if (!base64_encode(payload, payload_size, out + offset,
                       capacity - offset - 1U, &encoded_size))
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    offset += encoded_size;
    out[offset++] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = offset;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

static LinkMercedesMeNativeResult build_base64_command(
    uint8_t identifier,
    const uint8_t *payload,
    size_t payload_size,
    bool allow_empty,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    size_t encoded_size;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL || (payload == NULL && payload_size != 0U))
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (payload_size == 0U && allow_empty)
        return build_simple(identifier, out, capacity, out_size);
    encoded_size = base64_encoded_size(payload_size);
    if (capacity < 1U + encoded_size + 1U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    out[0] = identifier;
    if (!base64_encode(
            payload, payload_size, out + 1U,
            capacity - 2U, &encoded_size))
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    out[1U + encoded_size] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = encoded_size + 2U;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_get_seed(
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    return build_base64_command(
        (uint8_t)LINK_MERCEDES_ME_CMD_GET_SEED,
        payload, payload_size, true,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_set_key(
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    return build_base64_command(
        (uint8_t)LINK_MERCEDES_ME_CMD_SET_KEY,
        payload, payload_size, false,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_login_set_key(
    const uint8_t device_random[LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE],
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    uint8_t response[LINK_MERCEDES_ME_AUTH_RESPONSE_SIZE];
    LinkMercedesMeNativeResult result;

    if (device_random == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    result = link_mercedes_me_authentication_response(
        device_random, response);
    if (result != LINK_MERCEDES_ME_NATIVE_OK) return result;
    result = link_mercedes_me_build_set_key(
        response, sizeof(response), out, capacity, out_size);
    memset(response, 0, sizeof(response));
    return result;
}

LinkMercedesMeNativeResult link_mercedes_me_build_legacy_seed_request(
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    return link_mercedes_me_build_get_seed(
        NULL, 0U, out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_build_secure_seed_request(
    const uint8_t app_random[LINK_MERCEDES_ME_APP_RANDOM_SIZE],
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    if (app_random == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    return link_mercedes_me_build_get_seed(
        app_random, LINK_MERCEDES_ME_APP_RANDOM_SIZE,
        out, capacity, out_size);
}

LinkMercedesMeNativeResult link_mercedes_me_parse_seed_response(
    const uint8_t *wire,
    size_t wire_size,
    uint8_t device_random[LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE])
{
    size_t decoded_size = 0U;

    if (wire == NULL || device_random == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (wire_size < 3U ||
        wire[0] != (uint8_t)LINK_MERCEDES_ME_CMD_GET_SEED ||
        wire[wire_size - 1U] != UINT8_C(0x0d)) {
        return LINK_MERCEDES_ME_NATIVE_MALFORMED;
    }
    if (!base64_decode(
            wire + 1U, wire_size - 2U,
            device_random, LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE,
            &decoded_size)) {
        memset(device_random, 0, LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE);
        return LINK_MERCEDES_ME_NATIVE_BASE64;
    }
    if (decoded_size != LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE) {
        memset(device_random, 0, LINK_MERCEDES_ME_DEVICE_RANDOM_SIZE);
        return LINK_MERCEDES_ME_NATIVE_MALFORMED;
    }
    return LINK_MERCEDES_ME_NATIVE_OK;
}

static LinkMercedesMeNativeResult write_x_prefix(
    unsigned int mode,
    uint8_t *out,
    size_t capacity)
{
    static const char hex[] = "0123456789ABCDEF";

    if (mode < LINK_MERCEDES_ME_X_MODE_MIN ||
        mode > LINK_MERCEDES_ME_X_MODE_MAX)
        return LINK_MERCEDES_ME_NATIVE_RANGE;
    if (capacity < 3U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    out[0] = (uint8_t)LINK_MERCEDES_ME_CMD_X;
    out[1] = (uint8_t)hex[(mode >> 4U) & 0x0fU];
    out[2] = (uint8_t)hex[mode & 0x0fU];
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_get_x(
    unsigned int mode,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    LinkMercedesMeNativeResult result;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL)
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (capacity < 4U)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    result = write_x_prefix(mode, out, capacity);
    if (result != LINK_MERCEDES_ME_NATIVE_OK) return result;
    out[3] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = 4U;
    return LINK_MERCEDES_ME_NATIVE_OK;
}

LinkMercedesMeNativeResult link_mercedes_me_build_set_x(
    unsigned int mode,
    const uint8_t *payload,
    size_t payload_size,
    uint8_t *out,
    size_t capacity,
    size_t *out_size)
{
    LinkMercedesMeNativeResult result;

    if (out_size != NULL) *out_size = 0U;
    if (out == NULL || (payload == NULL && payload_size != 0U))
        return LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT;
    if (capacity < 4U + payload_size)
        return LINK_MERCEDES_ME_NATIVE_CAPACITY;
    result = write_x_prefix(mode, out, capacity);
    if (result != LINK_MERCEDES_ME_NATIVE_OK) return result;
    if (payload_size != 0U) memcpy(out + 3U, payload, payload_size);
    out[3U + payload_size] = UINT8_C(0x0d);
    if (out_size != NULL) *out_size = 4U + payload_size;
    return LINK_MERCEDES_ME_NATIVE_OK;
}
