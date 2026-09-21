// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_server.h
 * @brief Allocation-free ISO 14229 UDS ECU/server dispatcher.
 *
 * LINK's client codecs and this server dispatcher share the same 27-service
 * catalogue. Application-specific ECU behaviour is supplied through bounded
 * service handlers; DiagnosticSessionControl, ECUReset and TesterPresent have
 * portable built-in handlers. ReadDTCInformation has codecs for all 27 LINK
 * report types. The DTC store handler supports a compact status-only mode and
 * an optional rich metadata model for ISO 14229-1:2013 snapshot, stored-data,
 * extended-data, severity, mirror/emissions/permanent/user-memory, FDC and
 * WWH-OBD responses.
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
#define LINK_UDS_NRC_INVALID_KEY 0x35U
#define LINK_UDS_NRC_EXCEED_NUMBER_OF_ATTEMPTS 0x36U
#define LINK_UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED 0x37U
#define LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE 0x72U
#define LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7eU
#define LINK_UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION 0x7fU

#define LINK_UDS_SERVER_MAX_HANDLERS LINK_UDS_STANDARD_SERVICE_COUNT

#define LINK_UDS_SESSION_MASK_DEFAULT UINT8_C(0x01)
#define LINK_UDS_SESSION_MASK_PROGRAMMING UINT8_C(0x02)
#define LINK_UDS_SESSION_MASK_EXTENDED UINT8_C(0x04)
#define LINK_UDS_SESSION_MASK_SAFETY_SYSTEM UINT8_C(0x08)
#define LINK_UDS_SESSION_MASK_ALL UINT8_C(0x0f)

#define LINK_UDS_SECURITY_LEVEL_MAX 63U
#define LINK_UDS_SECURITY_LEVEL_MASK(level) (UINT64_C(1) << (level))
#define LINK_UDS_SECURITY_LEVEL_MASK_UNSECURED UINT64_C(1)
#define LINK_UDS_SECURITY_LEVEL_MASK_ALL UINT64_MAX

#define LINK_UDS_ADDRESSING_MASK_PHYSICAL UINT8_C(0x01)
#define LINK_UDS_ADDRESSING_MASK_FUNCTIONAL UINT8_C(0x02)
#define LINK_UDS_ADDRESSING_MASK_BOTH UINT8_C(0x03)

typedef enum {
    LINK_UDS_SERVER_ADDRESSING_PHYSICAL = 0,
    LINK_UDS_SERVER_ADDRESSING_FUNCTIONAL
} LinkUdsServerAddressing;

typedef struct {
    uint8_t service;
    bool subfunction_specific;
    uint8_t subfunction;
    uint8_t session_mask;
    uint64_t security_level_mask;
    uint8_t addressing_mask;
} LinkUdsServerPolicy;

typedef struct {
    LinkUdsServerAddressing addressing;
} LinkUdsServerRequestContext;

#define LINK_UDS_SERVER_REQUEST_CONTEXT_INIT \
    { LINK_UDS_SERVER_ADDRESSING_PHYSICAL }

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
    LinkUdsServerAddressing addressing;
    uint8_t security_level;
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

typedef LinkUdsServerHandlerResult (*LinkUdsSecurityAccessSeedFn)(
    void *context, uint8_t security_level,
    const uint8_t *request_record, size_t request_record_length,
    uint8_t *seed, size_t seed_capacity);

typedef bool (*LinkUdsSecurityAccessVerifyKeyFn)(
    void *context, uint8_t security_level,
    const uint8_t *key, size_t key_length);

typedef struct {
    LinkUdsSecurityAccessSeedFn seed;
    LinkUdsSecurityAccessVerifyKeyFn verify_key;
    void *context;
    uint8_t max_invalid_key_attempts;
    uint32_t delay_ms;
} LinkUdsSecurityAccessConfig;

#define LINK_UDS_SECURITY_ACCESS_CONFIG_INIT { NULL, NULL, NULL, 0U, 0U }

typedef uint32_t (*LinkUdsServerClockMsFn)(void *context);

typedef struct {
    uint16_t p2_server_max_ms;
    uint16_t p2_star_server_max_10ms;
    bool include_session_timing;
    bool enforce_session_sequence;
    bool reset_security_on_session_change;
    uint32_t s3_server_timeout_ms;
    LinkUdsServerClockMsFn clock_ms;
    void *clock_context;

    /*
     * Product/target ECUReset capabilities. 0x01..0x03 are advertised only
     * when the corresponding bit is set. 0x04/0x05 are controlled separately
     * because they change rapid-power-shutdown state rather than resetting the
     * processor. power_down_time_seconds is returned only for 0x04.
     */
    uint8_t supported_ecu_reset_types;
    bool rapid_power_shutdown_supported;
    uint8_t rapid_power_shutdown_time_seconds;

    /*
     * Optional product-neutral execution policy. Exact subfunction entries
     * override service-wide entries. No table preserves the historical
     * unrestricted dispatcher behaviour.
     */
    const LinkUdsServerPolicy *policies;
    size_t policy_count;

    /*
     * Optional built-in SecurityAccess (0x27) sequencing. LINK owns only the
     * UDS state machine, attempts/delay policy and level state. The target
     * supplies seed generation and key verification.
     */
    LinkUdsSecurityAccessConfig security_access;
} LinkUdsServerConfig;

#define LINK_UDS_SERVER_CONFIG_INIT \
    { UINT16_C(50), UINT16_C(500), true, false, true, 0U, NULL, NULL, \
      LINK_UDS_ECU_RESET_SUPPORT_ALL_RESETS, false, 0U, NULL, 0U, \
      LINK_UDS_SECURITY_ACCESS_CONFIG_INIT }

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
    uint8_t active_security_level;
    uint8_t pending_security_level;
    uint8_t invalid_security_key_attempts;
    bool security_seed_pending;
    bool security_delay_active;
    uint32_t security_delay_started_ms;
    uint8_t last_service;
    uint8_t last_negative_response_code;
    uint8_t pending_ecu_reset_type;
    bool rapid_power_shutdown_enabled;
    uint32_t last_activity_ms;
    bool activity_started;
    uint32_t request_count;
    uint32_t positive_response_count;
    uint32_t negative_response_count;
    uint32_t suppressed_response_count;
} LinkUdsServer;

typedef struct {
    uint32_t code;
    uint8_t severity;
    uint8_t functional_unit;
    uint8_t fault_detection_counter;
    uint32_t first_test_failed_sequence;
    uint32_t confirmed_sequence;
    bool mirror_memory;
    bool emissions_obd;
    bool permanent_status;
    uint8_t functional_group_identifier;
    uint8_t user_memory_selection;
    uint8_t snapshot_record_number;
    uint8_t snapshot_identifier_count;
    const uint8_t *snapshot_data;
    size_t snapshot_data_length;
    uint8_t stored_data_record_number;
    uint8_t stored_data_identifier_count;
    const uint8_t *stored_data;
    size_t stored_data_length;
    uint8_t ext_data_record_number;
    const uint8_t *ext_data;
    size_t ext_data_length;
} LinkUdsServerDtcDetail;

#define LINK_UDS_SERVER_DTC_DETAIL_INIT \
    { 0U, 0U, 0U, 0U, 0U, 0U, false, false, false, 0U, 0U, \
      0U, 0U, NULL, 0U, 0U, 0U, NULL, 0U, 0U, NULL, 0U }

typedef struct {
    const LinkUdsDtcRecord *records;
    size_t record_count;
    uint8_t status_availability_mask;
    uint8_t severity_availability_mask;
    uint8_t dtc_format_identifier;
    const LinkUdsServerDtcDetail *details;
    size_t detail_count;
    uint8_t wwh_dtc_format_identifier;
} LinkUdsServerDtcStore;

#define LINK_UDS_SERVER_DTC_STORE_INIT \
    { NULL, 0U, LINK_UDS_DTC_STATUS_MASK_ALL, 0U, 0x01U, NULL, 0U, 0x04U }

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
LinkUdsServerResult link_uds_server_handle_with_context(
    LinkUdsServer *server,
    const LinkUdsServerRequestContext *context,
    const uint8_t *request_pdu,
    size_t request_length,
    uint8_t *response_pdu,
    size_t response_capacity,
    size_t *response_length);
const LinkUdsServerPolicy *link_uds_server_policy_find(
    const LinkUdsServer *server,
    uint8_t service,
    bool has_subfunction,
    uint8_t subfunction);
bool link_uds_server_set_security_level(
    LinkUdsServer *server,
    uint8_t security_level);
uint8_t link_uds_server_active_security_level(const LinkUdsServer *server);
uint8_t link_uds_server_active_session(const LinkUdsServer *server);
uint8_t link_uds_server_last_negative_response_code(const LinkUdsServer *server);
void link_uds_server_tick(LinkUdsServer *server);
void link_uds_server_reset_session(LinkUdsServer *server);
bool link_uds_server_take_pending_ecu_reset(
    LinkUdsServer *server,
    uint8_t *reset_type);
bool link_uds_server_rapid_power_shutdown_enabled(
    const LinkUdsServer *server);

LinkUdsServerHandlerResult link_uds_server_dtc_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *response_data,
    size_t response_data_capacity);

#ifdef __cplusplus
}
#endif

#endif
