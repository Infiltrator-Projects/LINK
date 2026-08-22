// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds.h
 * @brief Portable ISO 14229 UDS request/response and client-state foundation.
 *
 * UDS consumes complete diagnostic PDUs. ISO-TP/CAN segmentation and concrete
 * transports remain outside this layer. All response data pointers are borrowed
 * from the caller-supplied PDU and remain valid only while that PDU is valid.
 */
#ifndef LINK_UDS_H
#define LINK_UDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL 0x10U
#define LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER 0x22U
#define LINK_UDS_SERVICE_TESTER_PRESENT 0x3eU
#define LINK_UDS_SERVICE_NEGATIVE_RESPONSE 0x7fU

#define LINK_UDS_SESSION_DEFAULT 0x01U
#define LINK_UDS_SESSION_PROGRAMMING 0x02U
#define LINK_UDS_SESSION_EXTENDED 0x03U
#define LINK_UDS_SESSION_SAFETY_SYSTEM 0x04U

#define LINK_UDS_NRC_RESPONSE_PENDING 0x78U

typedef enum {
    LINK_UDS_RESULT_OK = 0,
    LINK_UDS_RESULT_COMPLETE,
    LINK_UDS_RESULT_WAITING,
    LINK_UDS_RESULT_RESPONSE_PENDING,
    LINK_UDS_RESULT_NEGATIVE_RESPONSE,
    LINK_UDS_RESULT_INVALID_ARGUMENT,
    LINK_UDS_RESULT_BUFFER_TOO_SMALL,
    LINK_UDS_RESULT_MALFORMED_PDU,
    LINK_UDS_RESULT_UNEXPECTED_RESPONSE,
    LINK_UDS_RESULT_BUSY,
    LINK_UDS_RESULT_TIMEOUT,
    LINK_UDS_RESULT_FAILED_STATE,
    LINK_UDS_RESULT_UNSUPPORTED
} LinkUdsResult;

typedef enum {
    LINK_UDS_RESPONSE_POSITIVE = 0,
    LINK_UDS_RESPONSE_NEGATIVE
} LinkUdsResponseKind;

typedef struct {
    LinkUdsResponseKind kind;
    uint8_t request_service;
    uint8_t response_service;
    uint8_t negative_response_code;
    const uint8_t *data;
    size_t data_length;
} LinkUdsResponse;

typedef struct {
    uint8_t session_type;
    bool timing_present;
    uint16_t p2_server_max_ms;
    uint16_t p2_star_server_max_10ms;
} LinkUdsSessionResponse;

typedef struct {
    uint16_t identifier;
    const uint8_t *data;
    size_t data_length;
} LinkUdsDidRecord;

typedef struct {
    uint16_t identifier;
    const char *key;
    const char *name;
    size_t minimum_length;
    size_t maximum_length;
} LinkUdsDidDefinition;

typedef struct {
    const LinkUdsDidDefinition *definition;
    const uint8_t *data;
    size_t data_length;
} LinkUdsDidValue;

typedef struct {
    uint64_t p2_timeout_us;
    uint64_t p2_star_timeout_us;
} LinkUdsClientConfig;

typedef enum {
    LINK_UDS_CLIENT_IDLE = 0,
    LINK_UDS_CLIENT_WAITING_RESPONSE,
    LINK_UDS_CLIENT_RESPONSE_PENDING,
    LINK_UDS_CLIENT_COMPLETE,
    LINK_UDS_CLIENT_FAILED
} LinkUdsClientState;

typedef struct {
    LinkUdsClientConfig initial_config;
    uint64_t p2_timeout_us;
    uint64_t p2_star_timeout_us;
    uint64_t deadline_us;
    uint8_t request_service;
    uint8_t request_subfunction;
    uint16_t request_did;
    uint8_t active_session;
    bool request_has_subfunction;
    bool request_has_did;
    LinkUdsClientState state;
    LinkUdsResult failure;
} LinkUdsClient;

const char *link_uds_result_name(LinkUdsResult result);
const char *link_uds_client_state_name(LinkUdsClientState state);
const char *link_uds_negative_response_code_name(uint8_t code);

LinkUdsResult link_uds_decode_response(
    uint8_t request_service,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsResponse *response);
LinkUdsResult link_uds_build_session_control_request(
    uint8_t session_type,
    bool suppress_positive_response,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_decode_session_control_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint8_t expected_session_type,
    LinkUdsSessionResponse *response);
LinkUdsResult link_uds_build_tester_present_request(
    bool suppress_positive_response,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_decode_tester_present_response(
    const uint8_t *pdu,
    size_t pdu_length);
LinkUdsResult link_uds_build_read_did_request(
    uint16_t identifier,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_decode_read_did_response(
    const uint8_t *pdu,
    size_t pdu_length,
    uint16_t expected_identifier,
    LinkUdsDidRecord *record);
bool link_uds_did_definition_is_valid(const LinkUdsDidDefinition *definition);
LinkUdsResult link_uds_build_defined_did_request(
    const LinkUdsDidDefinition *definition,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_decode_defined_did_response(
    const uint8_t *pdu,
    size_t pdu_length,
    const LinkUdsDidDefinition *definition,
    LinkUdsDidValue *value);
LinkUdsResult link_uds_client_init(
    LinkUdsClient *client,
    const LinkUdsClientConfig *config);
void link_uds_client_reset(LinkUdsClient *client);
LinkUdsResult link_uds_client_begin(
    LinkUdsClient *client,
    const uint8_t *request_pdu,
    size_t request_length,
    uint64_t now_us);
LinkUdsResult link_uds_client_accept(
    LinkUdsClient *client,
    const uint8_t *response_pdu,
    size_t response_length,
    uint64_t now_us,
    LinkUdsResponse *response);
LinkUdsResult link_uds_client_tick(LinkUdsClient *client, uint64_t now_us);

#ifdef __cplusplus
}
#endif

#endif
