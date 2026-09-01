// SPDX-License-Identifier: GPL-3.0-or-later
/** @file uds_server.c @brief Portable ISO 14229 UDS ECU/server dispatcher. */
#include "link/uds_server.h"

#include <string.h>

static bool uds_server_config_valid(const LinkUdsServerConfig *config)
{
    return config != NULL && config->p2_server_max_ms != 0U &&
           config->p2_star_server_max_10ms != 0U &&
           (config->s3_server_timeout_ms == 0U || config->clock_ms != NULL);
}

static bool uds_server_nrc_valid(uint8_t nrc)
{
    return nrc != 0U;
}

LinkUdsServerHandlerResult link_uds_server_handler_positive(size_t data_length)
{
    LinkUdsServerHandlerResult result;
    result.action = LINK_UDS_SERVER_HANDLER_POSITIVE;
    result.negative_response_code = 0U;
    result.response_data_length = data_length;
    return result;
}

LinkUdsServerHandlerResult link_uds_server_handler_negative(uint8_t nrc)
{
    LinkUdsServerHandlerResult result;
    result.action = LINK_UDS_SERVER_HANDLER_NEGATIVE;
    result.negative_response_code = nrc;
    result.response_data_length = 0U;
    return result;
}

LinkUdsServerHandlerResult link_uds_server_handler_no_response(void)
{
    LinkUdsServerHandlerResult result;
    result.action = LINK_UDS_SERVER_HANDLER_NO_RESPONSE;
    result.negative_response_code = 0U;
    result.response_data_length = 0U;
    return result;
}

bool link_uds_server_init(LinkUdsServer *server, const LinkUdsServerConfig *config)
{
    if (server == NULL || !uds_server_config_valid(config)) return false;
    memset(server, 0, sizeof(*server));
    server->config = *config;
    server->active_session = LINK_UDS_SESSION_DEFAULT;
    if (config->clock_ms != NULL) {
        server->last_activity_ms = config->clock_ms(config->clock_context);
        server->activity_started = true;
    }
    return true;
}

static bool uds_server_session_transition_allowed(
    const LinkUdsServer *server,
    uint8_t requested_session)
{
    if (server == NULL || !server->config.enforce_session_sequence) {
        return true;
    }
    if (server->active_session == LINK_UDS_SESSION_DEFAULT &&
        requested_session == LINK_UDS_SESSION_PROGRAMMING) {
        return false;
    }
    if (server->active_session == LINK_UDS_SESSION_PROGRAMMING &&
        requested_session != LINK_UDS_SESSION_DEFAULT &&
        requested_session != LINK_UDS_SESSION_PROGRAMMING) {
        return false;
    }
    return true;
}

void link_uds_server_reset_session(LinkUdsServer *server)
{
    if (server == NULL) return;
    server->active_session = LINK_UDS_SESSION_DEFAULT;
    if (server->config.clock_ms != NULL) {
        server->last_activity_ms =
            server->config.clock_ms(server->config.clock_context);
        server->activity_started = true;
    }
}

void link_uds_server_tick(LinkUdsServer *server)
{
    uint32_t now;

    if (server == NULL || server->config.clock_ms == NULL ||
        server->config.s3_server_timeout_ms == 0U ||
        !server->activity_started) {
        return;
    }
    now = server->config.clock_ms(server->config.clock_context);
    if (server->active_session != LINK_UDS_SESSION_DEFAULT &&
        (uint32_t)(now - server->last_activity_ms) >=
            server->config.s3_server_timeout_ms) {
        server->active_session = LINK_UDS_SESSION_DEFAULT;
        server->last_activity_ms = now;
    }
}

bool link_uds_server_take_pending_ecu_reset(
    LinkUdsServer *server,
    uint8_t *reset_type)
{
    if (server == NULL || reset_type == NULL ||
        server->pending_ecu_reset_type == 0U) {
        return false;
    }
    *reset_type = server->pending_ecu_reset_type;
    server->pending_ecu_reset_type = 0U;
    return true;
}

static LinkUdsServerHandlerSlot *uds_server_find_handler(
    LinkUdsServer *server,
    uint8_t service)
{
    size_t index;
    if (server == NULL) return NULL;
    for (index = 0U; index < server->handler_count; ++index) {
        if (server->handlers[index].service == service) {
            return &server->handlers[index];
        }
    }
    return NULL;
}

bool link_uds_server_set_handler(
    LinkUdsServer *server,
    uint8_t service,
    LinkUdsServerHandlerFn handler,
    void *context)
{
    LinkUdsServerHandlerSlot *slot;

    if (server == NULL || link_uds_standard_service_find(service) == NULL) {
        return false;
    }
    slot = uds_server_find_handler(server, service);
    if (handler == NULL) {
        if (slot == NULL) return true;
        *slot = server->handlers[server->handler_count - 1U];
        memset(&server->handlers[server->handler_count - 1U], 0,
               sizeof(server->handlers[0]));
        server->handler_count--;
        return true;
    }
    if (slot != NULL) {
        slot->handler = handler;
        slot->context = context;
        return true;
    }
    if (server->handler_count >= LINK_UDS_SERVER_MAX_HANDLERS) return false;
    slot = &server->handlers[server->handler_count++];
    slot->service = service;
    slot->handler = handler;
    slot->context = context;
    return true;
}

static LinkUdsServerResult uds_server_write_negative(
    LinkUdsServer *server,
    uint8_t service,
    uint8_t nrc,
    uint8_t *response,
    size_t capacity,
    size_t *written)
{
    if (capacity < 3U) {
        *written = 0U;
        return LINK_UDS_SERVER_RESULT_BUFFER_TOO_SMALL;
    }
    response[0] = LINK_UDS_SERVICE_NEGATIVE_RESPONSE;
    response[1] = service;
    response[2] = nrc;
    *written = 3U;
    server->last_negative_response_code = nrc;
    server->negative_response_count++;
    return LINK_UDS_SERVER_RESULT_NEGATIVE;
}

static LinkUdsServerHandlerResult uds_server_builtin_session(
    LinkUdsServer *server,
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    uint8_t session;
    size_t required;

    if (request->pdu_length != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    session = request->subfunction;
    if (session < LINK_UDS_SESSION_DEFAULT ||
        session > LINK_UDS_SESSION_SAFETY_SYSTEM) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }

    if (!uds_server_session_transition_allowed(server, session)) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    }

    required = server->config.include_session_timing ? 5U : 1U;
    if (capacity < required) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    data[0] = session;
    if (server->config.include_session_timing) {
        data[1] = (uint8_t)(server->config.p2_server_max_ms >> 8U);
        data[2] = (uint8_t)server->config.p2_server_max_ms;
        data[3] = (uint8_t)(server->config.p2_star_server_max_10ms >> 8U);
        data[4] = (uint8_t)server->config.p2_star_server_max_10ms;
    }
    server->active_session = session;
    return link_uds_server_handler_positive(required);
}

static LinkUdsServerHandlerResult uds_server_builtin_ecu_reset(
    LinkUdsServer *server,
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    uint8_t reset_type;

    if (request->pdu_length != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    reset_type = request->subfunction;
    if (reset_type < LINK_UDS_ECU_RESET_HARD ||
        reset_type > LINK_UDS_ECU_RESET_DISABLE_RAPID_POWER_SHUTDOWN) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }

    data[0] = reset_type;
    server->pending_ecu_reset_type = reset_type;
    server->active_session = LINK_UDS_SESSION_DEFAULT;
    return link_uds_server_handler_positive(1U);
}

static LinkUdsServerHandlerResult uds_server_builtin_tester_present(
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    if (request->pdu_length != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }
    if (request->subfunction != 0U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    data[0] = 0U;
    return link_uds_server_handler_positive(1U);
}

LinkUdsServerResult link_uds_server_handle(
    LinkUdsServer *server,
    const uint8_t *request_pdu,
    size_t request_length,
    uint8_t *response_pdu,
    size_t response_capacity,
    size_t *response_length)
{
    const LinkUdsServiceDefinition *definition;
    LinkUdsServerHandlerSlot *slot;
    LinkUdsServerRequest request;
    LinkUdsServerHandlerResult handler_result;
    uint8_t service;

    if (response_length != NULL) *response_length = 0U;
    if (server == NULL || request_pdu == NULL || request_length == 0U ||
        response_pdu == NULL || response_length == NULL) {
        return LINK_UDS_SERVER_RESULT_INVALID_ARGUMENT;
    }

    link_uds_server_tick(server);
    if (server->config.clock_ms != NULL) {
        server->last_activity_ms =
            server->config.clock_ms(server->config.clock_context);
        server->activity_started = true;
    }

    service = request_pdu[0];
    server->request_count++;
    server->last_service = service;
    server->last_negative_response_code = 0U;

    definition = link_uds_standard_service_find(service);
    if (definition == NULL) {
        return uds_server_write_negative(
            server, service, LINK_UDS_NRC_SERVICE_NOT_SUPPORTED,
            response_pdu, response_capacity, response_length);
    }

    memset(&request, 0, sizeof(request));
    request.service = service;
    request.pdu = request_pdu;
    request.pdu_length = request_length;
    request.has_subfunction = definition->uses_subfunction;
    request.record = request_length > 1U ? request_pdu + 1U : NULL;
    request.record_length = request_length > 1U ? request_length - 1U : 0U;

    if (definition->uses_subfunction) {
        if (request_length < 2U) {
            return uds_server_write_negative(
                server, service,
                LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT,
                response_pdu, response_capacity, response_length);
        }
        request.suppress_positive_response =
            (request_pdu[1] & UINT8_C(0x80)) != 0U;
        request.subfunction = request_pdu[1] & UINT8_C(0x7f);
    }

    slot = uds_server_find_handler(server, service);
    if (slot != NULL) {
        handler_result = slot->handler(
            slot->context, &request,
            response_capacity > 1U ? response_pdu + 1U : response_pdu,
            response_capacity > 1U ? response_capacity - 1U : 0U);
    } else if (service == LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL) {
        handler_result = uds_server_builtin_session(
            server, &request,
            response_capacity > 1U ? response_pdu + 1U : response_pdu,
            response_capacity > 1U ? response_capacity - 1U : 0U);
    } else if (service == LINK_UDS_SERVICE_ECU_RESET) {
        handler_result = uds_server_builtin_ecu_reset(
            server, &request,
            response_capacity > 1U ? response_pdu + 1U : response_pdu,
            response_capacity > 1U ? response_capacity - 1U : 0U);
    } else if (service == LINK_UDS_SERVICE_TESTER_PRESENT) {
        handler_result = uds_server_builtin_tester_present(
            &request,
            response_capacity > 1U ? response_pdu + 1U : response_pdu,
            response_capacity > 1U ? response_capacity - 1U : 0U);
    } else {
        handler_result = link_uds_server_handler_negative(
            LINK_UDS_NRC_SERVICE_NOT_SUPPORTED);
    }

    if (handler_result.action == LINK_UDS_SERVER_HANDLER_NO_RESPONSE) {
        return LINK_UDS_SERVER_RESULT_NO_RESPONSE;
    }
    if (handler_result.action == LINK_UDS_SERVER_HANDLER_NEGATIVE) {
        const uint8_t nrc = uds_server_nrc_valid(
            handler_result.negative_response_code)
            ? handler_result.negative_response_code
            : LINK_UDS_NRC_GENERAL_REJECT;
        return uds_server_write_negative(
            server, service, nrc,
            response_pdu, response_capacity, response_length);
    }
    if (handler_result.response_data_length > response_capacity -
            (response_capacity != 0U ? 1U : 0U) ||
        response_capacity == 0U) {
        return uds_server_write_negative(
            server, service, LINK_UDS_NRC_RESPONSE_TOO_LONG,
            response_pdu, response_capacity, response_length);
    }

    if (request.suppress_positive_response) {
        server->suppressed_response_count++;
        *response_length = 0U;
        return LINK_UDS_SERVER_RESULT_SUPPRESSED;
    }

    response_pdu[0] = (uint8_t)(service + UINT8_C(0x40));
    *response_length = handler_result.response_data_length + 1U;
    server->positive_response_count++;
    return LINK_UDS_SERVER_RESULT_POSITIVE;
}

uint8_t link_uds_server_active_session(const LinkUdsServer *server)
{
    return server == NULL ? 0U : server->active_session;
}

uint8_t link_uds_server_last_negative_response_code(const LinkUdsServer *server)
{
    return server == NULL ? 0U : server->last_negative_response_code;
}

static bool uds_dtc_store_valid(const LinkUdsServerDtcStore *store)
{
    return store != NULL &&
           (store->record_count == 0U || store->records != NULL);
}

static bool uds_dtc_put_u8(
    uint8_t *data, size_t capacity, size_t *offset, uint8_t value)
{
    if (*offset >= capacity) return false;
    data[(*offset)++] = value;
    return true;
}

static bool uds_dtc_put_record(
    uint8_t *data,
    size_t capacity,
    size_t *offset,
    const LinkUdsDtcRecord *record)
{
    if (record == NULL || capacity - *offset < 4U) return false;
    data[(*offset)++] = (uint8_t)(record->code >> 16U);
    data[(*offset)++] = (uint8_t)(record->code >> 8U);
    data[(*offset)++] = (uint8_t)record->code;
    data[(*offset)++] = record->status;
    return true;
}

static size_t uds_dtc_count_status(
    const LinkUdsServerDtcStore *store, uint8_t mask)
{
    size_t index;
    size_t count = 0U;
    for (index = 0U; index < store->record_count; ++index) {
        if ((store->records[index].status & mask) != 0U) count++;
    }
    return count;
}

static const LinkUdsDtcRecord *uds_dtc_find_code(
    const LinkUdsServerDtcStore *store, uint32_t code)
{
    size_t index;
    for (index = 0U; index < store->record_count; ++index) {
        if (store->records[index].code == code) return &store->records[index];
    }
    return NULL;
}

static LinkUdsServerHandlerResult uds_dtc_too_long(void)
{
    return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
}

static LinkUdsServerHandlerResult uds_dtc_bad_length(void)
{
    return link_uds_server_handler_negative(
        LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

static LinkUdsServerHandlerResult uds_dtc_count_response(
    const LinkUdsServerDtcStore *store,
    uint8_t subfunction,
    uint16_t count,
    uint8_t *data,
    size_t capacity)
{
    if (capacity < 5U) return uds_dtc_too_long();
    data[0] = subfunction;
    data[1] = store->status_availability_mask;
    data[2] = store->dtc_format_identifier;
    data[3] = (uint8_t)(count >> 8U);
    data[4] = (uint8_t)count;
    return link_uds_server_handler_positive(5U);
}

static LinkUdsServerHandlerResult uds_dtc_status_list_response(
    const LinkUdsServerDtcStore *store,
    uint8_t subfunction,
    uint8_t mask,
    bool one_only,
    bool most_recent,
    uint8_t *data,
    size_t capacity)
{
    size_t offset = 0U;
    size_t index;
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }

    if (one_only && most_recent) {
        index = store->record_count;
        while (index != 0U) {
            --index;
            if ((store->records[index].status & mask) == 0U) continue;
            if (!uds_dtc_put_record(data, capacity, &offset,
                                    &store->records[index])) {
                return uds_dtc_too_long();
            }
            break;
        }
        return link_uds_server_handler_positive(offset);
    }

    for (index = 0U; index < store->record_count; ++index) {
        if ((store->records[index].status & mask) == 0U) continue;
        if (!uds_dtc_put_record(data, capacity, &offset,
                                &store->records[index])) {
            return uds_dtc_too_long();
        }
        if (one_only) break;
    }
    return link_uds_server_handler_positive(offset);
}

LinkUdsServerHandlerResult link_uds_server_dtc_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *data,
    size_t capacity)
{
    const LinkUdsServerDtcStore *store =
        (const LinkUdsServerDtcStore *)context;
    uint8_t subfunction;
    size_t count;

    if (!uds_dtc_store_valid(store) || request == NULL || data == NULL ||
        request->service != LINK_UDS_SERVICE_READ_DTC_INFORMATION ||
        !request->has_subfunction) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_GENERAL_REJECT);
    }
    subfunction = request->subfunction;

    switch (subfunction) {
    case LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        count = uds_dtc_count_status(store, request->pdu[2]);
        if (count > UINT16_MAX) count = UINT16_MAX;
        return uds_dtc_count_response(
            store, subfunction, (uint16_t)count, data, capacity);

    case LINK_UDS_DTC_REPORT_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, request->pdu[2], false, false,
            data, capacity);

    case LINK_UDS_DTC_REPORT_SNAPSHOT_IDENTIFICATION:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_SNAPSHOT_BY_DTC_NUMBER:
    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 6U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_STORED_DATA_BY_RECORD_NUMBER:
    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_RECORD_NUMBER:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_NUMBER_BY_SEVERITY_MASK_RECORD:
    case LINK_UDS_DTC_REPORT_BY_SEVERITY_MASK_RECORD:
        if (request->pdu_length != 4U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_SEVERITY_INFORMATION_OF_DTC:
        if (request->pdu_length != 5U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_SUPPORTED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, UINT8_C(0xff), false, false,
            data, capacity);

    case LINK_UDS_DTC_REPORT_FIRST_TEST_FAILED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, LINK_UDS_DTC_STATUS_TEST_FAILED,
            true, false, data, capacity);

    case LINK_UDS_DTC_REPORT_MOST_RECENT_TEST_FAILED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, LINK_UDS_DTC_STATUS_TEST_FAILED,
            true, true, data, capacity);

    case LINK_UDS_DTC_REPORT_FIRST_CONFIRMED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, LINK_UDS_DTC_STATUS_CONFIRMED_DTC,
            true, false, data, capacity);

    case LINK_UDS_DTC_REPORT_MOST_RECENT_CONFIRMED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_status_list_response(
            store, subfunction, LINK_UDS_DTC_STATUS_CONFIRMED_DTC,
            true, true, data, capacity);

    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 6U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_NUMBER_OF_MIRROR_MEMORY_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_NUMBER_OF_EMISSIONS_OBD_BY_STATUS_MASK:
    case LINK_UDS_DTC_REPORT_EMISSIONS_OBD_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_FAULT_DETECTION_COUNTER:
    case LINK_UDS_DTC_REPORT_WITH_PERMANENT_STATUS:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_RECORD_NUMBER:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_USER_MEMORY_BY_STATUS_MASK:
        if (request->pdu_length != 4U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_USER_MEMORY_SNAPSHOT_BY_DTC_NUMBER:
    case LINK_UDS_DTC_REPORT_USER_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 7U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_WWH_OBD_BY_MASK_RECORD:
        if (request->pdu_length != 5U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    case LINK_UDS_DTC_REPORT_WWH_OBD_WITH_PERMANENT_STATUS:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);

    default:
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
}
