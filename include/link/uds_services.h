// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_services.h
 * @brief Complete ISO 14229 UDS service catalogue and portable request codecs.
 *
 * This layer serializes and validates service PDUs only. It does not transmit
 * them and it does not weaken LINK Discover's deny-by-default safety policy.
 * Complex manufacturer- or application-defined records are accepted as raw
 * caller-owned byte spans so LINK does not invent vehicle-specific semantics.
 */
#ifndef LINK_UDS_SERVICES_H
#define LINK_UDS_SERVICES_H

#include "link/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_STANDARD_SERVICE_COUNT 27U

#ifndef LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL
#define LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL 0x10U
#endif
#define LINK_UDS_SERVICE_ECU_RESET 0x11U
#define LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION 0x14U
#ifndef LINK_UDS_SERVICE_READ_DTC_INFORMATION
#define LINK_UDS_SERVICE_READ_DTC_INFORMATION 0x19U
#endif
#ifndef LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER
#define LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER 0x22U
#endif
#define LINK_UDS_SERVICE_READ_MEMORY_BY_ADDRESS 0x23U
#define LINK_UDS_SERVICE_READ_SCALING_DATA_BY_IDENTIFIER 0x24U
#define LINK_UDS_SERVICE_SECURITY_ACCESS 0x27U
#define LINK_UDS_SERVICE_COMMUNICATION_CONTROL 0x28U
#define LINK_UDS_SERVICE_AUTHENTICATION 0x29U
#define LINK_UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER 0x2AU
#define LINK_UDS_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER 0x2CU
#define LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER 0x2EU
#define LINK_UDS_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER 0x2FU
#define LINK_UDS_SERVICE_ROUTINE_CONTROL 0x31U
#define LINK_UDS_SERVICE_REQUEST_DOWNLOAD 0x34U
#define LINK_UDS_SERVICE_REQUEST_UPLOAD 0x35U
#define LINK_UDS_SERVICE_TRANSFER_DATA 0x36U
#define LINK_UDS_SERVICE_REQUEST_TRANSFER_EXIT 0x37U
#define LINK_UDS_SERVICE_REQUEST_FILE_TRANSFER 0x38U
#define LINK_UDS_SERVICE_WRITE_MEMORY_BY_ADDRESS 0x3DU
#ifndef LINK_UDS_SERVICE_TESTER_PRESENT
#define LINK_UDS_SERVICE_TESTER_PRESENT 0x3EU
#endif
#define LINK_UDS_SERVICE_ACCESS_TIMING_PARAMETER 0x83U
#define LINK_UDS_SERVICE_SECURED_DATA_TRANSMISSION 0x84U
#define LINK_UDS_SERVICE_CONTROL_DTC_SETTING 0x85U
#define LINK_UDS_SERVICE_RESPONSE_ON_EVENT 0x86U
#define LINK_UDS_SERVICE_LINK_CONTROL 0x87U

#define LINK_UDS_ACCESS_TIMING_READ_EXTENDED_SET 0x01U
#define LINK_UDS_ACCESS_TIMING_SET_DEFAULT 0x02U
#define LINK_UDS_ACCESS_TIMING_READ_ACTIVE 0x03U
#define LINK_UDS_ACCESS_TIMING_SET_GIVEN 0x04U

#define LINK_UDS_DTC_SETTING_ON 0x01U
#define LINK_UDS_DTC_SETTING_OFF 0x02U

#define LINK_UDS_ROUTINE_START 0x01U
#define LINK_UDS_ROUTINE_STOP 0x02U
#define LINK_UDS_ROUTINE_REQUEST_RESULTS 0x03U

#define LINK_UDS_DYNAMIC_DID_DEFINE_BY_IDENTIFIER 0x01U
#define LINK_UDS_DYNAMIC_DID_DEFINE_BY_MEMORY_ADDRESS 0x02U
#define LINK_UDS_DYNAMIC_DID_CLEAR 0x03U

#define LINK_UDS_LINK_VERIFY_FIXED_BAUDRATE 0x01U
#define LINK_UDS_LINK_VERIFY_SPECIFIC_BAUDRATE 0x02U
#define LINK_UDS_LINK_TRANSITION_BAUDRATE 0x03U

#define LINK_UDS_PERIODIC_SEND_SLOW 0x01U
#define LINK_UDS_PERIODIC_SEND_MEDIUM 0x02U
#define LINK_UDS_PERIODIC_SEND_FAST 0x03U
#define LINK_UDS_PERIODIC_STOP 0x04U

typedef enum {
    LINK_UDS_SERVICE_EFFECT_READ_ONLY = 0,
    LINK_UDS_SERVICE_EFFECT_SESSION_CONTROL,
    LINK_UDS_SERVICE_EFFECT_STATE_CHANGING,
    LINK_UDS_SERVICE_EFFECT_SECURITY,
    LINK_UDS_SERVICE_EFFECT_PROGRAMMING
} LinkUdsServiceEffect;

typedef struct {
    uint8_t service;
    const char *name;
    bool uses_subfunction;
    LinkUdsServiceEffect effect;
} LinkUdsServiceDefinition;

typedef struct {
    const uint8_t *record;
    size_t record_length;
} LinkUdsRecordResponse;

static const LinkUdsServiceDefinition link_uds_services[] = {
    { LINK_UDS_SERVICE_ACCESS_TIMING_PARAMETER, "AccessTimingParameter", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION, "ClearDiagnosticInformation", false, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_COMMUNICATION_CONTROL, "CommunicationControl", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_CONTROL_DTC_SETTING, "ControlDTCSetting", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL, "DiagnosticSessionControl", true, LINK_UDS_SERVICE_EFFECT_SESSION_CONTROL },
    { LINK_UDS_SERVICE_ECU_RESET, "ECUReset", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER, "InputOutputControlByIdentifier", false, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_LINK_CONTROL, "LinkControl", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER, "ReadDataByIdentifier", false, LINK_UDS_SERVICE_EFFECT_READ_ONLY },
    { LINK_UDS_SERVICE_REQUEST_FILE_TRANSFER, "RequestFileTransfer", false, LINK_UDS_SERVICE_EFFECT_PROGRAMMING },
    { LINK_UDS_SERVICE_READ_DTC_INFORMATION, "ReadDTCInformation", true, LINK_UDS_SERVICE_EFFECT_READ_ONLY },
    { LINK_UDS_SERVICE_READ_MEMORY_BY_ADDRESS, "ReadMemoryByAddress", false, LINK_UDS_SERVICE_EFFECT_READ_ONLY },
    { LINK_UDS_SERVICE_REQUEST_DOWNLOAD, "RequestDownload", false, LINK_UDS_SERVICE_EFFECT_PROGRAMMING },
    { LINK_UDS_SERVICE_REQUEST_TRANSFER_EXIT, "RequestTransferExit", false, LINK_UDS_SERVICE_EFFECT_PROGRAMMING },
    { LINK_UDS_SERVICE_REQUEST_UPLOAD, "RequestUpload", false, LINK_UDS_SERVICE_EFFECT_PROGRAMMING },
    { LINK_UDS_SERVICE_ROUTINE_CONTROL, "RoutineControl", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_SECURITY_ACCESS, "SecurityAccess", true, LINK_UDS_SERVICE_EFFECT_SECURITY },
    { LINK_UDS_SERVICE_TESTER_PRESENT, "TesterPresent", true, LINK_UDS_SERVICE_EFFECT_SESSION_CONTROL },
    { LINK_UDS_SERVICE_TRANSFER_DATA, "TransferData", false, LINK_UDS_SERVICE_EFFECT_PROGRAMMING },
    { LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER, "WriteDataByIdentifier", false, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_WRITE_MEMORY_BY_ADDRESS, "WriteMemoryByAddress", false, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER, "DynamicallyDefineDataIdentifier", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_SECURED_DATA_TRANSMISSION, "SecuredDataTransmission", false, LINK_UDS_SERVICE_EFFECT_SECURITY },
    { LINK_UDS_SERVICE_RESPONSE_ON_EVENT, "ResponseOnEvent", true, LINK_UDS_SERVICE_EFFECT_STATE_CHANGING },
    { LINK_UDS_SERVICE_READ_SCALING_DATA_BY_IDENTIFIER, "ReadScalingDataByIdentifier", false, LINK_UDS_SERVICE_EFFECT_READ_ONLY },
    { LINK_UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER, "ReadDataByPeriodicIdentifier", false, LINK_UDS_SERVICE_EFFECT_READ_ONLY },
    { LINK_UDS_SERVICE_AUTHENTICATION, "Authentication", true, LINK_UDS_SERVICE_EFFECT_SECURITY }
};

static LinkUdsResult uds_services_write_failure(
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written,
    LinkUdsResult result)
{
    if (written != NULL) {
        *written = 0U;
    }
    if (buffer != NULL && buffer_size != 0U) {
        buffer[0] = 0U;
    }
    return result;
}

static bool uds_services_record_valid(const uint8_t *record, size_t record_length)
{
    return record_length == 0U || record != NULL;
}

static bool uds_services_u64_fits_width(uint64_t value, uint8_t width)
{
    if (width == 0U || width > 8U) {
        return false;
    }
    if (width == 8U) {
        return true;
    }
    return value < (UINT64_C(1) << ((unsigned int)width * 8U));
}

static void uds_services_write_u64_be(
    uint8_t *buffer,
    uint64_t value,
    uint8_t width)
{
    uint8_t index;

    for (index = 0U; index < width; ++index) {
        const unsigned int shift =
            (unsigned int)(width - 1U - index) * 8U;
        buffer[index] = (uint8_t)(value >> shift);
    }
}

static LinkUdsResult uds_services_build_prefix_record(
    const uint8_t *prefix,
    size_t prefix_length,
    const uint8_t *record,
    size_t record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    size_t total;

    if (prefix == NULL || prefix_length == 0U ||
        !uds_services_record_valid(record, record_length) ||
        buffer == NULL || written == NULL) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    if (record_length > SIZE_MAX - prefix_length) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    total = prefix_length + record_length;
    if (buffer_size < total) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_BUFFER_TOO_SMALL);
    }

    memcpy(buffer, prefix, prefix_length);
    if (record_length != 0U) {
        memcpy(buffer + prefix_length, record, record_length);
    }
    *written = total;
    return LINK_UDS_RESULT_OK;
}

static LinkUdsResult uds_services_build_did_record(
    uint8_t service,
    uint16_t identifier,
    const uint8_t *record,
    size_t record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    const uint8_t prefix[] = {
        service,
        (uint8_t)(identifier >> 8U),
        (uint8_t)identifier
    };

    return uds_services_build_prefix_record(
        prefix, sizeof(prefix), record, record_length,
        buffer, buffer_size, written);
}

static LinkUdsResult uds_services_build_memory_request(
    uint8_t service,
    bool include_data_format_identifier,
    uint8_t data_format_identifier,
    uint64_t address,
    uint8_t address_width,
    uint64_t memory_size,
    uint8_t size_width,
    const uint8_t *record,
    size_t record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t prefix[19];
    size_t offset = 0U;
    size_t prefix_length;

    if (!uds_services_u64_fits_width(address, address_width) ||
        !uds_services_u64_fits_width(memory_size, size_width) ||
        memory_size == 0U ||
        !uds_services_record_valid(record, record_length)) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    prefix[offset++] = service;
    if (include_data_format_identifier) {
        prefix[offset++] = data_format_identifier;
    }
    prefix[offset++] =
        (uint8_t)(((uint8_t)size_width << 4U) | address_width);

    uds_services_write_u64_be(&prefix[offset], address, address_width);
    offset += address_width;
    uds_services_write_u64_be(&prefix[offset], memory_size, size_width);
    offset += size_width;
    prefix_length = offset;

    return uds_services_build_prefix_record(
        prefix, prefix_length, record, record_length,
        buffer, buffer_size, written);
}

static inline size_t link_uds_standard_service_count(void)
{
    return sizeof(link_uds_services) / sizeof(link_uds_services[0]);
}

static inline const LinkUdsServiceDefinition *link_uds_standard_service_at(size_t index)
{
    if (index >= link_uds_standard_service_count()) {
        return NULL;
    }
    return &link_uds_services[index];
}

static inline const LinkUdsServiceDefinition *link_uds_standard_service_find(uint8_t service)
{
    size_t index;

    for (index = 0U; index < link_uds_standard_service_count(); ++index) {
        if (link_uds_services[index].service == service) {
            return &link_uds_services[index];
        }
    }
    return NULL;
}

static inline const char *link_uds_service_effect_name(LinkUdsServiceEffect effect)
{
    switch (effect) {
    case LINK_UDS_SERVICE_EFFECT_READ_ONLY: return "read-only";
    case LINK_UDS_SERVICE_EFFECT_SESSION_CONTROL: return "session-control";
    case LINK_UDS_SERVICE_EFFECT_STATE_CHANGING: return "state-changing";
    case LINK_UDS_SERVICE_EFFECT_SECURITY: return "security";
    case LINK_UDS_SERVICE_EFFECT_PROGRAMMING: return "programming";
    }
    return "unknown";
}

static inline LinkUdsResult link_uds_build_registered_raw_request(
    uint8_t service,
    const uint8_t *record,
    size_t record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    const uint8_t prefix[] = { service };

    if (link_uds_standard_service_find(service) == NULL) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_UNSUPPORTED);
    }
    return uds_services_build_prefix_record(
        prefix, sizeof(prefix), record, record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_registered_subfunction_request(
    uint8_t service,
    uint8_t subfunction,
    bool suppress_positive_response,
    const uint8_t *record,
    size_t record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    const LinkUdsServiceDefinition *definition =
        link_uds_standard_service_find(service);
    uint8_t prefix[2];

    if (definition == NULL || !definition->uses_subfunction ||
        subfunction > 0x7fU) {
        return uds_services_write_failure(
            buffer, buffer_size, written,
            definition == NULL
                ? LINK_UDS_RESULT_UNSUPPORTED
                : LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    prefix[0] = service;
    prefix[1] = subfunction;
    if (suppress_positive_response) {
        prefix[1] |= 0x80U;
    }

    return uds_services_build_prefix_record(
        prefix, sizeof(prefix), record, record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_decode_subfunction_response(
    uint8_t service,
    uint8_t expected_subfunction,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsRecordResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsResult result;

    if (response == NULL || expected_subfunction > 0x7fU) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_response(service, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) {
        return result;
    }
    if (generic.data_length < 1U) {
        return LINK_UDS_RESULT_MALFORMED_PDU;
    }
    if ((generic.data[0] & 0x7fU) != expected_subfunction) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    response->record = generic.data + 1U;
    response->record_length = generic.data_length - 1U;
    return LINK_UDS_RESULT_OK;
}

static inline LinkUdsResult link_uds_decode_did_response(
    uint8_t service,
    uint16_t expected_identifier,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsRecordResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsResult result;
    uint16_t identifier;

    if (response == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_response(service, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) {
        return result;
    }
    if (generic.data_length < 2U) {
        return LINK_UDS_RESULT_MALFORMED_PDU;
    }

    identifier =
        (uint16_t)(((uint16_t)generic.data[0] << 8U) | generic.data[1]);
    if (identifier != expected_identifier) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    response->record = generic.data + 2U;
    response->record_length = generic.data_length - 2U;
    return LINK_UDS_RESULT_OK;
}

static inline LinkUdsResult link_uds_decode_empty_service_response(
    uint8_t service,
    const uint8_t *pdu,
    size_t pdu_length)
{
    LinkUdsResponse generic;
    LinkUdsResult result =
        link_uds_decode_response(service, pdu, pdu_length, &generic);

    if (result != LINK_UDS_RESULT_OK) {
        return result;
    }
    return generic.data_length == 0U
        ? LINK_UDS_RESULT_OK
        : LINK_UDS_RESULT_MALFORMED_PDU;
}

static inline LinkUdsResult link_uds_decode_transfer_data_response(
    uint8_t expected_block_sequence_counter,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsRecordResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsResult result;

    if (response == NULL) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_response(
        LINK_UDS_SERVICE_TRANSFER_DATA, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) {
        return result;
    }
    if (generic.data_length < 1U) {
        return LINK_UDS_RESULT_MALFORMED_PDU;
    }
    if (generic.data[0] != expected_block_sequence_counter) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    response->record = generic.data + 1U;
    response->record_length = generic.data_length - 1U;
    return LINK_UDS_RESULT_OK;
}

static inline LinkUdsResult link_uds_decode_routine_control_response(
    uint8_t expected_control_type,
    uint16_t expected_routine_identifier,
    const uint8_t *pdu,
    size_t pdu_length,
    LinkUdsRecordResponse *response)
{
    LinkUdsResponse generic;
    LinkUdsResult result;
    uint16_t routine_identifier;

    if (response == NULL || expected_control_type == 0U ||
        expected_control_type > 0x7fU) {
        return LINK_UDS_RESULT_INVALID_ARGUMENT;
    }
    result = link_uds_decode_response(
        LINK_UDS_SERVICE_ROUTINE_CONTROL, pdu, pdu_length, &generic);
    if (result != LINK_UDS_RESULT_OK) {
        return result;
    }
    if (generic.data_length < 3U) {
        return LINK_UDS_RESULT_MALFORMED_PDU;
    }
    if ((generic.data[0] & 0x7fU) != expected_control_type) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    routine_identifier =
        (uint16_t)(((uint16_t)generic.data[1] << 8U) | generic.data[2]);
    if (routine_identifier != expected_routine_identifier) {
        return LINK_UDS_RESULT_UNEXPECTED_RESPONSE;
    }

    response->record = generic.data + 3U;
    response->record_length = generic.data_length - 3U;
    return LINK_UDS_RESULT_OK;
}

static inline LinkUdsResult link_uds_build_ecu_reset_request(
    uint8_t reset_type,
    bool suppress_positive_response,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (reset_type == 0U || reset_type > 0x7fU) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_ECU_RESET, reset_type, suppress_positive_response,
        NULL, 0U, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_clear_diagnostic_information_request(
    uint32_t group_of_dtc,
    bool memory_selection_present,
    uint8_t memory_selection,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t record[4];
    size_t length = 3U;

    if (group_of_dtc > UINT32_C(0x00ffffff)) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    record[0] = (uint8_t)(group_of_dtc >> 16U);
    record[1] = (uint8_t)(group_of_dtc >> 8U);
    record[2] = (uint8_t)group_of_dtc;
    if (memory_selection_present) {
        record[3] = memory_selection;
        length = 4U;
    }

    return link_uds_build_registered_raw_request(
        LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION,
        record, length, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_communication_control_request(
    uint8_t control_type,
    bool suppress_positive_response,
    const uint8_t *communication_record,
    size_t communication_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_COMMUNICATION_CONTROL,
        control_type, suppress_positive_response,
        communication_record, communication_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_read_memory_by_address_request(
    uint64_t address,
    uint8_t address_width,
    uint64_t memory_size,
    uint8_t size_width,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return uds_services_build_memory_request(
        LINK_UDS_SERVICE_READ_MEMORY_BY_ADDRESS,
        false, 0U, address, address_width, memory_size, size_width,
        NULL, 0U, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_read_scaling_data_by_identifier_request(
    uint16_t identifier,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return uds_services_build_did_record(
        LINK_UDS_SERVICE_READ_SCALING_DATA_BY_IDENTIFIER,
        identifier, NULL, 0U, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_security_access_request(
    uint8_t access_type,
    bool suppress_positive_response,
    const uint8_t *security_record,
    size_t security_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (access_type == 0U || access_type > 0x7fU) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_SECURITY_ACCESS,
        access_type, suppress_positive_response,
        security_record, security_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_authentication_request(
    uint8_t subfunction,
    bool suppress_positive_response,
    const uint8_t *authentication_record,
    size_t authentication_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_AUTHENTICATION,
        subfunction, suppress_positive_response,
        authentication_record, authentication_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_read_data_by_periodic_identifier_request(
    uint8_t transmission_mode,
    const uint8_t *periodic_identifiers,
    size_t identifier_count,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t prefix[2];

    if (transmission_mode < LINK_UDS_PERIODIC_SEND_SLOW ||
        transmission_mode > LINK_UDS_PERIODIC_STOP ||
        periodic_identifiers == NULL || identifier_count == 0U) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    prefix[0] = LINK_UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER;
    prefix[1] = transmission_mode;
    return uds_services_build_prefix_record(
        prefix, sizeof(prefix),
        periodic_identifiers, identifier_count,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_dynamically_define_data_identifier_request(
    uint8_t subfunction,
    bool suppress_positive_response,
    const uint8_t *definition_record,
    size_t definition_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (subfunction < LINK_UDS_DYNAMIC_DID_DEFINE_BY_IDENTIFIER ||
        subfunction > LINK_UDS_DYNAMIC_DID_CLEAR) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER,
        subfunction, suppress_positive_response,
        definition_record, definition_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_write_data_by_identifier_request(
    uint16_t identifier,
    const uint8_t *data,
    size_t data_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (data == NULL || data_length == 0U) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return uds_services_build_did_record(
        LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER,
        identifier, data, data_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_input_output_control_by_identifier_request(
    uint16_t identifier,
    const uint8_t *control_record,
    size_t control_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return uds_services_build_did_record(
        LINK_UDS_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER,
        identifier, control_record, control_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_routine_control_request(
    uint8_t control_type,
    bool suppress_positive_response,
    uint16_t routine_identifier,
    const uint8_t *option_record,
    size_t option_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t prefix[4];

    if (control_type < LINK_UDS_ROUTINE_START ||
        control_type > LINK_UDS_ROUTINE_REQUEST_RESULTS ||
        !uds_services_record_valid(option_record, option_record_length)) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    prefix[0] = LINK_UDS_SERVICE_ROUTINE_CONTROL;
    prefix[1] = (uint8_t)(control_type |
        (suppress_positive_response ? 0x80U : 0x00U));
    prefix[2] = (uint8_t)(routine_identifier >> 8U);
    prefix[3] = (uint8_t)routine_identifier;

    return uds_services_build_prefix_record(
        prefix, sizeof(prefix), option_record, option_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_request_download_request(
    uint8_t data_format_identifier,
    uint64_t address,
    uint8_t address_width,
    uint64_t memory_size,
    uint8_t size_width,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return uds_services_build_memory_request(
        LINK_UDS_SERVICE_REQUEST_DOWNLOAD,
        true, data_format_identifier,
        address, address_width, memory_size, size_width,
        NULL, 0U, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_request_upload_request(
    uint8_t data_format_identifier,
    uint64_t address,
    uint8_t address_width,
    uint64_t memory_size,
    uint8_t size_width,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return uds_services_build_memory_request(
        LINK_UDS_SERVICE_REQUEST_UPLOAD,
        true, data_format_identifier,
        address, address_width, memory_size, size_width,
        NULL, 0U, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_transfer_data_request(
    uint8_t block_sequence_counter,
    const uint8_t *transfer_record,
    size_t transfer_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t prefix[2] = {
        LINK_UDS_SERVICE_TRANSFER_DATA,
        block_sequence_counter
    };

    return uds_services_build_prefix_record(
        prefix, sizeof(prefix),
        transfer_record, transfer_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_request_transfer_exit_request(
    const uint8_t *transfer_exit_record,
    size_t transfer_exit_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    return link_uds_build_registered_raw_request(
        LINK_UDS_SERVICE_REQUEST_TRANSFER_EXIT,
        transfer_exit_record, transfer_exit_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_request_file_transfer_request(
    uint8_t mode_of_operation,
    const uint8_t *file_record,
    size_t file_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    uint8_t prefix[2];

    if (mode_of_operation == 0U) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    prefix[0] = LINK_UDS_SERVICE_REQUEST_FILE_TRANSFER;
    prefix[1] = mode_of_operation;
    return uds_services_build_prefix_record(
        prefix, sizeof(prefix), file_record, file_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_write_memory_by_address_request(
    uint64_t address,
    uint8_t address_width,
    uint64_t memory_size,
    uint8_t size_width,
    const uint8_t *data,
    size_t data_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (data == NULL || data_length == 0U ||
        memory_size > (uint64_t)SIZE_MAX ||
        (uint64_t)data_length != memory_size) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    return uds_services_build_memory_request(
        LINK_UDS_SERVICE_WRITE_MEMORY_BY_ADDRESS,
        false, 0U, address, address_width, memory_size, size_width,
        data, data_length, buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_access_timing_parameter_request(
    uint8_t access_type,
    bool suppress_positive_response,
    const uint8_t *timing_parameter_record,
    size_t timing_parameter_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (access_type < LINK_UDS_ACCESS_TIMING_READ_EXTENDED_SET ||
        access_type > LINK_UDS_ACCESS_TIMING_SET_GIVEN ||
        (access_type == LINK_UDS_ACCESS_TIMING_SET_GIVEN &&
         timing_parameter_record_length == 0U) ||
        (access_type != LINK_UDS_ACCESS_TIMING_SET_GIVEN &&
         timing_parameter_record_length != 0U) ||
        !uds_services_record_valid(
            timing_parameter_record, timing_parameter_record_length)) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_ACCESS_TIMING_PARAMETER,
        access_type, suppress_positive_response,
        timing_parameter_record, timing_parameter_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_secured_data_transmission_request(
    const uint8_t *secured_data,
    size_t secured_data_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (secured_data == NULL || secured_data_length == 0U) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_raw_request(
        LINK_UDS_SERVICE_SECURED_DATA_TRANSMISSION,
        secured_data, secured_data_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_control_dtc_setting_request(
    uint8_t setting_type,
    bool suppress_positive_response,
    const uint8_t *control_option_record,
    size_t control_option_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (setting_type == 0U || setting_type > 0x7eU) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_CONTROL_DTC_SETTING,
        setting_type, suppress_positive_response,
        control_option_record, control_option_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_response_on_event_request(
    uint8_t event_type,
    bool suppress_positive_response,
    const uint8_t *event_record,
    size_t event_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (event_type > 0x7fU) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }
    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_RESPONSE_ON_EVENT,
        event_type, suppress_positive_response,
        event_record, event_record_length,
        buffer, buffer_size, written);
}

static inline LinkUdsResult link_uds_build_link_control_request(
    uint8_t control_type,
    bool suppress_positive_response,
    const uint8_t *baudrate_record,
    size_t baudrate_record_length,
    uint8_t *buffer,
    size_t buffer_size,
    size_t *written)
{
    if (control_type < LINK_UDS_LINK_VERIFY_FIXED_BAUDRATE ||
        control_type > LINK_UDS_LINK_TRANSITION_BAUDRATE ||
        ((control_type == LINK_UDS_LINK_TRANSITION_BAUDRATE) &&
         baudrate_record_length != 0U) ||
        ((control_type != LINK_UDS_LINK_TRANSITION_BAUDRATE) &&
         baudrate_record_length == 0U) ||
        !uds_services_record_valid(baudrate_record, baudrate_record_length)) {
        return uds_services_write_failure(
            buffer, buffer_size, written, LINK_UDS_RESULT_INVALID_ARGUMENT);
    }

    return link_uds_build_registered_subfunction_request(
        LINK_UDS_SERVICE_LINK_CONTROL,
        control_type, suppress_positive_response,
        baudrate_record, baudrate_record_length,
        buffer, buffer_size, written);
}

#ifdef __cplusplus
}
#endif

#endif
