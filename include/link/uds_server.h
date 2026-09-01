// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_server.h
 * @brief Allocation-free ISO 14229 UDS ECU/server dispatcher.
 *
 * LINK's client codecs and this server dispatcher share the same 27-service
 * catalogue. Application-specific ECU behaviour is supplied through bounded
 * service handlers; DiagnosticSessionControl and TesterPresent have portable
 * built-in handlers, and ReadDTCInformation can use the supplied bounded DTC
 * store handler covering every LINK 0x19 report type.
 */
#ifndef LINK_UDS_SERVER_H
#define LINK_UDS_SERVER_H

#include "link/uds_dtc.h"
#include "link/uds_services.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_NRC_GENERAL_REJECT 0x10U
#define LINK_UDS_NRC_SERVICE_NOT_SUPPORTED 0x11U
#define LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED 0x12U
#define LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT 0x13U
#define LINK_UDS_NRC_RESPONSE_TOO_LONG 0x14U
#define LINK_UDS_NRC_BUSY_REPEAT_REQUEST 0x21U
#define LINK_UDS_NRC_CONDITIONS_NOT_CORRECT 0x22U
#define LINK_UDS_NRC_REQUEST_SEQUENCE_ERROR 0x24U
#define LINK_UDS_NRC_REQUEST_OUT_OF_RANGE 0x31U
#define LINK_UDS_NRC_SECURITY_ACCESS_DENIED 0x33U
#define LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE 0x72U
#define LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7eU
#define LINK_UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7fU

#define LINK_UDS_SERVER_MAX_HANDLERS LINK_UDS_STANDARD_SERVICE_COUNT

typedef enum {
    LINK_UDS_SERVER_HANDLER_POSITIVE = 0,
    LINK_UDS_SERVER_HANDLER_NEGATIVE,
    LINK_UDS_SERVER_HANDLER_NO_RESPONSE
} LinkUdsServerHandlerAction;

typedef struct {
    uint8_t service;
    bool has_subfunction;
    uint8_t subfunction;
    bool suppress_positive_response;
    const uint8_t *pdu;
    size_t pdu_length;
    const uint8_t *record;
    size_t record_length;
} LinkUdsServerRequest;

typedef struct {
    LinkUdsServerHandlerAction action;
    uint8_t negative_response_code;
    size_t response_data_length;
} LinkUdsServerHandlerResult;

typedef LinkUdsServerHandlerResult (*LinkUdsServerHandlerFn)(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *response_data,
    size_t response_data_capacity);

typedef struct {
    uint8_t service;
    LinkUdsServerHandlerFn handler;
    void *context;
} LinkUdsServerHandlerSlot;

typedef struct {
    uint16_t p2_server_max_ms;
    uint16_t p2_star_server_max_10ms;
    bool include_session_timing;
} LinkUdsServerConfig;

#define LINK_UDS_SERVER_CONFIG_INIT \
    { UINT16_C(50), UINT16_C(500), true }

typedef enum {
    LINK_UDS_SERVER_RESULT_POSITIVE = 0,
    LINK_UDS_SERVER_RESULT_NEGATIVE,
    LINK_UDS_SERVER_RESULT_SUPPRESSED,
    LINK_UDS_SERVER_RESULT_NO_RESPONSE,
    LINK_UDS_SERVER_RESULT_INVALID_ARGUMENT,
    LINK_UDS_SERVER_RESULT_BUFFER_TOO_SMALL
} LinkUdsServerResult;

typedef struct {
    LinkUdsServerConfig config;
    LinkUdsServerHandlerSlot handlers[LINK_UDS_SERVER_MAX_HANDLERS];
    size_t handler_count;
    uint8_t active_session;
    uint8_t last_service;
    uint8_t last_negative_response_code;
    uint32_t request_count;
    uint32_t positive_response_count;
    uint32_t negative_response_count;
    uint32_t suppressed_response_count;
} LinkUdsServer;

typedef struct {
    const LinkUdsDtcRecord *records;
    size_t record_count;
    uint8_t status_availability_mask;
    uint8_t severity_availability_mask;
    uint8_t dtc_format_identifier;
} LinkUdsServerDtcStore;

#define LINK_UDS_SERVER_DTC_STORE_INIT \
    { NULL, 0U, LINK_UDS_DTC_STATUS_MASK_ALL, 0U, 0x01U }

LinkUdsServerHandlerResult link_uds_server_handler_positive(size_t data_length);
LinkUdsServerHandlerResult link_uds_server_handler_negative(uint8_t nrc);
LinkUdsServerHandlerResult link_uds_server_handler_no_response(void);

bool link_uds_server_init(LinkUdsServer *server, const LinkUdsServerConfig *config);
bool link_uds_server_set_handler(
    LinkUdsServer *server,
    uint8_t service,
    LinkUdsServerHandlerFn handler,
    void *context);
LinkUdsServerResult link_uds_server_handle(
    LinkUdsServer *server,
    const uint8_t *request_pdu,
    size_t request_length,
    uint8_t *response_pdu,
    size_t response_capacity,
    size_t *response_length);
uint8_t link_uds_server_active_session(const LinkUdsServer *server);
uint8_t link_uds_server_last_negative_response_code(const LinkUdsServer *server);

LinkUdsServerHandlerResult link_uds_server_dtc_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *response_data,
    size_t response_data_capacity);

#ifdef __cplusplus
}
#endif

#endif
