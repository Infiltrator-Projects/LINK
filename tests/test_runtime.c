// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/parameter.h"
#include "link/parameter_store.h"
#include "link/scheduler.h"
#include "link/telemetry.h"
#include "link/transport.h"
#include "link/version.h"
#include "link/mercedes_me_adapter.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "check failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); return 1; } } while (0)

typedef struct { char data[8192]; size_t length; } Buffer;
static bool sink(void *context, const char *bytes, size_t length)
{
    Buffer *buffer = context;
    if (buffer == NULL || bytes == NULL || length > sizeof(buffer->data) - buffer->length - 1U) return false;
    memcpy(buffer->data + buffer->length, bytes, length); buffer->length += length; buffer->data[buffer->length] = '\0'; return true;
}
static const char *pid_name(uint8_t pid) { return pid == 0x0cU ? "Engine speed" : "PID"; }
static const char *unit_name(uint32_t unit) { return unit == (uint32_t)LINK_OBD2_UNIT_RPM ? "rpm" : "unit"; }
static const char *result_name(uint32_t result) { return result == 0U ? "ok" : "error"; }
static size_t occurrence_count(const char *text, const char *needle)
{
    size_t count = 0U, length;
    if (text == NULL || needle == NULL || needle[0] == '\0') return 0U;
    length = strlen(needle);
    while ((text = strstr(text, needle)) != NULL) { ++count; text += length; }
    return count;
}

int main(void)
{
    LinkParameterSample parameter;
    LinkParameterStore parameter_store;
    LinkScheduler scheduler;
    LinkSchedulerDispatch dispatch;
    uint8_t bits[LINK_OBD2_PID_SET_BYTES] = {0};
    LinkTelemetryStore telemetry;
    LinkStructuredTelemetryStore structured_telemetry;
    LinkTelemetryMeasurement measurement = { 0x0cU, 1234.5, LINK_OBD2_UNIT_RPM };
    LinkTelemetrySample sample;
    LinkTelemetrySessionMetadata metadata;
    LinkTelemetryRecorder recorder;
    Buffer output = {{0}, 0U};
    char formatted[64];

    CHECK(link_adapter_kind_from_bluetooth_name("MB-123456") ==
          LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE);
    CHECK(link_adapter_kind_from_bluetooth_name("MB-812345") ==
          LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE);
    CHECK(link_adapter_kind_from_bluetooth_name("MB-512345") ==
          LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE);
    CHECK(link_adapter_kind_from_bluetooth_name("MB-012345") ==
          LINK_ADAPTER_KIND_UNKNOWN);
    CHECK(link_adapter_kind_from_bluetooth_name("iOS-VLink") ==
          LINK_ADAPTER_KIND_ELM327);
    CHECK(link_adapter_kind_from_bluetooth_name("Vgate iCar Pro") ==
          LINK_ADAPTER_KIND_ELM327);
    CHECK(link_adapter_kind_requires_native_protocol(
              LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE));
    CHECK(!link_adapter_kind_requires_native_protocol(
              LINK_ADAPTER_KIND_ELM327));
    CHECK(strcmp(link_adapter_kind_name(
                     LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE),
                 "mercedes-me-native") == 0);
    {
        LinkAdapterCapabilities capabilities;
        CHECK(link_adapter_capabilities(
            LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE, &capabilities));
        CHECK((capabilities.flags & LINK_ADAPTER_CAP_NATIVE_DIAGNOSTIC) != 0U);
        CHECK((capabilities.flags & LINK_ADAPTER_CAP_SECURE_SESSION) != 0U);
        CHECK((capabilities.flags & LINK_ADAPTER_CAP_CAN_29BIT) == 0U);
        CHECK(capabilities.max_standard_can_id == UINT32_C(0x7ff));
        CHECK(capabilities.max_raw_can_payload == 8U);
        CHECK(capabilities.max_isotp_payload == 100U);
        CHECK(capabilities.max_filter_ids == 15U);
        CHECK(link_adapter_has_capability(
            LINK_ADAPTER_KIND_TACTRIX_OPENPORT2,
            LINK_ADAPTER_CAP_ISOTP | LINK_ADAPTER_CAP_CAN_29BIT));
        CHECK(link_adapter_kind_requires_native_protocol(
            LINK_ADAPTER_KIND_STM32_LINK));
    }

    CHECK(link_parameter_obd2_definition_count() == 62U);
    CHECK(link_parameter_from_obd2_scalar(0x0cU, LINK_OBD2_UNIT_RPM, 1234.5, 10U, &parameter));
    CHECK(strcmp(parameter.definition->stable_key, "obd2.engine.rpm") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x11U)->name,
                 "Absolute throttle valve position") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x49U)->stable_key,
                 "obd2.driver.accelerator_pedal_d") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x2fU)->stable_key,
                 "obd2.fuel.tank_level") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x1fU)->stable_key,
                 "obd2.engine.runtime") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x31U)->stable_key,
                 "obd2.maintenance.distance_since_clear") == 0);
    CHECK(strcmp(link_parameter_obd2_definition(0x4dU)->stable_key,
                 "obd2.emissions.mil_runtime") == 0);
    CHECK(link_parameter_obd2_definition(0x06U) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0x06U)->name,
                 "Short term fuel trim - Bank 1") == 0);
    CHECK(link_parameter_obd2_definition(0xa6U) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0xa6U)->name,
                 "Odometer") == 0);
    CHECK(link_parameter_obd2_definition(0x8dU) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0x8dU)->name,
                 "Throttle position G") == 0);
    CHECK(link_parameter_obd2_definition(0xc7U) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0xc7U)->name,
                 "Distance since reflash or module replacement") == 0);
    CHECK(link_parameter_obd2_definition(0xaaU) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0xaaU)->stable_key,
                 "obd2.vehicle.max_speed_limit") == 0);
    CHECK(link_parameter_obd2_definition(0xb2U) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0xb2U)->stable_key,
                 "obd2.battery.traction_soh") == 0);
    CHECK(link_parameter_obd2_definition(0xd3U) != NULL);
    CHECK(strcmp(link_parameter_obd2_definition(0xd3U)->stable_key,
                 "obd2.engine.odometer") == 0);
    {
        LinkObd2UnitCode exhaust_unit = LINK_OBD2_UNIT_NONE;
        CHECK(link_parameter_obd2_expected_unit(0x9eU, &exhaust_unit));
        CHECK(exhaust_unit == LINK_OBD2_UNIT_KILOGRAMS_PER_HOUR);
    }
    CHECK(link_parameter_obd2_definition(0x7aU) == NULL);
    CHECK(link_parameter_obd2_definition(0x7cU) == NULL);
    CHECK(link_obd2_pid_definition(0x01U, 0x7aU) != NULL);
    CHECK(link_obd2_mode01_identifier_count() == 256U);
    CHECK(link_obd2_mode01_assigned_count() == 220U);
    CHECK(link_parameter_format_value(
              link_parameter_obd2_definition(0x23U),
              true, 123400.0, formatted, sizeof(formatted)));
    CHECK(strcmp(formatted, "123.4 MPa") == 0);
    CHECK(link_parameter_format_value(
              link_parameter_obd2_definition(0x23U),
              true, 990.0, formatted, sizeof(formatted)));
    CHECK(strcmp(formatted, "990 kPa") == 0);
    CHECK(link_parameter_format_value(
              link_parameter_obd2_definition(0x46U),
              true, 18.0, formatted, sizeof(formatted)));
    CHECK(strcmp(formatted, "18.0 °C") == 0);
    link_parameter_store_init(&parameter_store);
    CHECK(link_parameter_store_register(&parameter_store, parameter.definition) == LINK_PARAMETER_STORE_OK);
    CHECK(link_parameter_store_record(&parameter_store, &parameter) == LINK_PARAMETER_STORE_OK);
    CHECK(link_parameter_store_total_sample_count(&parameter_store) == 1U);

    {
        LinkScheduler unified;
        LinkSchedulerDispatch unified_dispatch;
        link_scheduler_init(&unified);
        CHECK(link_scheduler_add(&unified, 0x0cU, 250U,
                  LINK_SCHEDULER_PRIORITY_CRITICAL, 100U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_add_external(&unified, UINT32_C(0x4d420001),
                  750U, LINK_SCHEDULER_PRIORITY_HIGH, 50U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_next(&unified, 50U, &unified_dispatch) ==
              LINK_SCHEDULER_NEXT_READY);
        CHECK(unified_dispatch.kind == LINK_SCHEDULER_ITEM_EXTERNAL);
        CHECK(unified_dispatch.external_token == UINT32_C(0x4d420001));
        CHECK(link_scheduler_mark_dispatched(
                  &unified, unified_dispatch.index, 50U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_set_external_enabled(
                  &unified, UINT32_C(0x4d420001), false) ==
              LINK_SCHEDULER_RESULT_OK);
    }

    /*
     * Under adapter overload, due priority must matter: an older LOW item that
     * is still within one of its own intervals must not hold up a newly due
     * HIGH OEM live transaction.
     */
    {
        LinkScheduler overloaded;
        LinkSchedulerDispatch overloaded_dispatch;
        link_scheduler_init(&overloaded);
        CHECK(link_scheduler_add(&overloaded, 0x05U, 3000U,
                  LINK_SCHEDULER_PRIORITY_LOW, 1000U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_add_external(&overloaded, UINT32_C(0x4d420001),
                  750U, LINK_SCHEDULER_PRIORITY_HIGH, 3000U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_next(&overloaded, 3500U,
                  &overloaded_dispatch) == LINK_SCHEDULER_NEXT_READY);
        CHECK(overloaded_dispatch.kind == LINK_SCHEDULER_ITEM_EXTERNAL);
        CHECK(overloaded_dispatch.external_token == UINT32_C(0x4d420001));
    }

    /*
     * Priority must not become starvation. A LOW item that has missed three of
     * its requested periods ages to CRITICAL and is serviced ahead of a newly
     * due CRITICAL item because its original deadline is older.
     */
    {
        LinkScheduler aged;
        LinkSchedulerDispatch aged_dispatch;
        link_scheduler_init(&aged);
        CHECK(link_scheduler_add(&aged, 0x05U, 1000U,
                  LINK_SCHEDULER_PRIORITY_LOW, 1000U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_add(&aged, 0x0cU, 500U,
                  LINK_SCHEDULER_PRIORITY_CRITICAL, 4000U) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_next(&aged, 4000U, &aged_dispatch) ==
              LINK_SCHEDULER_NEXT_READY);
        CHECK(aged_dispatch.pid_valid);
        CHECK(aged_dispatch.pid == 0x05U);
    }

    link_scheduler_init(&scheduler);
    CHECK(link_scheduler_add(&scheduler, 0x0cU, 250U, LINK_SCHEDULER_PRIORITY_CRITICAL, 100U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(link_scheduler_next(&scheduler, 100U, &dispatch) == LINK_SCHEDULER_NEXT_READY);
    CHECK(dispatch.pid == 0x0cU);
    bits[0x0cU / 8U] |= (uint8_t)(1U << (0x0cU % 8U));
    CHECK(link_scheduler_configure_standard_obd2_bits(&scheduler, bits, 0U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(scheduler.count == 1U && scheduler.items[0].pid == 0x0cU);
    CHECK(scheduler.items[0].interval_ms == 500U);

    memset(bits, 0, sizeof(bits));
    bits[0x06U / 8U] |= (uint8_t)(1U << (0x06U % 8U));
    bits[0xa6U / 8U] |= (uint8_t)(1U << (0xa6U % 8U));
    bits[0x7aU / 8U] |= (uint8_t)(1U << (0x7aU % 8U));
    CHECK(link_scheduler_configure_standard_obd2_bits(
              &scheduler, bits, 0U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(scheduler.count == 3U);
    CHECK(link_scheduler_set_enabled(&scheduler, 0x06U, false) ==
          LINK_SCHEDULER_RESULT_OK);
    CHECK(link_scheduler_set_enabled(&scheduler, 0xa6U, false) ==
          LINK_SCHEDULER_RESULT_OK);
    CHECK(link_scheduler_set_enabled(&scheduler, 0x7aU, false) ==
          LINK_SCHEDULER_RESULT_OK);

    memset(bits, 0, sizeof(bits));
    for (size_t definition_index = 0U;
         definition_index < link_parameter_obd2_definition_count();
         ++definition_index) {
        const LinkParameterDefinition *definition =
            link_parameter_obd2_definition_at(definition_index);
        CHECK(definition != NULL);
        CHECK(definition->key.identifier <= UINT8_MAX);
        {
            const uint8_t pid = (uint8_t)definition->key.identifier;
            bits[pid / 8U] |= (uint8_t)(1U << (pid % 8U));
        }
    }
    CHECK(link_scheduler_configure_standard_obd2_bits(
              &scheduler, bits, 0U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(scheduler.count == link_parameter_obd2_definition_count());

    /*
     * Full SAE live contract: if a vehicle advertises every assigned classic
     * Mode 01 PID, the scheduler must retain every live PID rather than only
     * the scalar compatibility subset. Support-page PIDs are discovery
     * metadata and are intentionally not polled as live measurements.
     */
    {
        size_t expected_live = 0U;
        memset(bits, 0xff, sizeof(bits));
        for (size_t definition_index = 0U;
             definition_index < link_obd2_pid_definition_count();
             ++definition_index) {
            const LinkObd2PidDefinition *definition =
                link_obd2_pid_definition_at(definition_index);
            if (definition != NULL &&
                definition->mode == UINT8_C(0x01) &&
                (definition->pid & UINT8_C(0x1f)) != 0U) {
                ++expected_live;
            }
        }
        CHECK(expected_live > 64U);
        CHECK(expected_live <= LINK_SCHEDULER_MAX_ITEMS);
        CHECK(link_scheduler_configure_standard_obd2_bits(
                  &scheduler, bits, 0U) == LINK_SCHEDULER_RESULT_OK);
        CHECK(scheduler.count == expected_live);
        CHECK(link_scheduler_set_enabled(&scheduler, 0x7aU, false) ==
              LINK_SCHEDULER_RESULT_OK);
        CHECK(link_scheduler_set_enabled(&scheduler, 0xccU, false) ==
              LINK_SCHEDULER_RESULT_OK);
    }

    link_telemetry_store_init(&telemetry);
    CHECK(link_telemetry_store_record(&telemetry, 20U, &measurement));
    CHECK(link_telemetry_store_latest(&telemetry, 0x0cU, &sample));
    CHECK(sample.measurement.value == 1234.5);
    CHECK(link_telemetry_store_record_transcript(&telemetry, 30U, "010C", 0U, "41 0C 13 4A"));

    {
        LinkResponderTelemetryStore responders;
        LinkResponderTelemetrySample responder_sample;
        link_responder_telemetry_store_init(&responders);
        CHECK(link_responder_telemetry_store_record(
            &responders, 21U, 0x7e8U, false, &measurement));
        CHECK(link_responder_telemetry_store_record(
            &responders, 22U, 0x7e9U, false, &measurement));
        CHECK(link_responder_telemetry_store_history_count(&responders) == 2U);
        CHECK(link_responder_telemetry_store_total_sample_count(&responders) == 2U);
        CHECK(link_responder_telemetry_store_history_at(
            &responders, 1U, &responder_sample));
        CHECK(responder_sample.responder_id == 0x7e9U);
        CHECK(responder_sample.measurement.pid == measurement.pid);
        link_responder_telemetry_store_clear(&responders);
        CHECK(link_responder_telemetry_store_history_count(&responders) == 0U);
        CHECK(!link_responder_telemetry_store_record(
            &responders, 23U, 0x800U, false, &measurement));
    }

    {
        static const uint8_t dpf_pressure_payload[] = {
            UINT8_C(0x07), UINT8_C(0xff), UINT8_C(0x9c),
            UINT8_C(0x00), UINT8_C(0x64), UINT8_C(0x00), UINT8_C(0xc8)
        };
        LinkObd2DecodedPid decoded;
        LinkStructuredTelemetrySample structured_sample;

        CHECK(link_obd2_decode_pid_payload(
            UINT8_C(0x01), UINT8_C(0x7a),
            dpf_pressure_payload, sizeof(dpf_pressure_payload),
            &decoded) == LINK_OBD2_RESULT_OK);
        CHECK(decoded.signal_count == 3U);
        link_structured_telemetry_store_init(&structured_telemetry);
        CHECK(link_structured_telemetry_store_record(
            &structured_telemetry, 23U, true, UINT32_C(0x7e8),
            false, &decoded));
        CHECK(link_structured_telemetry_store_history_count(
            &structured_telemetry) == 1U);
        CHECK(link_structured_telemetry_store_total_sample_count(
            &structured_telemetry) == 1U);
        CHECK(link_structured_telemetry_store_history_at(
            &structured_telemetry, 0U, &structured_sample));
        CHECK(structured_sample.responder_id_available);
        CHECK(structured_sample.responder_id == UINT32_C(0x7e8));
        CHECK(!structured_sample.extended_id);
        CHECK(structured_sample.decoded.definition != NULL);
        CHECK(structured_sample.decoded.definition->pid == UINT8_C(0x7a));
        CHECK(structured_sample.decoded.raw_length ==
              sizeof(dpf_pressure_payload));
        CHECK(memcmp(
            structured_sample.decoded.raw, dpf_pressure_payload,
            sizeof(dpf_pressure_payload)) == 0);
        link_structured_telemetry_store_clear(&structured_telemetry);
        CHECK(link_structured_telemetry_store_history_count(
            &structured_telemetry) == 0U);
        CHECK(!link_structured_telemetry_store_record(
            &structured_telemetry, 24U, true, UINT32_C(0x800),
            false, &decoded));
    }
    link_telemetry_session_metadata_init(&metadata, 1U, "adapter", "vehicle");
    link_telemetry_session_metadata_finish(&metadata, 2U);
    CHECK(link_telemetry_export_csv_named(&telemetry, &metadata, "link", pid_name, unit_name, result_name, sink, &output));
    CHECK(strstr(output.data, "# link_csv_version,1\n") != NULL);
    {
        char expected_link_version[64];
        (void)snprintf(expected_link_version, sizeof(expected_link_version),
                       "# link_version,\"%s\"\n", LINK_VERSION_STRING);
        CHECK(strstr(output.data, expected_link_version) != NULL);
        CHECK(occurrence_count(output.data, "# link_version,") == 1U);
    }
    CHECK(strstr(output.data, "\"Engine speed\"") != NULL);

    memset(&output, 0, sizeof(output));
    link_telemetry_recorder_init(&recorder);
    CHECK(link_telemetry_recorder_begin(&recorder, &metadata, "link", sink, &output));
    CHECK(link_telemetry_recorder_record_sample_named(&recorder, &sample, true, "Engine speed", "rpm"));
    {
        LinkResponderTelemetrySample responder_sample = {
            .sequence = 2U,
            .timestamp_ms = 21U,
            .responder_id = 0x7e9U,
            .extended_id = false,
            .measurement = measurement
        };
        CHECK(link_telemetry_recorder_record_responder_sample_named(
            &recorder, &responder_sample, true, "Engine speed", "rpm"));
    }

    {
        static const uint8_t dpf_pressure_payload[] = {
            UINT8_C(0x07), UINT8_C(0xff), UINT8_C(0x9c),
            UINT8_C(0x00), UINT8_C(0x64), UINT8_C(0x00), UINT8_C(0xc8)
        };
        LinkObd2DecodedPid decoded;
        LinkStructuredTelemetrySample structured_sample = {
            .sequence = 3U,
            .timestamp_ms = 23U,
            .responder_id_available = true,
            .responder_id = UINT32_C(0x7e8),
            .extended_id = false
        };
        CHECK(link_obd2_decode_pid_payload(
            UINT8_C(0x01), UINT8_C(0x7a),
            dpf_pressure_payload, sizeof(dpf_pressure_payload),
            &decoded) == LINK_OBD2_RESULT_OK);
        structured_sample.decoded = decoded;
        CHECK(link_telemetry_recorder_record_structured_pid_named(
            &recorder, &structured_sample, false, "DPF pressure"));
    }
    CHECK(link_telemetry_recorder_record_response_named(&recorder, 30U, "010C", "ok", "41 0C 13 4A"));
    CHECK(link_telemetry_recorder_finish(&recorder, 3U));
    CHECK(strstr(output.data, "# link_session_stream_version,2\n") != NULL);
    CHECK(strstr(output.data, ",0x7E9,0,\"\",\"\",\"\"\n") != NULL);
    CHECK(strstr(output.data, "structured,3,23,0x7A,") != NULL);
    CHECK(strstr(output.data, "\"DPF pressure · ") != NULL);
    CHECK(strstr(output.data, "\"RAW 07 FF 9C 00 64 00 C8\"") != NULL);
    link_telemetry_recorder_init(&recorder);
    link_telemetry_session_metadata_init(&metadata, 4U, "adapter", "vehicle");
    CHECK(link_telemetry_recorder_continue(&recorder, &metadata, "link", sink, &output));
    CHECK(link_telemetry_recorder_record_response_named(&recorder, 5U, "ATI", "ok", "ELM327"));
    CHECK(link_telemetry_recorder_finish(&recorder, 6U));
    CHECK(occurrence_count(output.data, "# link_session_stream_version,2\n") == 1U);
    CHECK(occurrence_count(output.data, "# session_started_epoch_ms,") == 2U);
    CHECK(occurrence_count(output.data, "record_type,sequence,timestamp_ms,pid,name,value,unit,favourite,responder_can_id,responder_extended,command,result,response\n") == 1U);
    return 0;
}
