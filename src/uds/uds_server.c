// SPDX-License-Identifier: GPL-3.0-or-later
/** @file uds_server.c @brief Portable ISO 14229 UDS ECU/server dispatcher. */
#include "link/uds_server.h"

#include <string.h>

static bool uds_server_config_valid(const LinkUdsServerConfig *config)
{
    return config != NULL && config->p2_server_max_ms != 0U &&
           config->p2_star_server_max_10ms != 0U &&
           (config->s3_server_timeout_ms == 0U || config->clock_ms != NULL) &&
           (config->supported_ecu_reset_types &
            (uint8_t)~LINK_UDS_ECU_RESET_SUPPORT_ALL_RESETS) == 0U;
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

bool link_uds_server_rapid_power_shutdown_enabled(
    const LinkUdsServer *server)
{
    return server != NULL && server->rapid_power_shutdown_enabled;
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
    const uint8_t reset_type = request->subfunction;
    uint8_t support_bit;

    if (request->pdu_length != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
    }

    switch (reset_type) {
    case LINK_UDS_ECU_RESET_HARD:
    case LINK_UDS_ECU_RESET_KEY_OFF_ON:
    case LINK_UDS_ECU_RESET_SOFT:
        support_bit = (uint8_t)(UINT8_C(1) << (reset_type - 1U));
        if ((server->config.supported_ecu_reset_types & support_bit) == 0U) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        if (capacity < 1U) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_RESPONSE_TOO_LONG);
        }

        /*
         * Only true ECU restart requests become pending reset actions.
         * The application executes the target-specific reset after transport
         * confirms the positive response has completed.
         */
        data[0] = reset_type;
        server->pending_ecu_reset_type = reset_type;
        server->active_session = LINK_UDS_SESSION_DEFAULT;
        return link_uds_server_handler_positive(1U);

    case LINK_UDS_ECU_RESET_ENABLE_RAPID_POWER_SHUTDOWN:
        if (!server->config.rapid_power_shutdown_supported) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        if (capacity < 2U) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_RESPONSE_TOO_LONG);
        }

        /*
         * ISO 14229 0x04 is not an ECU reset. It enables the rapid shutdown
         * mode and its positive response includes powerDownTime in seconds.
         */
        data[0] = reset_type;
        data[1] = server->config.rapid_power_shutdown_time_seconds;
        server->rapid_power_shutdown_enabled = true;
        return link_uds_server_handler_positive(2U);

    case LINK_UDS_ECU_RESET_DISABLE_RAPID_POWER_SHUTDOWN:
        if (!server->config.rapid_power_shutdown_supported) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
        }
        if (capacity < 1U) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_RESPONSE_TOO_LONG);
        }

        /* 0x05 disables the mode; it must never queue an MCU reset. */
        data[0] = reset_type;
        server->rapid_power_shutdown_enabled = false;
        return link_uds_server_handler_positive(1U);

    default:
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
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
           (store->record_count == 0U || store->records != NULL) &&
           (store->detail_count == 0U || store->details != NULL);
}

static bool uds_dtc_put_u8(
    uint8_t *data, size_t capacity, size_t *offset, uint8_t value)
{
    if (*offset >= capacity) return false;
    data[(*offset)++] = value;
    return true;
}

static bool uds_dtc_put_bytes(
    uint8_t *data, size_t capacity, size_t *offset,
    const uint8_t *bytes, size_t length)
{
    if (length == 0U) return true;
    if (bytes == NULL || capacity - *offset < length) return false;
    memcpy(data + *offset, bytes, length);
    *offset += length;
    return true;
}

static bool uds_dtc_put_code(
    uint8_t *data, size_t capacity, size_t *offset, uint32_t code)
{
    if (code > UINT32_C(0x00ffffff) || capacity - *offset < 3U) return false;
    data[(*offset)++] = (uint8_t)(code >> 16U);
    data[(*offset)++] = (uint8_t)(code >> 8U);
    data[(*offset)++] = (uint8_t)code;
    return true;
}

static bool uds_dtc_put_record(
    uint8_t *data, size_t capacity, size_t *offset,
    const LinkUdsDtcRecord *record)
{
    return record != NULL &&
           uds_dtc_put_code(data, capacity, offset, record->code) &&
           uds_dtc_put_u8(data, capacity, offset, record->status);
}

static uint32_t uds_dtc_request_code(const LinkUdsServerRequest *request)
{
    if (request == NULL || request->pdu == NULL || request->pdu_length < 5U)
        return UINT32_MAX;
    return ((uint32_t)request->pdu[2] << 16U) |
           ((uint32_t)request->pdu[3] << 8U) |
           (uint32_t)request->pdu[4];
}

static const LinkUdsDtcRecord *uds_dtc_record_for_code(
    const LinkUdsServerDtcStore *store, uint32_t code)
{
    size_t index;
    if (store == NULL) return NULL;
    for (index = 0U; index < store->record_count; ++index) {
        if (store->records[index].code == code) return &store->records[index];
    }
    return NULL;
}

static const LinkUdsServerDtcDetail *uds_dtc_detail_for_code(
    const LinkUdsServerDtcStore *store, uint32_t code)
{
    size_t index;
    if (store == NULL) return NULL;
    for (index = 0U; index < store->detail_count; ++index) {
        if (store->details[index].code == code) return &store->details[index];
    }
    return NULL;
}

static uint8_t uds_dtc_effective_status_mask(
    const LinkUdsServerDtcStore *store, uint8_t request_mask)
{
    return (uint8_t)(request_mask & store->status_availability_mask);
}

static size_t uds_dtc_count_status(
    const LinkUdsServerDtcStore *store, uint8_t mask)
{
    size_t index;
    size_t count = 0U;
    const uint8_t effective = uds_dtc_effective_status_mask(store, mask);
    if (effective == 0U) return 0U;
    for (index = 0U; index < store->record_count; ++index) {
        if ((store->records[index].status & effective) != 0U) count++;
    }
    return count;
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

static LinkUdsServerHandlerResult uds_dtc_out_of_range(void)
{
    return link_uds_server_handler_negative(LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
}

static LinkUdsServerHandlerResult uds_dtc_count_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction, uint16_t count,
    uint8_t *data, size_t capacity)
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
    const LinkUdsServerDtcStore *store, uint8_t subfunction, uint8_t mask,
    bool one_only, bool most_recent, uint8_t *data, size_t capacity)
{
    size_t offset = 0U;
    size_t index;
    const uint8_t effective = uds_dtc_effective_status_mask(store, mask);

    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }
    if (effective == 0U) return link_uds_server_handler_positive(offset);

    if (one_only && most_recent) {
        index = store->record_count;
        while (index != 0U) {
            --index;
            if ((store->records[index].status & effective) == 0U) continue;
            if (!uds_dtc_put_record(
                    data, capacity, &offset, &store->records[index])) {
                return uds_dtc_too_long();
            }
            break;
        }
        return link_uds_server_handler_positive(offset);
    }

    for (index = 0U; index < store->record_count; ++index) {
        if ((store->records[index].status & effective) == 0U) continue;
        if (!uds_dtc_put_record(
                data, capacity, &offset, &store->records[index])) {
            return uds_dtc_too_long();
        }
        if (one_only) break;
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_supported_list_response(
    const LinkUdsServerDtcStore *store,
    uint8_t subfunction,
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

    for (index = 0U; index < store->record_count; ++index) {
        if (!uds_dtc_put_record(
                data, capacity, &offset, &store->records[index])) {
            return uds_dtc_too_long();
        }
    }
    return link_uds_server_handler_positive(offset);
}

static bool uds_dtc_detail_matches(
    const LinkUdsServerDtcStore *store,
    const LinkUdsServerDtcDetail *detail,
    uint8_t status_mask,
    uint8_t severity_mask)
{
    const LinkUdsDtcRecord *record;
    const uint8_t effective_status =
        uds_dtc_effective_status_mask(store, status_mask);
    const uint8_t effective_severity =
        (uint8_t)(severity_mask & store->severity_availability_mask);

    if (detail == NULL) return false;
    record = uds_dtc_record_for_code(store, detail->code);
    if (record == NULL) return false;
    if (status_mask != 0U &&
        (effective_status == 0U ||
         (record->status & effective_status) == 0U)) {
        return false;
    }
    if (severity_mask != 0U &&
        (effective_severity == 0U ||
         (detail->severity & effective_severity) == 0U)) {
        return false;
    }
    return true;
}

static LinkUdsServerHandlerResult uds_dtc_temporal_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    bool confirmed, bool most_recent, uint8_t *data, size_t capacity)
{
    const LinkUdsServerDtcDetail *selected = NULL;
    uint32_t selected_sequence = 0U;
    size_t index;
    size_t offset = 0U;

    if (store->detail_count == 0U) {
        return uds_dtc_status_list_response(
            store, subfunction,
            confirmed ? LINK_UDS_DTC_STATUS_CONFIRMED_DTC
                      : LINK_UDS_DTC_STATUS_TEST_FAILED,
            true, most_recent, data, capacity);
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        const uint32_t sequence = confirmed
            ? detail->confirmed_sequence
            : detail->first_test_failed_sequence;

        if (record == NULL || sequence == 0U) {
            continue;
        }
        if (selected == NULL ||
            (most_recent && sequence > selected_sequence) ||
            (!most_recent && sequence < selected_sequence)) {
            selected = detail;
            selected_sequence = sequence;
        }
    }

    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }
    if (selected != NULL) {
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, selected->code);
        if (!uds_dtc_put_record(data, capacity, &offset, record))
            return uds_dtc_too_long();
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_snapshot_identification_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction))
        return uds_dtc_too_long();

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        if (detail->snapshot_record_number == 0U ||
            detail->snapshot_data == NULL ||
            detail->snapshot_data_length == 0U) {
            continue;
        }
        if (!uds_dtc_put_code(data, capacity, &offset, detail->code) ||
            !uds_dtc_put_u8(
                data, capacity, &offset, detail->snapshot_record_number)) {
            return uds_dtc_too_long();
        }
    }
    return link_uds_server_handler_positive(offset);
}

static bool uds_dtc_record_number_matches(uint8_t requested, uint8_t actual)
{
    return requested == actual || requested == UINT8_C(0xff);
}

static bool uds_dtc_ext_record_number_matches(uint8_t requested, uint8_t actual)
{
    if (requested == actual || requested == UINT8_C(0xff)) return true;
    if (requested == UINT8_C(0xfe)) {
        return actual >= UINT8_C(0x90) && actual <= UINT8_C(0xef);
    }
    return false;
}

static LinkUdsServerHandlerResult uds_dtc_snapshot_by_dtc_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction, uint32_t code,
    uint8_t requested_record, bool user_memory, uint8_t memory_selection,
    uint8_t *data, size_t capacity)
{
    const LinkUdsDtcRecord *record = uds_dtc_record_for_code(store, code);
    const LinkUdsServerDtcDetail *detail =
        uds_dtc_detail_for_code(store, code);
    size_t offset = 0U;

    if (record == NULL || detail == NULL ||
        detail->snapshot_record_number == 0U ||
        !uds_dtc_record_number_matches(
            requested_record, detail->snapshot_record_number) ||
        (user_memory &&
         detail->user_memory_selection != memory_selection)) {
        return uds_dtc_out_of_range();
    }

    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction))
        return uds_dtc_too_long();
    if (user_memory &&
        !uds_dtc_put_u8(data, capacity, &offset, memory_selection)) {
        return uds_dtc_too_long();
    }
    if (!uds_dtc_put_record(data, capacity, &offset, record))
        return uds_dtc_too_long();

    if (detail->snapshot_data == NULL ||
        detail->snapshot_data_length == 0U) {
        return link_uds_server_handler_positive(offset);
    }

    if (!uds_dtc_put_u8(
            data, capacity, &offset, detail->snapshot_record_number) ||
        !uds_dtc_put_u8(
            data, capacity, &offset, detail->snapshot_identifier_count) ||
        !uds_dtc_put_bytes(
            data, capacity, &offset,
            detail->snapshot_data, detail->snapshot_data_length)) {
        return uds_dtc_too_long();
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_stored_data_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t requested_record, uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    bool supported = false;
    bool wrote_data = false;

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction))
        return uds_dtc_too_long();

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);

        if (record == NULL || detail->stored_data_record_number == 0U ||
            !uds_dtc_record_number_matches(
                requested_record, detail->stored_data_record_number)) {
            continue;
        }
        supported = true;
        if (detail->stored_data == NULL ||
            detail->stored_data_length == 0U) {
            continue;
        }

        if (!uds_dtc_put_u8(
                data, capacity, &offset, detail->stored_data_record_number) ||
            !uds_dtc_put_record(data, capacity, &offset, record) ||
            !uds_dtc_put_u8(
                data, capacity, &offset,
                detail->stored_data_identifier_count) ||
            !uds_dtc_put_bytes(
                data, capacity, &offset,
                detail->stored_data, detail->stored_data_length)) {
            return uds_dtc_too_long();
        }
        wrote_data = true;
        if (requested_record != UINT8_C(0xff)) break;
    }

    if (!supported) return uds_dtc_out_of_range();
    if (!wrote_data) {
        if (!uds_dtc_put_u8(data, capacity, &offset, requested_record))
            return uds_dtc_too_long();
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_ext_by_dtc_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction, uint32_t code,
    uint8_t requested_record, bool require_mirror,
    bool user_memory, uint8_t memory_selection,
    uint8_t *data, size_t capacity)
{
    const LinkUdsDtcRecord *record = uds_dtc_record_for_code(store, code);
    const LinkUdsServerDtcDetail *detail =
        uds_dtc_detail_for_code(store, code);
    size_t offset = 0U;

    if (record == NULL || detail == NULL ||
        detail->ext_data_record_number == 0U ||
        !uds_dtc_ext_record_number_matches(
            requested_record, detail->ext_data_record_number) ||
        (require_mirror && !detail->mirror_memory) ||
        (user_memory && detail->user_memory_selection != memory_selection)) {
        return uds_dtc_out_of_range();
    }

    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction))
        return uds_dtc_too_long();
    if (user_memory &&
        !uds_dtc_put_u8(data, capacity, &offset, memory_selection)) {
        return uds_dtc_too_long();
    }
    if (!uds_dtc_put_record(data, capacity, &offset, record))
        return uds_dtc_too_long();

    if (detail->ext_data == NULL || detail->ext_data_length == 0U)
        return link_uds_server_handler_positive(offset);

    if (!uds_dtc_put_u8(
            data, capacity, &offset, detail->ext_data_record_number) ||
        !uds_dtc_put_bytes(
            data, capacity, &offset,
            detail->ext_data, detail->ext_data_length)) {
        return uds_dtc_too_long();
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_severity_count_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t severity_mask, uint8_t status_mask,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t count = 0U;

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    for (index = 0U; index < store->detail_count; ++index) {
        if (uds_dtc_detail_matches(
                store, &store->details[index],
                status_mask, severity_mask)) {
            ++count;
        }
    }
    if (count > UINT16_MAX) count = UINT16_MAX;
    return uds_dtc_count_response(
        store, subfunction, (uint16_t)count, data, capacity);
}

static bool uds_dtc_put_severity_record(
    uint8_t *data, size_t capacity, size_t *offset,
    const LinkUdsServerDtcDetail *detail,
    const LinkUdsDtcRecord *record)
{
    return detail != NULL && record != NULL &&
           uds_dtc_put_u8(data, capacity, offset, detail->severity) &&
           uds_dtc_put_u8(data, capacity, offset, detail->functional_unit) &&
           uds_dtc_put_record(data, capacity, offset, record);
}

static LinkUdsServerHandlerResult uds_dtc_severity_list_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t severity_mask, uint8_t status_mask,
    uint32_t exact_code, bool use_exact_code,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    bool found_exact = false;

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        if (record == NULL) continue;

        if (use_exact_code) {
            if (detail->code != exact_code) continue;
            found_exact = true;
        } else if (!uds_dtc_detail_matches(
                       store, detail, status_mask, severity_mask)) {
            continue;
        }

        if (!uds_dtc_put_severity_record(
                data, capacity, &offset, detail, record)) {
            return uds_dtc_too_long();
        }
        if (use_exact_code) break;
    }

    if (use_exact_code && !found_exact) return uds_dtc_out_of_range();
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_detail_status_list_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction, uint8_t mask,
    bool require_mirror, bool require_emissions, bool require_permanent,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    const uint8_t effective = uds_dtc_effective_status_mask(store, mask);

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        if (record == NULL) continue;
        if (require_mirror && !detail->mirror_memory) continue;
        if (require_emissions && !detail->emissions_obd) continue;
        if (require_permanent && !detail->permanent_status) continue;
        if (mask != 0U &&
            (effective == 0U || (record->status & effective) == 0U)) {
            continue;
        }
        if (!uds_dtc_put_record(data, capacity, &offset, record))
            return uds_dtc_too_long();
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_subset_count_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t status_mask, bool mirror, bool emissions,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t count = 0U;
    const uint8_t effective =
        uds_dtc_effective_status_mask(store, status_mask);

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        if (record == NULL) continue;
        if (mirror && !detail->mirror_memory) continue;
        if (emissions && !detail->emissions_obd) continue;
        if (effective == 0U || (record->status & effective) == 0U) continue;
        ++count;
    }
    if (count > UINT16_MAX) count = UINT16_MAX;
    return uds_dtc_count_response(
        store, subfunction, (uint16_t)count, data, capacity);
}

static LinkUdsServerHandlerResult uds_dtc_fault_counter_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction))
        return uds_dtc_too_long();

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        if (uds_dtc_record_for_code(store, detail->code) == NULL ||
            detail->fault_detection_counter == 0U ||
            detail->fault_detection_counter >= UINT8_C(0x7f)) {
            continue;
        }
        if (!uds_dtc_put_code(data, capacity, &offset, detail->code) ||
            !uds_dtc_put_u8(
                data, capacity, &offset,
                detail->fault_detection_counter)) {
            return uds_dtc_too_long();
        }
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult uds_dtc_ext_by_record_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t record_number, uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    bool supported = false;

    if (store->detail_count == 0U || record_number > UINT8_C(0xef))
        return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset, record_number)) {
        return uds_dtc_too_long();
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);

        if (record == NULL ||
            detail->ext_data_record_number != record_number) {
            continue;
        }
        supported = true;
        if (detail->ext_data == NULL || detail->ext_data_length == 0U)
            continue;
        if (!uds_dtc_put_record(data, capacity, &offset, record) ||
            !uds_dtc_put_bytes(
                data, capacity, &offset,
                detail->ext_data, detail->ext_data_length)) {
            return uds_dtc_too_long();
        }
    }
    return supported ? link_uds_server_handler_positive(offset)
                     : uds_dtc_out_of_range();
}

static LinkUdsServerHandlerResult uds_dtc_user_memory_list_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t status_mask, uint8_t memory_selection,
    uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    bool memory_known = false;
    const uint8_t effective =
        uds_dtc_effective_status_mask(store, status_mask);

    if (store->detail_count == 0U) return uds_dtc_out_of_range();
    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset, memory_selection) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        if (detail->user_memory_selection != memory_selection) continue;
        memory_known = true;
        if (record == NULL || effective == 0U ||
            (record->status & effective) == 0U) {
            continue;
        }
        if (!uds_dtc_put_record(data, capacity, &offset, record))
            return uds_dtc_too_long();
    }
    return memory_known ? link_uds_server_handler_positive(offset)
                        : uds_dtc_out_of_range();
}

static LinkUdsServerHandlerResult uds_dtc_wwh_response(
    const LinkUdsServerDtcStore *store, uint8_t subfunction,
    uint8_t functional_group, uint8_t status_mask, uint8_t severity_mask,
    bool permanent_only, uint8_t *data, size_t capacity)
{
    size_t index;
    size_t offset = 0U;
    bool group_known = false;
    const uint8_t effective =
        uds_dtc_effective_status_mask(store, status_mask);
    const uint8_t effective_severity =
        (uint8_t)(severity_mask & store->severity_availability_mask);

    if (store->detail_count == 0U ||
        (store->wwh_dtc_format_identifier != UINT8_C(0x04) &&
         store->wwh_dtc_format_identifier != UINT8_C(0x02))) {
        return uds_dtc_out_of_range();
    }

    if (!uds_dtc_put_u8(data, capacity, &offset, subfunction) ||
        !uds_dtc_put_u8(data, capacity, &offset, functional_group) ||
        !uds_dtc_put_u8(data, capacity, &offset,
                        store->status_availability_mask)) {
        return uds_dtc_too_long();
    }
    if (permanent_only) {
        if (!uds_dtc_put_u8(
                data, capacity, &offset,
                store->wwh_dtc_format_identifier)) {
            return uds_dtc_too_long();
        }
    } else {
        if (!uds_dtc_put_u8(
                data, capacity, &offset,
                store->severity_availability_mask) ||
            !uds_dtc_put_u8(
                data, capacity, &offset,
                store->wwh_dtc_format_identifier)) {
            return uds_dtc_too_long();
        }
    }

    for (index = 0U; index < store->detail_count; ++index) {
        const LinkUdsServerDtcDetail *detail = &store->details[index];
        const LinkUdsDtcRecord *record =
            uds_dtc_record_for_code(store, detail->code);
        if (detail->functional_group_identifier != functional_group) continue;
        group_known = true;
        if (record == NULL || !detail->emissions_obd) continue;

        if (permanent_only) {
            if (!detail->permanent_status) continue;
            if (!uds_dtc_put_record(data, capacity, &offset, record))
                return uds_dtc_too_long();
        } else {
            if (effective == 0U || effective_severity == 0U ||
                (record->status & effective) == 0U ||
                (detail->severity & effective_severity) == 0U) {
                continue;
            }
            if (!uds_dtc_put_u8(data, capacity, &offset, detail->severity) ||
                !uds_dtc_put_code(data, capacity, &offset, detail->code) ||
                !uds_dtc_put_u8(data, capacity, &offset, record->status)) {
                return uds_dtc_too_long();
            }
        }
    }
    return group_known ? link_uds_server_handler_positive(offset)
                       : uds_dtc_out_of_range();
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
        return uds_dtc_snapshot_identification_response(
            store, subfunction, data, capacity);

    case LINK_UDS_DTC_REPORT_SNAPSHOT_BY_DTC_NUMBER:
        if (request->pdu_length != 6U) return uds_dtc_bad_length();
        return uds_dtc_snapshot_by_dtc_response(
            store, subfunction, uds_dtc_request_code(request),
            request->pdu[5], false, 0U, data, capacity);

    case LINK_UDS_DTC_REPORT_STORED_DATA_BY_RECORD_NUMBER:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_stored_data_response(
            store, subfunction, request->pdu[2], data, capacity);

    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 6U) return uds_dtc_bad_length();
        return uds_dtc_ext_by_dtc_response(
            store, subfunction, uds_dtc_request_code(request),
            request->pdu[5], false, false, 0U, data, capacity);

    case LINK_UDS_DTC_REPORT_NUMBER_BY_SEVERITY_MASK_RECORD:
        if (request->pdu_length != 4U) return uds_dtc_bad_length();
        return uds_dtc_severity_count_response(
            store, subfunction, request->pdu[2], request->pdu[3],
            data, capacity);

    case LINK_UDS_DTC_REPORT_BY_SEVERITY_MASK_RECORD:
        if (request->pdu_length != 4U) return uds_dtc_bad_length();
        return uds_dtc_severity_list_response(
            store, subfunction, request->pdu[2], request->pdu[3],
            0U, false, data, capacity);

    case LINK_UDS_DTC_REPORT_SEVERITY_INFORMATION_OF_DTC:
        if (request->pdu_length != 5U) return uds_dtc_bad_length();
        return uds_dtc_severity_list_response(
            store, subfunction, 0U, 0U,
            uds_dtc_request_code(request), true, data, capacity);

    case LINK_UDS_DTC_REPORT_SUPPORTED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_supported_list_response(
            store, subfunction, data, capacity);

    case LINK_UDS_DTC_REPORT_FIRST_TEST_FAILED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_temporal_response(
            store, subfunction, false, false, data, capacity);

    case LINK_UDS_DTC_REPORT_FIRST_CONFIRMED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_temporal_response(
            store, subfunction, true, false, data, capacity);

    case LINK_UDS_DTC_REPORT_MOST_RECENT_TEST_FAILED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_temporal_response(
            store, subfunction, false, true, data, capacity);

    case LINK_UDS_DTC_REPORT_MOST_RECENT_CONFIRMED_DTC:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_temporal_response(
            store, subfunction, true, true, data, capacity);

    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_detail_status_list_response(
            store, subfunction, request->pdu[2],
            true, false, false, data, capacity);

    case LINK_UDS_DTC_REPORT_MIRROR_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 6U) return uds_dtc_bad_length();
        return uds_dtc_ext_by_dtc_response(
            store, subfunction, uds_dtc_request_code(request),
            request->pdu[5], true, false, 0U, data, capacity);

    case LINK_UDS_DTC_REPORT_NUMBER_OF_MIRROR_MEMORY_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_subset_count_response(
            store, subfunction, request->pdu[2],
            true, false, data, capacity);

    case LINK_UDS_DTC_REPORT_NUMBER_OF_EMISSIONS_OBD_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_subset_count_response(
            store, subfunction, request->pdu[2],
            false, true, data, capacity);

    case LINK_UDS_DTC_REPORT_EMISSIONS_OBD_BY_STATUS_MASK:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_detail_status_list_response(
            store, subfunction, request->pdu[2],
            false, true, false, data, capacity);

    case LINK_UDS_DTC_REPORT_FAULT_DETECTION_COUNTER:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_fault_counter_response(
            store, subfunction, data, capacity);

    case LINK_UDS_DTC_REPORT_WITH_PERMANENT_STATUS:
        if (request->pdu_length != 2U) return uds_dtc_bad_length();
        return uds_dtc_detail_status_list_response(
            store, subfunction, 0U,
            false, false, true, data, capacity);

    case LINK_UDS_DTC_REPORT_EXT_DATA_BY_RECORD_NUMBER:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_ext_by_record_response(
            store, subfunction, request->pdu[2], data, capacity);

    case LINK_UDS_DTC_REPORT_USER_MEMORY_BY_STATUS_MASK:
        if (request->pdu_length != 4U) return uds_dtc_bad_length();
        return uds_dtc_user_memory_list_response(
            store, subfunction, request->pdu[2], request->pdu[3],
            data, capacity);

    case LINK_UDS_DTC_REPORT_USER_MEMORY_SNAPSHOT_BY_DTC_NUMBER:
        if (request->pdu_length != 7U) return uds_dtc_bad_length();
        return uds_dtc_snapshot_by_dtc_response(
            store, subfunction, uds_dtc_request_code(request),
            request->pdu[5], true, request->pdu[6], data, capacity);

    case LINK_UDS_DTC_REPORT_USER_MEMORY_EXT_DATA_BY_DTC_NUMBER:
        if (request->pdu_length != 7U) return uds_dtc_bad_length();
        return uds_dtc_ext_by_dtc_response(
            store, subfunction, uds_dtc_request_code(request),
            request->pdu[5], false, true, request->pdu[6],
            data, capacity);

    case LINK_UDS_DTC_REPORT_WWH_OBD_BY_MASK_RECORD:
        if (request->pdu_length != 5U) return uds_dtc_bad_length();
        return uds_dtc_wwh_response(
            store, subfunction, request->pdu[2],
            request->pdu[3], request->pdu[4], false,
            data, capacity);

    case LINK_UDS_DTC_REPORT_WWH_OBD_WITH_PERMANENT_STATUS:
        if (request->pdu_length != 3U) return uds_dtc_bad_length();
        return uds_dtc_wwh_response(
            store, subfunction, request->pdu[2],
            UINT8_C(0xff), UINT8_C(0xff), true,
            data, capacity);

    default:
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
}
