// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_services.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        (void)fprintf(stderr, "FAIL: %s\n", message);
        ++failures;
    }
}

static void expect_bytes(
    LinkUdsResult result,
    LinkUdsResult expected_result,
    const uint8_t *actual,
    size_t actual_length,
    const uint8_t *expected,
    size_t expected_length,
    const char *message)
{
    check(result == expected_result, message);
    if (result == LINK_UDS_RESULT_OK && expected_result == LINK_UDS_RESULT_OK) {
        check(actual_length == expected_length, message);
        check(actual_length == expected_length &&
              memcmp(actual, expected, expected_length) == 0,
              message);
    }
}

static void test_service_catalogue(void)
{
    static const uint8_t services[] = {
        0x83U, 0x14U, 0x28U, 0x85U, 0x10U, 0x11U, 0x2fU, 0x87U,
        0x22U, 0x38U, 0x19U, 0x23U, 0x34U, 0x37U, 0x35U, 0x31U,
        0x27U, 0x3eU, 0x36U, 0x2eU, 0x3dU, 0x2cU, 0x84U, 0x86U,
        0x24U, 0x2aU, 0x29U
    };
    bool seen[256] = { false };
    size_t index;

    check(link_uds_standard_service_count() == LINK_UDS_STANDARD_SERVICE_COUNT,
          "27-service catalogue count");
    check(link_uds_standard_service_at(LINK_UDS_STANDARD_SERVICE_COUNT) == NULL,
          "service catalogue bounds");

    for (index = 0U; index < sizeof(services); ++index) {
        const LinkUdsServiceDefinition *definition =
            link_uds_standard_service_find(services[index]);
        check(definition != NULL, "catalogue contains expected service");
        if (definition != NULL) {
            check(definition->service == services[index],
                  "catalogue service id");
            check(definition->name != NULL && definition->name[0] != '\0',
                  "catalogue service name");
            check(!seen[definition->service], "catalogue ids unique");
            seen[definition->service] = true;
        }
    }

    check(link_uds_standard_service_find(0x99U) == NULL,
          "unknown service absent");
    check(strcmp(link_uds_service_effect_name(
                     LINK_UDS_SERVICE_EFFECT_PROGRAMMING),
                 "programming") == 0,
          "service effect name");
}

static void test_requested_issue_services(void)
{
    uint8_t buffer[64];
    size_t written = 0U;
    LinkUdsResult result;

    {
        const uint8_t expected[] = { 0x83U, 0x03U };
        result = link_uds_build_access_timing_parameter_request(
            LINK_UDS_ACCESS_TIMING_READ_ACTIVE, false,
            NULL, 0U, buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "AccessTimingParameter");
    }
    {
        const uint8_t secured[] = { 0xaaU, 0xbbU };
        const uint8_t expected[] = { 0x84U, 0xaaU, 0xbbU };
        result = link_uds_build_secured_data_transmission_request(
            secured, sizeof(secured), buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "SecuredDataTransmission");
    }
    {
        const uint8_t expected[] = { 0x85U, 0x02U };
        result = link_uds_build_control_dtc_setting_request(
            LINK_UDS_DTC_SETTING_OFF, false, NULL, 0U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ControlDTCSetting");
    }
    {
        const uint8_t record[] = { 0x01U, 0x22U, 0xf1U, 0x90U };
        const uint8_t expected[] = { 0x86U, 0x01U, 0x01U, 0x22U, 0xf1U, 0x90U };
        result = link_uds_build_response_on_event_request(
            0x01U, false, record, sizeof(record),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ResponseOnEvent");
    }
    {
        const uint8_t baud[] = { 0x01U };
        const uint8_t expected[] = { 0x87U, 0x01U, 0x01U };
        result = link_uds_build_link_control_request(
            LINK_UDS_LINK_VERIFY_FIXED_BAUDRATE, false,
            baud, sizeof(baud), buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "LinkControl");
    }
    {
        const uint8_t expected[] = { 0x23U, 0x23U, 0x12U, 0x34U, 0x56U, 0x00U, 0x20U };
        result = link_uds_build_read_memory_by_address_request(
            UINT64_C(0x123456), 3U, UINT64_C(0x20), 2U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ReadMemoryByAddress");
    }
    {
        const uint8_t expected[] = { 0x24U, 0xf1U, 0x90U };
        result = link_uds_build_read_scaling_data_by_identifier_request(
            0xf190U, buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ReadScalingDataByIdentifier");
    }
    {
        const uint8_t ids[] = { 0x01U, 0x02U };
        const uint8_t expected[] = { 0x2aU, 0x03U, 0x01U, 0x02U };
        result = link_uds_build_read_data_by_periodic_identifier_request(
            LINK_UDS_PERIODIC_SEND_FAST, ids, sizeof(ids),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ReadDataByPeriodicIdentifier");
    }
    {
        const uint8_t definition[] = { 0xf2U, 0x00U, 0xf1U, 0x90U, 0x01U, 0x04U };
        const uint8_t expected[] = {
            0x2cU, 0x01U, 0xf2U, 0x00U, 0xf1U, 0x90U, 0x01U, 0x04U
        };
        result = link_uds_build_dynamically_define_data_identifier_request(
            LINK_UDS_DYNAMIC_DID_DEFINE_BY_IDENTIFIER, false,
            definition, sizeof(definition),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "DynamicallyDefineDataIdentifier");
    }
    {
        const uint8_t data[] = { 0xdeU, 0xadU };
        const uint8_t expected[] = { 0x3dU, 0x12U, 0x00U, 0x80U, 0x02U, 0xdeU, 0xadU };
        result = link_uds_build_write_memory_by_address_request(
            0x80U, 2U, 2U, 1U,
            data, sizeof(data), buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "WriteMemoryByAddress");
    }
    {
        const uint8_t expected[] = { 0x14U, 0xffU, 0xffU, 0xffU };
        result = link_uds_build_clear_diagnostic_information_request(
            UINT32_C(0x00ffffff), false, 0U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ClearDiagnosticInformation");
    }
    {
        const uint8_t control[] = { 0x03U, 0x55U };
        const uint8_t expected[] = { 0x2fU, 0x12U, 0x34U, 0x03U, 0x55U };
        result = link_uds_build_input_output_control_by_identifier_request(
            0x1234U, control, sizeof(control),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "InputOutputControlByIdentifier");
    }
    {
        const uint8_t option[] = { 0xaaU };
        const uint8_t expected[] = { 0x31U, 0x01U, 0x12U, 0x34U, 0xaaU };
        result = link_uds_build_routine_control_request(
            LINK_UDS_ROUTINE_START, false, 0x1234U,
            option, sizeof(option),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "RoutineControl");
    }
}

static void test_remaining_catalogue_builders(void)
{
    uint8_t buffer[64];
    size_t written = 0U;
    LinkUdsResult result;

    {
        const uint8_t expected[] = { 0x11U, 0x01U };
        result = link_uds_build_ecu_reset_request(
            0x01U, false, buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "ECUReset");
    }
    {
        const uint8_t communication[] = { 0x01U };
        const uint8_t expected[] = { 0x28U, 0x00U, 0x01U };
        result = link_uds_build_communication_control_request(
            0x00U, false, communication, sizeof(communication),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "CommunicationControl");
    }
    {
        const uint8_t expected[] = { 0x27U, 0x01U };
        result = link_uds_build_security_access_request(
            0x01U, false, NULL, 0U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "SecurityAccess");
    }
    {
        const uint8_t auth[] = { 0x00U, 0x01U };
        const uint8_t expected[] = { 0x29U, 0x01U, 0x00U, 0x01U };
        result = link_uds_build_authentication_request(
            0x01U, false, auth, sizeof(auth),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "Authentication");
    }
    {
        const uint8_t data[] = { 0xaaU, 0xbbU };
        const uint8_t expected[] = { 0x2eU, 0xf1U, 0x90U, 0xaaU, 0xbbU };
        result = link_uds_build_write_data_by_identifier_request(
            0xf190U, data, sizeof(data),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "WriteDataByIdentifier");
    }
    {
        const uint8_t expected[] = { 0x34U, 0x00U, 0x22U, 0x10U, 0x00U, 0x00U, 0x20U };
        result = link_uds_build_request_download_request(
            0x00U, 0x1000U, 2U, 0x20U, 2U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "RequestDownload");
    }
    {
        const uint8_t expected[] = { 0x35U, 0x00U, 0x22U, 0x10U, 0x00U, 0x00U, 0x20U };
        result = link_uds_build_request_upload_request(
            0x00U, 0x1000U, 2U, 0x20U, 2U,
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "RequestUpload");
    }
    {
        const uint8_t data[] = { 0xaaU };
        const uint8_t expected[] = { 0x36U, 0x05U, 0xaaU };
        result = link_uds_build_transfer_data_request(
            0x05U, data, sizeof(data),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "TransferData");
    }
    {
        const uint8_t expected[] = { 0x37U };
        result = link_uds_build_request_transfer_exit_request(
            NULL, 0U, buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "RequestTransferExit");
    }
    {
        const uint8_t record[] = { 0x00U, 0x01U, 'x' };
        const uint8_t expected[] = { 0x38U, 0x04U, 0x00U, 0x01U, 'x' };
        result = link_uds_build_request_file_transfer_request(
            0x04U, record, sizeof(record),
            buffer, sizeof(buffer), &written);
        expect_bytes(result, LINK_UDS_RESULT_OK, buffer, written,
                     expected, sizeof(expected), "RequestFileTransfer");
    }
}

static void test_response_helpers_and_errors(void)
{
    uint8_t buffer[8];
    size_t written = 99U;
    LinkUdsRecordResponse record;

    {
        const uint8_t response[] = { 0xc5U, 0x02U };
        check(link_uds_decode_subfunction_response(
                  LINK_UDS_SERVICE_CONTROL_DTC_SETTING,
                  LINK_UDS_DTC_SETTING_OFF,
                  response, sizeof(response), &record) == LINK_UDS_RESULT_OK &&
              record.record_length == 0U,
              "subfunction response");
    }
    {
        const uint8_t response[] = { 0x6eU, 0xf1U, 0x90U };
        check(link_uds_decode_did_response(
                  LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER,
                  0xf190U, response, sizeof(response), &record) ==
              LINK_UDS_RESULT_OK && record.record_length == 0U,
              "DID response");
    }
    {
        const uint8_t response[] = { 0x71U, 0x01U, 0x12U, 0x34U, 0x55U };
        check(link_uds_decode_routine_control_response(
                  LINK_UDS_ROUTINE_START, 0x1234U,
                  response, sizeof(response), &record) == LINK_UDS_RESULT_OK &&
              record.record_length == 1U && record.record[0] == 0x55U,
              "routine response");
    }
    {
        const uint8_t response[] = { 0x76U, 0x05U, 0xaaU };
        check(link_uds_decode_transfer_data_response(
                  0x05U, response, sizeof(response), &record) ==
              LINK_UDS_RESULT_OK &&
              record.record_length == 1U && record.record[0] == 0xaaU,
              "transfer response");
    }
    {
        const uint8_t response[] = { 0x54U };
        check(link_uds_decode_empty_service_response(
                  LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION,
                  response, sizeof(response)) == LINK_UDS_RESULT_OK,
              "empty response");
    }

    check(link_uds_build_read_memory_by_address_request(
              0x100U, 1U, 1U, 1U,
              buffer, sizeof(buffer), &written) ==
          LINK_UDS_RESULT_INVALID_ARGUMENT && written == 0U,
          "address width overflow rejected");

    check(link_uds_build_access_timing_parameter_request(
              LINK_UDS_ACCESS_TIMING_SET_GIVEN, false,
              NULL, 0U, buffer, sizeof(buffer), &written) ==
          LINK_UDS_RESULT_INVALID_ARGUMENT && written == 0U,
          "timing write requires record");

    check(link_uds_build_registered_raw_request(
              0x99U, NULL, 0U, buffer, sizeof(buffer), &written) ==
          LINK_UDS_RESULT_UNSUPPORTED && written == 0U,
          "unknown service rejected");

    check(link_uds_build_write_data_by_identifier_request(
              0x1234U, (const uint8_t *)"x", 1U,
              buffer, 3U, &written) == LINK_UDS_RESULT_BUFFER_TOO_SMALL &&
          written == 0U,
          "buffer-too-small is bounded");
}

static void test_existing_uds_core(void)
{
    uint8_t request[8];
    size_t written = 0U;
    LinkUdsSessionResponse session;
    LinkUdsClient client;
    LinkUdsClientConfig config = { 50000U, 500000U };
    LinkUdsResponse response;
    const uint8_t session_reply[] = {
        0x50U, 0x03U, 0x00U, 0x32U, 0x00U, 0x0aU
    };
    const uint8_t pending_reply[] = { 0x7fU, 0x22U, 0x78U };
    const uint8_t did_request[] = { 0x22U, 0xf1U, 0x90U };

    check(link_uds_build_session_control_request(
              LINK_UDS_SESSION_EXTENDED, false,
              request, sizeof(request), &written) == LINK_UDS_RESULT_OK &&
              written == 2U && request[0] == 0x10U && request[1] == 0x03U,
          "build session request");

    memset(&session, 0, sizeof(session));
    check(link_uds_decode_session_control_response(
              session_reply, sizeof(session_reply), LINK_UDS_SESSION_EXTENDED,
              &session) == LINK_UDS_RESULT_OK && session.timing_present &&
              session.p2_server_max_ms == 50U &&
              session.p2_star_server_max_10ms == 10U,
          "decode session timing");

    check(link_uds_client_init(&client, &config) == LINK_UDS_RESULT_OK,
          "client init");
    check(link_uds_client_begin(
              &client, did_request, sizeof(did_request), 1000U) ==
          LINK_UDS_RESULT_OK,
          "client begin");
    check(link_uds_client_accept(
              &client, pending_reply, sizeof(pending_reply),
              2000U, &response) == LINK_UDS_RESULT_RESPONSE_PENDING &&
              client.state == LINK_UDS_CLIENT_RESPONSE_PENDING,
          "response pending extends client wait");
    check(link_uds_client_tick(&client, 2001U) == LINK_UDS_RESULT_WAITING,
          "client waiting after pending");
}

int main(void)
{
    test_existing_uds_core();
    test_service_catalogue();
    test_requested_issue_services();
    test_remaining_catalogue_builders();
    test_response_helpers_and_errors();

    if (failures != 0) {
        (void)fprintf(stderr, "%d UDS service test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
