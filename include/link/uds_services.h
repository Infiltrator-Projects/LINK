// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_services.h
 * @brief Complete ISO 14229 UDS service catalogue and bounded request codecs.
 *
 * Codec availability is not authorization. These APIs serialize and validate
 * product-neutral UDS PDUs only; the Discover safety classifier remains the
 * independent deny-by-default transmission policy.
 */
#ifndef LINK_UDS_SERVICES_H
#define LINK_UDS_SERVICES_H

#include "link/uds.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_STANDARD_SERVICE_COUNT 27U

#ifndef LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL
#define LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL 0x10U
#endif
#define LINK_UDS_SERVICE_ECU_RESET 0x11U

#define LINK_UDS_ECU_RESET_HARD 0x01U
#define LINK_UDS_ECU_RESET_KEY_OFF_ON 0x02U
#define LINK_UDS_ECU_RESET_SOFT 0x03U
#define LINK_UDS_ECU_RESET_ENABLE_RAPID_POWER_SHUTDOWN 0x04U
#define LINK_UDS_ECU_RESET_DISABLE_RAPID_POWER_SHUTDOWN 0x05U
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

size_t link_uds_standard_service_count(void);
const LinkUdsServiceDefinition *link_uds_standard_service_at(size_t index);
const LinkUdsServiceDefinition *link_uds_standard_service_find(uint8_t service);
const char *link_uds_service_effect_name(LinkUdsServiceEffect effect);

LinkUdsResult link_uds_build_registered_raw_request(
    uint8_t service, const uint8_t *record, size_t record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_registered_subfunction_request(
    uint8_t service, uint8_t subfunction, bool suppress_positive_response,
    const uint8_t *record, size_t record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_decode_subfunction_response(
    uint8_t service, uint8_t expected_subfunction,
    const uint8_t *pdu, size_t pdu_length, LinkUdsRecordResponse *response);
LinkUdsResult link_uds_decode_did_response(
    uint8_t service, uint16_t expected_identifier,
    const uint8_t *pdu, size_t pdu_length, LinkUdsRecordResponse *response);
LinkUdsResult link_uds_decode_empty_service_response(
    uint8_t service, const uint8_t *pdu, size_t pdu_length);
LinkUdsResult link_uds_decode_transfer_data_response(
    uint8_t expected_block_sequence_counter,
    const uint8_t *pdu, size_t pdu_length, LinkUdsRecordResponse *response);
LinkUdsResult link_uds_decode_routine_control_response(
    uint8_t expected_control_type, uint16_t expected_routine_identifier,
    const uint8_t *pdu, size_t pdu_length, LinkUdsRecordResponse *response);

LinkUdsResult link_uds_build_ecu_reset_request(
    uint8_t reset_type, bool suppress_positive_response,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_clear_diagnostic_information_request(
    uint32_t group_of_dtc, bool memory_selection_present,
    uint8_t memory_selection, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_communication_control_request(
    uint8_t control_type, bool suppress_positive_response,
    const uint8_t *communication_record, size_t communication_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_read_memory_by_address_request(
    uint64_t address, uint8_t address_width,
    uint64_t memory_size, uint8_t size_width,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_read_scaling_data_by_identifier_request(
    uint16_t identifier, uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_security_access_request(
    uint8_t access_type, bool suppress_positive_response,
    const uint8_t *security_record, size_t security_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_authentication_request(
    uint8_t subfunction, bool suppress_positive_response,
    const uint8_t *authentication_record, size_t authentication_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_read_data_by_periodic_identifier_request(
    uint8_t transmission_mode, const uint8_t *periodic_identifiers,
    size_t identifier_count, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_dynamically_define_data_identifier_request(
    uint8_t subfunction, bool suppress_positive_response,
    const uint8_t *definition_record, size_t definition_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_write_data_by_identifier_request(
    uint16_t identifier, const uint8_t *data, size_t data_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_input_output_control_by_identifier_request(
    uint16_t identifier, const uint8_t *control_record,
    size_t control_record_length, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_routine_control_request(
    uint8_t control_type, bool suppress_positive_response,
    uint16_t routine_identifier, const uint8_t *option_record,
    size_t option_record_length, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_request_download_request(
    uint8_t data_format_identifier, uint64_t address, uint8_t address_width,
    uint64_t memory_size, uint8_t size_width,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_request_upload_request(
    uint8_t data_format_identifier, uint64_t address, uint8_t address_width,
    uint64_t memory_size, uint8_t size_width,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_transfer_data_request(
    uint8_t block_sequence_counter, const uint8_t *transfer_record,
    size_t transfer_record_length, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_request_transfer_exit_request(
    const uint8_t *transfer_exit_record, size_t transfer_exit_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_request_file_transfer_request(
    uint8_t mode_of_operation, const uint8_t *file_record,
    size_t file_record_length, uint8_t *buffer, size_t buffer_size,
    size_t *written);
LinkUdsResult link_uds_build_write_memory_by_address_request(
    uint64_t address, uint8_t address_width,
    uint64_t memory_size, uint8_t size_width,
    const uint8_t *data, size_t data_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_access_timing_parameter_request(
    uint8_t access_type, bool suppress_positive_response,
    const uint8_t *timing_parameter_record, size_t timing_parameter_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_secured_data_transmission_request(
    const uint8_t *secured_data, size_t secured_data_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_control_dtc_setting_request(
    uint8_t setting_type, bool suppress_positive_response,
    const uint8_t *control_option_record, size_t control_option_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_response_on_event_request(
    uint8_t event_type, bool suppress_positive_response,
    const uint8_t *event_record, size_t event_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);
LinkUdsResult link_uds_build_link_control_request(
    uint8_t control_type, bool suppress_positive_response,
    const uint8_t *baudrate_record, size_t baudrate_record_length,
    uint8_t *buffer, size_t buffer_size, size_t *written);

#ifdef __cplusplus
}
#endif

#endif
