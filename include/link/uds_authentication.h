// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_authentication.h
 * @brief Typed ISO 14229-1:2020 Authentication (0x29) codec.
 *
 * LINK owns protocol framing and structural validation only. Certificate
 * parsing, trust anchors, private keys, role policy, cryptographic verification
 * and session-key use remain caller-owned.
 */
#ifndef LINK_UDS_AUTHENTICATION_H
#define LINK_UDS_AUTHENTICATION_H

#include "link/uds_services.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_AUTH_DEAUTHENTICATE 0x00U
#define LINK_UDS_AUTH_VERIFY_CERTIFICATE_UNIDIRECTIONAL 0x01U
#define LINK_UDS_AUTH_VERIFY_CERTIFICATE_BIDIRECTIONAL 0x02U
#define LINK_UDS_AUTH_PROOF_OF_OWNERSHIP 0x03U
#define LINK_UDS_AUTH_TRANSMIT_CERTIFICATE 0x04U
#define LINK_UDS_AUTH_REQUEST_CHALLENGE 0x05U
#define LINK_UDS_AUTH_VERIFY_PROOF_UNIDIRECTIONAL 0x06U
#define LINK_UDS_AUTH_VERIFY_PROOF_BIDIRECTIONAL 0x07U
#define LINK_UDS_AUTH_CONFIGURATION 0x08U
#define LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES 16U

#define LINK_UDS_AUTH_RETURN_REQUEST_ACCEPTED 0x00U
#define LINK_UDS_AUTH_RETURN_GENERAL_REJECT 0x01U
#define LINK_UDS_AUTH_RETURN_CONFIGURATION 0x02U
#define LINK_UDS_AUTH_RETURN_CONFIGURATION_ACR_ASYMMETRIC 0x03U
#define LINK_UDS_AUTH_RETURN_CONFIGURATION_ACR_SYMMETRIC 0x04U
#define LINK_UDS_AUTH_RETURN_DEAUTHENTICATED 0x10U
#define LINK_UDS_AUTH_RETURN_CERTIFICATE_VERIFIED_OWNERSHIP_REQUIRED 0x11U
#define LINK_UDS_AUTH_RETURN_OWNERSHIP_VERIFIED_COMPLETE 0x12U
#define LINK_UDS_AUTH_RETURN_CERTIFICATE_VERIFIED 0x13U

typedef struct {
    const uint8_t *data;
    size_t length;
} LinkUdsAuthenticationSpan;

typedef struct {
    uint8_t task;
    uint8_t return_parameter;
    LinkUdsAuthenticationSpan algorithm_indicator;
    LinkUdsAuthenticationSpan challenge_server;
    LinkUdsAuthenticationSpan certificate_server;
    LinkUdsAuthenticationSpan proof_of_ownership_server;
    LinkUdsAuthenticationSpan ephemeral_public_key_server;
    LinkUdsAuthenticationSpan session_key_info;
    LinkUdsAuthenticationSpan needed_additional_parameter;
} LinkUdsAuthenticationResponse;

LinkUdsResult link_uds_build_authentication_deauthenticate_request(
    bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_verify_certificate_request(
    uint8_t task, bool suppress_positive_response,
    uint8_t communication_configuration,
    const uint8_t *certificate_client, size_t certificate_client_length,
    const uint8_t *challenge_client, size_t challenge_client_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_proof_of_ownership_request(
    bool suppress_positive_response,
    const uint8_t *proof_client, size_t proof_client_length,
    const uint8_t *ephemeral_public_key_client,
    size_t ephemeral_public_key_client_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_transmit_certificate_request(
    bool suppress_positive_response, uint16_t certificate_evaluation_id,
    const uint8_t *certificate_data, size_t certificate_data_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_request_challenge_request(
    bool suppress_positive_response, uint8_t communication_configuration,
    const uint8_t algorithm_indicator[LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES],
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_verify_proof_request(
    uint8_t task, bool suppress_positive_response,
    const uint8_t algorithm_indicator[LINK_UDS_AUTH_ALGORITHM_INDICATOR_BYTES],
    const uint8_t *proof_client, size_t proof_client_length,
    const uint8_t *challenge_client, size_t challenge_client_length,
    const uint8_t *additional_parameter, size_t additional_parameter_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_build_authentication_configuration_request(
    bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written);

LinkUdsResult link_uds_decode_authentication_response(
    uint8_t expected_task, const uint8_t *pdu, size_t pdu_length,
    LinkUdsAuthenticationResponse *response);

#ifdef __cplusplus
}
#endif

#endif
