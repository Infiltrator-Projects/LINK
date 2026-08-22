// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/telemetry.h"

#include "infiltratr/core.h"
#include "infiltratr/format.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

void link_telemetry_store_init(LinkTelemetryStore *store) { if (store != NULL) memset(store, 0, sizeof(*store)); }

void link_telemetry_store_clear_samples(LinkTelemetryStore *store)
{
    if (store == NULL) return;
    memset(store->history, 0, sizeof(store->history));
    memset(store->latest, 0, sizeof(store->latest));
    memset(store->latest_valid, 0, sizeof(store->latest_valid));
    memset(store->transcript, 0, sizeof(store->transcript));
    store->transcript_head = 0U; store->transcript_count = 0U; store->history_head = 0U; store->history_count = 0U; store->next_sequence = 0U; store->total_sample_count = 0U;
}

bool link_telemetry_store_record(LinkTelemetryStore *store, uint64_t timestamp_ms, const LinkTelemetryMeasurement *measurement)
{
    LinkTelemetrySample sample;
    if (store == NULL || measurement == NULL || !isfinite(measurement->value)) return false;
    sample.sequence = store->next_sequence; sample.timestamp_ms = timestamp_ms; sample.measurement = *measurement;
    if (store->next_sequence != UINT64_MAX) store->next_sequence++;
    store->total_sample_count = infiltratr_u64_add_saturating(store->total_sample_count, 1U);
    store->latest[measurement->pid] = sample; store->latest_valid[measurement->pid] = true;
    store->history[store->history_head] = sample;
    store->history_head = (store->history_head + 1U) % LINK_TELEMETRY_HISTORY_CAPACITY;
    if (store->history_count < LINK_TELEMETRY_HISTORY_CAPACITY) store->history_count++;
    return true;
}

bool link_telemetry_store_latest(const LinkTelemetryStore *store, uint8_t pid, LinkTelemetrySample *sample)
{
    if (store == NULL || sample == NULL || !store->latest_valid[pid]) return false;
    *sample = store->latest[pid]; return true;
}

size_t link_telemetry_store_history_count(const LinkTelemetryStore *store) { return store != NULL ? store->history_count : 0U; }
uint64_t link_telemetry_store_total_sample_count(const LinkTelemetryStore *store) { return store != NULL ? store->total_sample_count : 0U; }

bool link_telemetry_store_history_at(const LinkTelemetryStore *store, size_t chronological_index, LinkTelemetrySample *sample)
{
    size_t oldest, storage_index;
    if (store == NULL || sample == NULL || chronological_index >= store->history_count) return false;
    oldest = (store->history_head + LINK_TELEMETRY_HISTORY_CAPACITY - store->history_count) % LINK_TELEMETRY_HISTORY_CAPACITY;
    storage_index = (oldest + chronological_index) % LINK_TELEMETRY_HISTORY_CAPACITY;
    *sample = store->history[storage_index]; return true;
}

void link_telemetry_store_set_favourite(LinkTelemetryStore *store, uint8_t pid, bool favourite) { if (store != NULL) store->favourite[pid] = favourite; }
bool link_telemetry_store_is_favourite(const LinkTelemetryStore *store, uint8_t pid) { return store != NULL && store->favourite[pid]; }

bool link_telemetry_store_record_transcript(LinkTelemetryStore *store, uint64_t timestamp_ms, const char *command, uint32_t result, const char *response_text)
{
    LinkTelemetryTranscriptEntry *entry;
    if (store == NULL || command == NULL || response_text == NULL) return false;
    entry = &store->transcript[store->transcript_head];
    memset(entry, 0, sizeof(*entry)); entry->timestamp_ms = timestamp_ms; entry->result = result;
    infiltratr_copy_string(entry->command, sizeof(entry->command), command);
    infiltratr_copy_string(entry->response, sizeof(entry->response), response_text);
    store->transcript_head = (store->transcript_head + 1U) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    if (store->transcript_count < LINK_TELEMETRY_TRANSCRIPT_CAPACITY) store->transcript_count++;
    return true;
}

size_t link_telemetry_store_transcript_count(const LinkTelemetryStore *store) { return store != NULL ? store->transcript_count : 0U; }
bool link_telemetry_store_transcript_at(const LinkTelemetryStore *store, size_t chronological_index, LinkTelemetryTranscriptEntry *entry)
{
    size_t oldest, storage_index;
    if (store == NULL || entry == NULL || chronological_index >= store->transcript_count) return false;
    oldest = (store->transcript_head + LINK_TELEMETRY_TRANSCRIPT_CAPACITY - store->transcript_count) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    storage_index = (oldest + chronological_index) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    *entry = store->transcript[storage_index]; return true;
}

void link_telemetry_session_metadata_init(LinkTelemetrySessionMetadata *metadata, uint64_t started_epoch_ms, const char *adapter_identifier, const char *vehicle_identifier)
{
    if (metadata == NULL) return;
    memset(metadata, 0, sizeof(*metadata)); metadata->started_epoch_ms = started_epoch_ms;
    infiltratr_copy_string(metadata->adapter_identifier, sizeof(metadata->adapter_identifier), adapter_identifier);
    infiltratr_copy_string(metadata->vehicle_identifier, sizeof(metadata->vehicle_identifier), vehicle_identifier);
}
void link_telemetry_session_metadata_set_adapter(LinkTelemetrySessionMetadata *metadata, const char *adapter_identifier) { if (metadata != NULL) infiltratr_copy_string(metadata->adapter_identifier, sizeof(metadata->adapter_identifier), adapter_identifier); }
void link_telemetry_session_metadata_set_vehicle(LinkTelemetrySessionMetadata *metadata, const char *vehicle_identifier) { if (metadata != NULL) infiltratr_copy_string(metadata->vehicle_identifier, sizeof(metadata->vehicle_identifier), vehicle_identifier); }
void link_telemetry_session_metadata_finish(LinkTelemetrySessionMetadata *metadata, uint64_t ended_epoch_ms) { if (metadata != NULL) metadata->ended_epoch_ms = ended_epoch_ms; }

static bool emit(LinkTelemetryTextSink sink, void *context, const char *text) { return sink != NULL && text != NULL && sink(context, text, strlen(text)); }
static bool emit_quoted(LinkTelemetryTextSink sink, void *context, const char *text)
{
    const char *cursor = text != NULL ? text : "", *segment = cursor;
    if (!emit(sink, context, "\"")) return false;
    while (*cursor != '\0') {
        if (*cursor == '"') {
            if (cursor > segment && !sink(context, segment, (size_t)(cursor - segment))) return false;
            if (!emit(sink, context, "\"\"")) return false;
            segment = cursor + 1;
        }
        ++cursor;
    }
    if (cursor > segment && !sink(context, segment, (size_t)(cursor - segment))) return false;
    return emit(sink, context, "\"");
}
static bool emit_metadata(LinkTelemetryTextSink sink, void *context, const char *key, const char *value)
{
    return emit(sink, context, "# ") && emit(sink, context, key) && emit(sink, context, ",") && emit_quoted(sink, context, value) && emit(sink, context, "\n");
}
static bool format_value(double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options = INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    char *cursor, *end;
    options.decimal_places = 6U; options.unavailable_text = "";
    if (!infiltratr_format_scalar(true, (long double)value, &options, buffer, size)) return false;
    for (cursor = buffer; *cursor != '\0'; ++cursor) if (*cursor == ',') *cursor = '.';
    end = buffer + strlen(buffer); while (end > buffer && end[-1] == '0') --end; if (end > buffer && end[-1] == '.') --end; *end = '\0';
    return true;
}
static bool latch_failure(LinkTelemetryRecorder *recorder) { if (recorder != NULL) recorder->failed = true; return false; }

void link_telemetry_recorder_init(LinkTelemetryRecorder *recorder) { if (recorder != NULL) memset(recorder, 0, sizeof(*recorder)); }

bool link_telemetry_recorder_begin(LinkTelemetryRecorder *recorder, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryTextSink sink, void *context)
{
    char line[192]; int written;
    if (recorder == NULL || metadata == NULL || product_slug == NULL || product_slug[0] == '\0' || sink == NULL || recorder->started || recorder->failed) return false;
    recorder->sink = sink; recorder->context = context; recorder->started = true; recorder->finished = false; recorder->failed = false;
    written = snprintf(line, sizeof(line), "# %s_session_stream_version,1\n# session_started_epoch_ms,%llu\n", product_slug, (unsigned long long)metadata->started_epoch_ms);
    if (written < 0 || (size_t)written >= sizeof(line) || !emit(sink, context, line) || !emit_metadata(sink, context, "adapter_identifier", metadata->adapter_identifier) || !emit_metadata(sink, context, "vehicle_identifier", metadata->vehicle_identifier) || !emit(sink, context, "record_type,sequence,timestamp_ms,pid,name,value,unit,favourite,command,result,response\n")) return latch_failure(recorder);
    return true;
}

bool link_telemetry_recorder_record_sample_named(LinkTelemetryRecorder *recorder, const LinkTelemetrySample *sample, bool favourite, const char *pid_name, const char *unit_name)
{
    char prefix[64], row[256], value[64]; int written;
    if (recorder == NULL || !recorder->started || recorder->finished || recorder->failed || sample == NULL || pid_name == NULL || unit_name == NULL || !isfinite(sample->measurement.value)) return false;
    if (!format_value(sample->measurement.value, value, sizeof(value))) return false;
    written = snprintf(prefix, sizeof(prefix), "sample,%llu,", (unsigned long long)sample->sequence); if (written < 0 || (size_t)written >= sizeof(prefix)) return false;
    written = snprintf(row, sizeof(row), "%llu,0x%02X,", (unsigned long long)sample->timestamp_ms, (unsigned int)sample->measurement.pid); if (written < 0 || (size_t)written >= sizeof(row)) return false;
    if (!emit(recorder->sink, recorder->context, prefix) || !emit(recorder->sink, recorder->context, row) || !emit_quoted(recorder->sink, recorder->context, pid_name) || !emit(recorder->sink, recorder->context, ",") || !emit(recorder->sink, recorder->context, value) || !emit(recorder->sink, recorder->context, ",") || !emit_quoted(recorder->sink, recorder->context, unit_name)) return latch_failure(recorder);
    written = snprintf(row, sizeof(row), ",%u,\"\",\"\",\"\"\n", favourite ? 1U : 0U); if (written < 0 || (size_t)written >= sizeof(row)) return false;
    return emit(recorder->sink, recorder->context, row) ? true : latch_failure(recorder);
}

bool link_telemetry_recorder_record_response_named(LinkTelemetryRecorder *recorder, uint64_t timestamp_ms, const char *command, const char *result_name, const char *response_text)
{
    char line[96]; int written;
    if (recorder == NULL || !recorder->started || recorder->finished || recorder->failed || recorder->sink == NULL || command == NULL || result_name == NULL || response_text == NULL) return false;
    written = snprintf(line, sizeof(line), "transcript,,%llu,,,,,,", (unsigned long long)timestamp_ms); if (written < 0 || (size_t)written >= sizeof(line)) return false;
    if (!emit(recorder->sink, recorder->context, line) || !emit_quoted(recorder->sink, recorder->context, command) || !emit(recorder->sink, recorder->context, ",") || !emit_quoted(recorder->sink, recorder->context, result_name) || !emit(recorder->sink, recorder->context, ",") || !emit_quoted(recorder->sink, recorder->context, response_text) || !emit(recorder->sink, recorder->context, "\n")) return latch_failure(recorder);
    return true;
}

bool link_telemetry_recorder_finish(LinkTelemetryRecorder *recorder, uint64_t ended_epoch_ms)
{
    char line[96]; int written;
    if (recorder == NULL || !recorder->started || recorder->finished || recorder->failed) return false;
    written = snprintf(line, sizeof(line), "# session_ended_epoch_ms,%llu\n", (unsigned long long)ended_epoch_ms); if (written < 0 || (size_t)written >= sizeof(line)) return false;
    if (!emit(recorder->sink, recorder->context, line)) return latch_failure(recorder);
    recorder->finished = true; return true;
}

bool link_telemetry_export_csv_named(const LinkTelemetryStore *store, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryPidName pid_name, LinkTelemetryUnitName unit_name, LinkTelemetryResultName result_name, LinkTelemetryTextSink sink, void *context)
{
    char line[256], value[64]; int written; size_t index;
    if (store == NULL || metadata == NULL || product_slug == NULL || product_slug[0] == '\0' || pid_name == NULL || unit_name == NULL || result_name == NULL || sink == NULL) return false;
    written = snprintf(line, sizeof(line), "# %s_csv_version,1\n# session_started_epoch_ms,%llu\n# session_ended_epoch_ms,%llu\n", product_slug, (unsigned long long)metadata->started_epoch_ms, (unsigned long long)metadata->ended_epoch_ms);
    if (written < 0 || (size_t)written >= sizeof(line) || !emit(sink, context, line) || !emit_metadata(sink, context, "adapter_identifier", metadata->adapter_identifier) || !emit_metadata(sink, context, "vehicle_identifier", metadata->vehicle_identifier) || !emit(sink, context, "sequence,timestamp_ms,pid,name,value,unit,favourite\n")) return false;
    for (index = 0U; index < store->history_count; ++index) {
        LinkTelemetrySample sample;
        if (!link_telemetry_store_history_at(store, index, &sample) || !format_value(sample.measurement.value, value, sizeof(value))) return false;
        written = snprintf(line, sizeof(line), "%llu,%llu,0x%02X,", (unsigned long long)sample.sequence, (unsigned long long)sample.timestamp_ms, (unsigned int)sample.measurement.pid);
        if (written < 0 || (size_t)written >= sizeof(line) || !emit(sink, context, line) || !emit_quoted(sink, context, pid_name(sample.measurement.pid)) || !emit(sink, context, ",") || !emit(sink, context, value) || !emit(sink, context, ",") || !emit_quoted(sink, context, unit_name((uint32_t)sample.measurement.unit))) return false;
        written = snprintf(line, sizeof(line), ",%u\n", store->favourite[sample.measurement.pid] ? 1U : 0U); if (written < 0 || (size_t)written >= sizeof(line) || !emit(sink, context, line)) return false;
    }
    if (!emit(sink, context, "# diagnostic_transcript\n") || !emit(sink, context, "timestamp_ms,command,result,response\n")) return false;
    for (index = 0U; index < store->transcript_count; ++index) {
        LinkTelemetryTranscriptEntry entry;
        if (!link_telemetry_store_transcript_at(store, index, &entry)) return false;
        written = snprintf(line, sizeof(line), "%llu,", (unsigned long long)entry.timestamp_ms);
        if (written < 0 || (size_t)written >= sizeof(line) || !emit(sink, context, line) || !emit_quoted(sink, context, entry.command) || !emit(sink, context, ",") || !emit_quoted(sink, context, result_name(entry.result)) || !emit(sink, context, ",") || !emit_quoted(sink, context, entry.response) || !emit(sink, context, "\n")) return false;
    }
    return true;
}
