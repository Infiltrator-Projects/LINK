// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/parameter.h"
#include "link/parameter_store.h"
#include "link/scheduler.h"
#include "link/telemetry.h"

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

int main(void)
{
    LinkParameterSample parameter;
    LinkParameterStore parameter_store;
    LinkScheduler scheduler;
    LinkSchedulerDispatch dispatch;
    uint8_t bits[LINK_OBD2_PID_SET_BYTES] = {0};
    LinkTelemetryStore telemetry;
    LinkTelemetryMeasurement measurement = { 0x0cU, 1234.5, LINK_OBD2_UNIT_RPM };
    LinkTelemetrySample sample;
    LinkTelemetrySessionMetadata metadata;
    LinkTelemetryRecorder recorder;
    Buffer output = {{0}, 0U};

    CHECK(link_parameter_obd2_definition_count() == 20U);
    CHECK(link_parameter_from_obd2_scalar(0x0cU, LINK_OBD2_UNIT_RPM, 1234.5, 10U, &parameter));
    CHECK(strcmp(parameter.definition->stable_key, "obd2.engine.rpm") == 0);
    link_parameter_store_init(&parameter_store);
    CHECK(link_parameter_store_register(&parameter_store, parameter.definition) == LINK_PARAMETER_STORE_OK);
    CHECK(link_parameter_store_record(&parameter_store, &parameter) == LINK_PARAMETER_STORE_OK);
    CHECK(link_parameter_store_total_sample_count(&parameter_store) == 1U);

    link_scheduler_init(&scheduler);
    CHECK(link_scheduler_add(&scheduler, 0x0cU, 250U, LINK_SCHEDULER_PRIORITY_CRITICAL, 100U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(link_scheduler_next(&scheduler, 100U, &dispatch) == LINK_SCHEDULER_NEXT_READY);
    CHECK(dispatch.pid == 0x0cU);
    bits[0x0cU / 8U] |= (uint8_t)(1U << (0x0cU % 8U));
    CHECK(link_scheduler_configure_standard_obd2_bits(&scheduler, bits, 0U) == LINK_SCHEDULER_RESULT_OK);
    CHECK(scheduler.count == 1U && scheduler.items[0].pid == 0x0cU);

    link_telemetry_store_init(&telemetry);
    CHECK(link_telemetry_store_record(&telemetry, 20U, &measurement));
    CHECK(link_telemetry_store_latest(&telemetry, 0x0cU, &sample));
    CHECK(sample.measurement.value == 1234.5);
    CHECK(link_telemetry_store_record_transcript(&telemetry, 30U, "010C", 0U, "41 0C 13 4A"));
    link_telemetry_session_metadata_init(&metadata, 1U, "adapter", "vehicle");
    link_telemetry_session_metadata_finish(&metadata, 2U);
    CHECK(link_telemetry_export_csv_named(&telemetry, &metadata, "link", pid_name, unit_name, result_name, sink, &output));
    CHECK(strstr(output.data, "# link_csv_version,1\n") != NULL);
    CHECK(strstr(output.data, "\"Engine speed\"") != NULL);

    memset(&output, 0, sizeof(output));
    link_telemetry_recorder_init(&recorder);
    CHECK(link_telemetry_recorder_begin(&recorder, &metadata, "link", sink, &output));
    CHECK(link_telemetry_recorder_record_sample_named(&recorder, &sample, true, "Engine speed", "rpm"));
    CHECK(link_telemetry_recorder_record_response_named(&recorder, 30U, "010C", "ok", "41 0C 13 4A"));
    CHECK(link_telemetry_recorder_finish(&recorder, 3U));
    CHECK(strstr(output.data, "# link_session_stream_version,1\n") != NULL);
    return 0;
}
