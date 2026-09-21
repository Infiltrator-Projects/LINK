// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_authentication.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
    return 1; } } while (0)

static int test_builders(void)
{
    static const uint8_t cert[] = {0x30U,0x01U,0xaaU};
    static const uint8_t challenge[] = {0x10U,0x20U};
    static const uint8_t proof[] = {0x55U,0x66U,0x77U};
    static const uint8_t ephemeral[] = {0x04U,0x99U};
    static const uint8_t additional[] = {0xdeU,0xadU};
    uint8_t algorithm[LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES];
    uint8_t buffer[128U];
    size_t written = 0U;
    size_t index;

    for (index = 0U; index < sizeof(algorithm); ++index)
        algorithm[index] = (uint8_t)(index + 1U);

    CHECK(link_uds_build_authentication_deauthenticate_request(
              false, buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(written == 2U && buffer[0] == 0x29U && buffer[1] == 0x00U);

    CHECK(link_uds_build_authentication_verify_certificate_request(
              LINK_UDS_AUTH_VERIFY_CERTIFICATE_UNIDIRECTIONAL, false, 0xa5U,
              cert, sizeof(cert), NULL, 0U,
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(written == 10U && buffer[1] == 0x01U && buffer[2] == 0xa5U);

    CHECK(link_uds_build_authentication_verify_certificate_request(
              LINK_UDS_AUTH_VERIFY_CERTIFICATE_BIDIRECTIONAL, false, 0x01U,
              cert, sizeof(cert), challenge, sizeof(challenge),
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(buffer[1] == 0x02U && written == 12U);

    CHECK(link_uds_build_authentication_proof_of_ownership_request(
              false, proof, sizeof(proof), ephemeral, sizeof(ephemeral),
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(buffer[1] == 0x03U);

    CHECK(link_uds_build_authentication_transmit_certificate_request(
              false, 0x1234U, cert, sizeof(cert),
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(buffer[1] == 0x04U && buffer[2] == 0x12U && buffer[3] == 0x34U);

    CHECK(link_uds_build_authentication_request_challenge_request(
              false, 0x22U, algorithm,
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(written == 19U && buffer[1] == 0x05U && buffer[2] == 0x22U);
    CHECK(memcmp(buffer + 3U, algorithm, sizeof(algorithm)) == 0);

    CHECK(link_uds_build_authentication_verify_proof_request(
              LINK_UDS_AUTH_VERIFY_PROOF_UNIDIRECTIONAL, false,
              algorithm, proof, sizeof(proof), NULL, 0U,
              additional, sizeof(additional),
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(buffer[1] == 0x06U);

    CHECK(link_uds_build_authentication_verify_proof_request(
              LINK_UDS_AUTH_VERIFY_PROOF_BIDIRECTIONAL, false,
              algorithm, proof, sizeof(proof), challenge, sizeof(challenge),
              additional, sizeof(additional),
              buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(buffer[1] == 0x07U);

    CHECK(link_uds_build_authentication_configuration_request(
              true, buffer, sizeof(buffer), &written) == LINK_UDS_RESULT_OK);
    CHECK(written == 2U && buffer[1] == 0x88U);
    return 0;
}

static int test_response_decoder(void)
{
    static const uint8_t algorithm[16U] = {
        1U,2U,3U,4U,5U,6U,7U,8U,9U,10U,11U,12U,13U,14U,15U,16U
    };
    const uint8_t deauth[] = {0x69U,0x00U,0x10U};
    const uint8_t cert_uni[] = {
        0x69U,0x01U,0x11U,0x00U,0x02U,0xaaU,0xbbU,0x00U,0x00U
    };
    const uint8_t cert_bi[] = {
        0x69U,0x02U,0x11U,
        0x00U,0x01U,0xa1U,
        0x00U,0x01U,0xb1U,
        0x00U,0x01U,0xc1U,
        0x00U,0x00U
    };
    const uint8_t proof[] = {0x69U,0x03U,0x12U,0x00U,0x02U,0x44U,0x55U};
    const uint8_t transmit[] = {0x69U,0x04U,0x00U};
    uint8_t challenge_pdu[32U] = {0x69U,0x05U,0x00U};
    uint8_t verify_uni[24U] = {0x69U,0x06U,0x12U};
    uint8_t verify_bi[32U] = {0x69U,0x07U,0x12U};
    const uint8_t config[] = {0x69U,0x08U,0x03U};
    const uint8_t malformed[] = {0x69U,0x01U,0x11U,0x00U,0x03U,0xaaU};
    LinkUdsAuthenticationResponse response;
    size_t offset;

    CHECK(link_uds_decode_authentication_response(
              0x00U, deauth, sizeof(deauth), &response) == LINK_UDS_RESULT_OK);
    CHECK(link_uds_decode_authentication_response(
              0x01U, cert_uni, sizeof(cert_uni), &response) == LINK_UDS_RESULT_OK);
    CHECK(response.challenge_server.length == 2U);
    CHECK(link_uds_decode_authentication_response(
              0x02U, cert_bi, sizeof(cert_bi), &response) == LINK_UDS_RESULT_OK);
    CHECK(response.certificate_server.length == 1U);
    CHECK(link_uds_decode_authentication_response(
              0x03U, proof, sizeof(proof), &response) == LINK_UDS_RESULT_OK);
    CHECK(link_uds_decode_authentication_response(
              0x04U, transmit, sizeof(transmit), &response) == LINK_UDS_RESULT_OK);

    offset = 3U;
    memcpy(challenge_pdu + offset, algorithm, sizeof(algorithm));
    offset += sizeof(algorithm);
    challenge_pdu[offset++] = 0U; challenge_pdu[offset++] = 1U;
    challenge_pdu[offset++] = 0xccU;
    challenge_pdu[offset++] = 0U; challenge_pdu[offset++] = 0U;
    CHECK(link_uds_decode_authentication_response(
              0x05U, challenge_pdu, offset, &response) == LINK_UDS_RESULT_OK);

    offset = 3U;
    memcpy(verify_uni + offset, algorithm, sizeof(algorithm));
    offset += sizeof(algorithm);
    verify_uni[offset++] = 0U; verify_uni[offset++] = 1U;
    verify_uni[offset++] = 0x99U;
    CHECK(link_uds_decode_authentication_response(
              0x06U, verify_uni, offset, &response) == LINK_UDS_RESULT_OK);

    offset = 3U;
    memcpy(verify_bi + offset, algorithm, sizeof(algorithm));
    offset += sizeof(algorithm);
    verify_bi[offset++] = 0U; verify_bi[offset++] = 1U;
    verify_bi[offset++] = 0x88U;
    verify_bi[offset++] = 0U; verify_bi[offset++] = 0U;
    CHECK(link_uds_decode_authentication_response(
              0x07U, verify_bi, offset, &response) == LINK_UDS_RESULT_OK);

    CHECK(link_uds_decode_authentication_response(
              0x08U, config, sizeof(config), &response) == LINK_UDS_RESULT_OK);
    CHECK(link_uds_decode_authentication_response(
              0x01U, malformed, sizeof(malformed), &response) ==
          LINK_UDS_RESULT_MALFORMED_PDU);
    CHECK(link_uds_decode_authentication_response(
              0x02U, cert_uni, sizeof(cert_uni), &response) ==
          LINK_UDS_RESULT_UNEXPECTED_RESPONSE);
    return 0;
}

int main(void)
{
    if (test_builders() != 0) return 1;
    if (test_response_decoder() != 0) return 1;
    puts("UDS Authentication tests passed");
    return 0;
}
