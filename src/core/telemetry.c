// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/telemetry.h"
#include "link/version.h"

#include "infiltratr/core.h"
#include "infiltratr/format.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/*
 * Product faces compile the shared telemetry implementation either through
 * LINK::Core or directly into their Apple target.  Normalize the product
 * build identity here so MBLINK and JAGLINK evidence receive the same fields.
 */
#if defined(MBLINK_VERSION)
#define LINK_TELEMETRY_PRODUCT_VERSION MBLINK_VERSION
#elif defined(JAGLINK_VERSION)
#define LINK_TELEMETRY_PRODUCT_VERSION JAGLINK_VERSION
#endif

#if defined(MBLINK_BUILD_PROFILE)
#define LINK_TELEMETRY_PRODUCT_BUILD_PROFILE MBLINK_BUILD_PROFILE
#elif defined(JAGLINK_BUILD_PROFILE)
#define LINK_TELEMETRY_PRODUCT_BUILD_PROFILE JAGLINK_BUILD_PROFILE
#endif

#if defined(MBLINK_BUILD_REVISION)
#define LINK_TELEMETRY_PRODUCT_BUILD_REVISION MBLINK_BUILD_REVISION
#elif defined(JAGLINK_BUILD_REVISION)
#define LINK_TELEMETRY_PRODUCT_BUILD_REVISION JAGLINK_BUILD_REVISION
#endif

#ifndef LINK_TELEMETRY_BUILD_ID
#define LINK_TELEMETRY_BUILD_ID __DATE__ " " __TIME__
#endif

void link_telemetry_store_init(LinkTelemetryStore *store)
{
    if (store != NULL) memset(store, 0, sizeof(*store));
}

void link_telemetry_store_clear_samples(LinkTelemetryStore *store)
{
    if (store == NULL) return;
    memset(store->history, 0, sizeof(store->history));
    memset(store->latest, 0, sizeof(store->latest));
    memset(store->latest_valid, 0, sizeof(store->latest_valid));
    memset(store->transcript, 0, sizeof(store->transcript));
    store->transcript_head = 0U;
    store->transcript_count = 0U;
    store->history_head = 0U;
    store->history_count = 0U;
    store->next_sequence = 0U;
    store->total_sample_count = 0U;
}

bool link_telemetry_store_record(LinkTelemetryStore *store,
                                 uint64_t timestamp_ms,
                                 const LinkTelemetryMeasurement *measurement)
{
    LinkTelemetrySample sample;
    if (store == NULL || measurement == NULL || !isfinite(measurement->value))
        return false;
    sample.sequence = store->next_sequence;
    sample.timestamp_ms = timestamp_ms;
    sample.measurement = *measurement;
    if (store->next_sequence != UINT64_MAX) store->next_sequence++;
    store->total_sample_count = infiltratr_u64_add_saturating(
        store->total_sample_count, 1U);
    store->latest[measurement->pid] = sample;
    store->latest_valid[measurement->pid] = true;
    store->history[store->history_head] = sample;
    store->history_head =
        (store->history_head + 1U) % LINK_TELEMETRY_HISTORY_CAPACITY;
    if (store->history_count < LINK_TELEMETRY_HISTORY_CAPACITY)
        store->history_count++;
    return true;
}

bool link_telemetry_store_latest(const LinkTelemetryStore *store,
                                 uint8_t pid,
                                 LinkTelemetrySample *sample)
{
    if (store == NULL || sample == NULL || !store->latest_valid[pid])
        return false;
    *sample = store->latest[pid];
    return true;
}

size_t link_telemetry_store_history_count(const LinkTelemetryStore *store)
{
    return store != NULL ? store->history_count : 0U;
}

uint64_t link_telemetry_store_total_sample_count(const LinkTelemetryStore *store)
{
    return store != NULL ? store->total_sample_count : 0U;
}

bool link_telemetry_store_history_at(const LinkTelemetryStore *store,
                                     size_t chronological_index,
                                     LinkTelemetrySample *sample)
{
    size_t oldest;
    size_t storage_index;
    if (store == NULL || sample == NULL ||
        chronological_index >= store->history_count)
        return false;
    oldest = (store->history_head + LINK_TELEMETRY_HISTORY_CAPACITY -
              store->history_count) % LINK_TELEMETRY_HISTORY_CAPACITY;
    storage_index =
        (oldest + chronological_index) % LINK_TELEMETRY_HISTORY_CAPACITY;
    *sample = store->history[storage_index];
    return true;
}

void link_telemetry_store_set_favourite(LinkTelemetryStore *store,
                                        uint8_t pid,
                                        bool favourite)
{
    if (store != NULL) store->favourite[pid] = favourite;
}

bool link_telemetry_store_is_favourite(const LinkTelemetryStore *store,
                                       uint8_t pid)
{
    return store != NULL && store->favourite[pid];
}

bool link_telemetry_store_record_transcript(LinkTelemetryStore *store,
                                            uint64_t timestamp_ms,
                                            const char *command,
                                            uint32_t result,
                                            const char *response_text)
{
    LinkTelemetryTranscriptEntry *entry;
    if (store == NULL || command == NULL || response_text == NULL) return false;
    entry = &store->transcript[store->transcript_head];
    memset(entry, 0, sizeof(*entry));
    entry->timestamp_ms = timestamp_ms;
    entry->result = result;
    infiltratr_copy_string(entry->command, sizeof(entry->command), command);
    infiltratr_copy_string(
        entry->response, sizeof(entry->response), response_text);
    store->transcript_head =
        (store->transcript_head + 1U) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    if (store->transcript_count < LINK_TELEMETRY_TRANSCRIPT_CAPACITY)
        store->transcript_count++;
    return true;
}

size_t link_telemetry_store_transcript_count(const LinkTelemetryStore *store)
{
    return store != NULL ? store->transcript_count : 0U;
}

bool link_telemetry_store_transcript_at(const LinkTelemetryStore *store,
                                        size_t chronological_index,
                                        LinkTelemetryTranscriptEntry *entry)
{
    size_t oldest;
    size_t storage_index;
    if (store == NULL || entry == NULL ||
        chronological_index >= store->transcript_count)
        return false;
    oldest = (store->transcript_head + LINK_TELEMETRY_TRANSCRIPT_CAPACITY -
              store->transcript_count) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    storage_index =
        (oldest + chronological_index) % LINK_TELEMETRY_TRANSCRIPT_CAPACITY;
    *entry = store->transcript[storage_index];
    return true;
}

void link_responder_telemetry_store_init(LinkResponderTelemetryStore *store)
{
    if (store != NULL) memset(store, 0, sizeof(*store));
}

void link_responder_telemetry_store_clear(LinkResponderTelemetryStore *store)
{
    if (store != NULL) memset(store, 0, sizeof(*store));
}

bool link_responder_telemetry_store_record(
    LinkResponderTelemetryStore *store,
    uint64_t timestamp_ms,
    uint32_t responder_id,
    bool extended_id,
    const LinkTelemetryMeasurement *measurement)
{
    LinkResponderTelemetrySample sample;
    const uint32_t maximum_id = extended_id
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (store == NULL || measurement == NULL ||
        responder_id > maximum_id || !isfinite(measurement->value)) {
        return false;
    }

    sample.sequence = store->next_sequence;
    sample.timestamp_ms = timestamp_ms;
    sample.responder_id = responder_id;
    sample.extended_id = extended_id;
    sample.measurement = *measurement;
    if (store->next_sequence != UINT64_MAX) store->next_sequence++;
    store->total_sample_count = infiltratr_u64_add_saturating(
        store->total_sample_count, 1U);
    store->history[store->history_head] = sample;
    store->history_head =
        (store->history_head + 1U) %
        LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY;
    if (store->history_count < LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY)
        store->history_count++;
    return true;
}

size_t link_responder_telemetry_store_history_count(
    const LinkResponderTelemetryStore *store)
{
    return store != NULL ? store->history_count : 0U;
}

uint64_t link_responder_telemetry_store_total_sample_count(
    const LinkResponderTelemetryStore *store)
{
    return store != NULL ? store->total_sample_count : 0U;
}

bool link_responder_telemetry_store_history_at(
    const LinkResponderTelemetryStore *store,
    size_t chronological_index,
    LinkResponderTelemetrySample *sample)
{
    size_t oldest;
    size_t storage_index;
    if (store == NULL || sample == NULL ||
        chronological_index >= store->history_count) {
        return false;
    }
    oldest =
        (store->history_head + LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY -
         store->history_count) % LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY;
    storage_index =
        (oldest + chronological_index) %
        LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY;
    *sample = store->history[storage_index];
    return true;
}


void link_structured_telemetry_store_init(LinkStructuredTelemetryStore *store)
{
    if (store != NULL) memset(store, 0, sizeof(*store));
}

void link_structured_telemetry_store_clear(LinkStructuredTelemetryStore *store)
{
    if (store != NULL) memset(store, 0, sizeof(*store));
}

bool link_structured_telemetry_store_record(
    LinkStructuredTelemetryStore *store,
    uint64_t timestamp_ms,
    bool responder_id_available,
    uint32_t responder_id,
    bool extended_id,
    const LinkObd2DecodedPid *decoded)
{
    LinkStructuredTelemetrySample sample;
    const uint32_t maximum_id = extended_id
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    size_t index;

    if (store == NULL || decoded == NULL || decoded->definition == NULL ||
        decoded->raw_length > LINK_OBD2_MAX_PID_PAYLOAD ||
        decoded->signal_count > LINK_OBD2_MAX_DECODED_SIGNALS ||
        (responder_id_available && responder_id > maximum_id)) {
        return false;
    }
    for (index = 0U; index < decoded->signal_count; ++index) {
        if (!isfinite(decoded->signals[index].value)) return false;
    }

    memset(&sample, 0, sizeof(sample));
    sample.sequence = store->next_sequence;
    sample.timestamp_ms = timestamp_ms;
    sample.responder_id_available = responder_id_available;
    sample.responder_id = responder_id;
    sample.extended_id = extended_id;
    sample.decoded = *decoded;

    if (store->next_sequence != UINT64_MAX) store->next_sequence++;
    store->total_sample_count = infiltratr_u64_add_saturating(
        store->total_sample_count, 1U);
    store->history[store->history_head] = sample;
    store->history_head =
        (store->history_head + 1U) %
        LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY;
    if (store->history_count < LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY)
        store->history_count++;
    return true;
}

size_t link_structured_telemetry_store_history_count(
    const LinkStructuredTelemetryStore *store)
{
    return store != NULL ? store->history_count : 0U;
}

uint64_t link_structured_telemetry_store_total_sample_count(
    const LinkStructuredTelemetryStore *store)
{
    return store != NULL ? store->total_sample_count : 0U;
}

bool link_structured_telemetry_store_history_at(
    const LinkStructuredTelemetryStore *store,
    size_t chronological_index,
    LinkStructuredTelemetrySample *sample)
{
    size_t oldest;
    size_t storage_index;
    if (store == NULL || sample == NULL ||
        chronological_index >= store->history_count) {
        return false;
    }
    oldest =
        (store->history_head + LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY -
         store->history_count) % LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY;
    storage_index =
        (oldest + chronological_index) %
        LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY;
    *sample = store->history[storage_index];
    return true;
}

void link_telemetry_session_metadata_init(
    LinkTelemetrySessionMetadata *metadata,
    uint64_t started_epoch_ms,
    const char *adapter_identifier,
    const char *vehicle_identifier)
{
    if (metadata == NULL) return;
    memset(metadata, 0, sizeof(*metadata));
    metadata->started_epoch_ms = started_epoch_ms;
    infiltratr_copy_string(metadata->adapter_identifier,
                           sizeof(metadata->adapter_identifier),
                           adapter_identifier);
    infiltratr_copy_string(metadata->vehicle_identifier,
                           sizeof(metadata->vehicle_identifier),
                           vehicle_identifier);
}

void link_telemetry_session_metadata_set_adapter(
    LinkTelemetrySessionMetadata *metadata,
    const char *adapter_identifier)
{
    if (metadata != NULL)
        infiltratr_copy_string(metadata->adapter_identifier,
                               sizeof(metadata->adapter_identifier),
                               adapter_identifier);
}

void link_telemetry_session_metadata_set_vehicle(
    LinkTelemetrySessionMetadata *metadata,
    const char *vehicle_identifier)
{
    if (metadata != NULL)
        infiltratr_copy_string(metadata->vehicle_identifier,
                               sizeof(metadata->vehicle_identifier),
                               vehicle_identifier);
}

void link_telemetry_session_metadata_finish(LinkTelemetrySessionMetadata *metadata,
                                            uint64_t ended_epoch_ms)
{
    if (metadata != NULL) metadata->ended_epoch_ms = ended_epoch_ms;
}

static bool emit(LinkTelemetryTextSink sink, void *context, const char *text)
{
    return sink != NULL && text != NULL && sink(context, text, strlen(text));
}

static bool emit_quoted(LinkTelemetryTextSink sink,
                        void *context,
                        const char *text)
{
    const char *cursor = text != NULL ? text : "";
    const char *segment = cursor;
    if (!emit(sink, context, "\"")) return false;
    while (*cursor != '\0') {
        if (*cursor == '"') {
            if (cursor > segment &&
                !sink(context, segment, (size_t)(cursor - segment)))
                return false;
            if (!emit(sink, context, "\"\"")) return false;
            segment = cursor + 1;
        }
        ++cursor;
    }
    if (cursor > segment &&
        !sink(context, segment, (size_t)(cursor - segment)))
        return false;
    return emit(sink, context, "\"");
}

static bool emit_metadata(LinkTelemetryTextSink sink,
                          void *context,
                          const char *key,
                          const char *value)
{
    return emit(sink, context, "# ") &&
           emit(sink, context, key) &&
           emit(sink, context, ",") &&
           emit_quoted(sink, context, value) &&
           emit(sink, context, "\n");
}

static bool emit_build_identity(LinkTelemetryTextSink sink,
                                void *context,
                                const char *product_slug)
{
    char key[96];
    int written;

    if (!emit_metadata(
            sink, context, "link_version", LINK_VERSION_STRING) ||
        !emit_metadata(sink, context, "build_id", LINK_TELEMETRY_BUILD_ID))
        return false;
#ifdef LINK_SOURCE_REVISION
    if (!emit_metadata(
            sink, context, "link_revision", LINK_SOURCE_REVISION))
        return false;
#endif

#ifdef LINK_TELEMETRY_PRODUCT_VERSION
    written = snprintf(key, sizeof(key), "%s_version", product_slug);
    if (written < 0 || (size_t)written >= sizeof(key) ||
        !emit_metadata(sink, context, key, LINK_TELEMETRY_PRODUCT_VERSION))
        return false;
#endif
#ifdef LINK_TELEMETRY_PRODUCT_BUILD_PROFILE
    written = snprintf(key, sizeof(key), "%s_build_profile", product_slug);
    if (written < 0 || (size_t)written >= sizeof(key) ||
        !emit_metadata(
            sink, context, key, LINK_TELEMETRY_PRODUCT_BUILD_PROFILE))
        return false;
#endif
#ifdef LINK_TELEMETRY_PRODUCT_BUILD_REVISION
    written = snprintf(key, sizeof(key), "%s_build_revision", product_slug);
    if (written < 0 || (size_t)written >= sizeof(key) ||
        !emit_metadata(
            sink, context, key, LINK_TELEMETRY_PRODUCT_BUILD_REVISION))
        return false;
#endif
    /*
     * Standalone LINK builds do not define a product-face identity, so keep
     * the shared helper warning-clean there too.
     */
    (void)product_slug;
    (void)key;
    (void)written;
    return true;
}

static bool format_value(double value, char *buffer, size_t size)
{
    InfiltratrScalarFormatOptions options =
        INFILTRATR_SCALAR_FORMAT_OPTIONS_INIT;
    char *cursor;
    char *end;
    options.decimal_places = 6U;
    options.unavailable_text = "";
    if (!infiltratr_format_scalar(
            true, (long double)value, &options, buffer, size))
        return false;
    for (cursor = buffer; *cursor != '\0'; ++cursor)
        if (*cursor == ',') *cursor = '.';
    end = buffer + strlen(buffer);
    while (end > buffer && end[-1] == '0') --end;
    if (end > buffer && end[-1] == '.') --end;
    *end = '\0';
    return true;
}


static bool format_structured_raw(
    const LinkObd2DecodedPid *decoded,
    char *buffer,
    size_t capacity)
{
    static const char digits[] = "0123456789ABCDEF";
    size_t index;
    size_t used = 0U;
    if (decoded == NULL || buffer == NULL || capacity == 0U)
        return false;
    buffer[0] = '\0';
    if (decoded->text_available && decoded->text[0] != '\0') {
        return infiltratr_copy_string(buffer, capacity, decoded->text);
    }
    if (decoded->raw_length == 0U)
        return infiltratr_copy_string(buffer, capacity, "RAW");
    if (capacity < 5U) return false;
    memcpy(buffer, "RAW ", 4U);
    used = 4U;
    for (index = 0U; index < decoded->raw_length; ++index) {
        if (used + (index == 0U ? 2U : 3U) + 1U > capacity)
            return false;
        if (index != 0U) buffer[used++] = ' ';
        buffer[used++] = digits[(decoded->raw[index] >> 4U) & 0x0fU];
        buffer[used++] = digits[decoded->raw[index] & 0x0fU];
    }
    buffer[used] = '\0';
    return true;
}

static bool latch_failure(LinkTelemetryRecorder *recorder)
{
    if (recorder != NULL) recorder->failed = true;
    return false;
}

void link_telemetry_recorder_init(LinkTelemetryRecorder *recorder)
{
    if (recorder != NULL) memset(recorder, 0, sizeof(*recorder));
}

static bool recorder_begin_session(LinkTelemetryRecorder *recorder,
                                   const LinkTelemetrySessionMetadata *metadata,
                                   const char *product_slug,
                                   LinkTelemetryTextSink sink,
                                   void *context,
                                   bool write_stream_header)
{
    char line[192];
    int written;
    if (recorder == NULL || metadata == NULL || product_slug == NULL ||
        product_slug[0] == '\0' || sink == NULL || recorder->started ||
        recorder->failed)
        return false;
    recorder->sink = sink;
    recorder->context = context;
    recorder->started = true;
    recorder->finished = false;
    recorder->failed = false;
    if (write_stream_header) {
        written = snprintf(line, sizeof(line),
                           "# %s_session_stream_version,2\n", product_slug);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !emit(sink, context, line))
            return latch_failure(recorder);
    }
    written = snprintf(line, sizeof(line),
                       "# session_started_epoch_ms,%llu\n",
                       (unsigned long long)metadata->started_epoch_ms);
    if (written < 0 || (size_t)written >= sizeof(line) ||
        !emit(sink, context, line) ||
        !emit_build_identity(sink, context, product_slug) ||
        !emit_metadata(sink, context, "adapter_identifier",
                       metadata->adapter_identifier) ||
        !emit_metadata(sink, context, "vehicle_identifier",
                       metadata->vehicle_identifier))
        return latch_failure(recorder);
    if (write_stream_header &&
        !emit(sink, context,
              "record_type,sequence,timestamp_ms,pid,name,value,unit,"
              "favourite,responder_can_id,responder_extended,"
              "command,result,response\n"))
        return latch_failure(recorder);
    return true;
}

bool link_telemetry_recorder_begin(LinkTelemetryRecorder *recorder,
                                   const LinkTelemetrySessionMetadata *metadata,
                                   const char *product_slug,
                                   LinkTelemetryTextSink sink,
                                   void *context)
{
    return recorder_begin_session(
        recorder, metadata, product_slug, sink, context, true);
}

bool link_telemetry_recorder_continue(
    LinkTelemetryRecorder *recorder,
    const LinkTelemetrySessionMetadata *metadata,
    const char *product_slug,
    LinkTelemetryTextSink sink,
    void *context)
{
    return recorder_begin_session(
        recorder, metadata, product_slug, sink, context, false);
}

static bool recorder_record_sample_named(
    LinkTelemetryRecorder *recorder,
    uint64_t sequence,
    uint64_t timestamp_ms,
    const LinkTelemetryMeasurement *measurement,
    bool favourite,
    bool responder_available,
    uint32_t responder_id,
    bool responder_extended,
    const char *pid_name,
    const char *unit_name)
{
    char prefix[64];
    char row[256];
    char value[64];
    int written;
    if (recorder == NULL || !recorder->started || recorder->finished ||
        recorder->failed || measurement == NULL || pid_name == NULL ||
        unit_name == NULL || !isfinite(measurement->value) ||
        (responder_available && responder_id >
            (responder_extended ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff))))
        return false;
    if (!format_value(measurement->value, value, sizeof(value)))
        return false;
    written = snprintf(prefix, sizeof(prefix), "sample,%llu,",
                       (unsigned long long)sequence);
    if (written < 0 || (size_t)written >= sizeof(prefix)) return false;
    written = snprintf(row, sizeof(row), "%llu,0x%02X,",
                       (unsigned long long)timestamp_ms,
                       (unsigned int)measurement->pid);
    if (written < 0 || (size_t)written >= sizeof(row)) return false;
    if (!emit(recorder->sink, recorder->context, prefix) ||
        !emit(recorder->sink, recorder->context, row) ||
        !emit_quoted(recorder->sink, recorder->context, pid_name) ||
        !emit(recorder->sink, recorder->context, ",") ||
        !emit(recorder->sink, recorder->context, value) ||
        !emit(recorder->sink, recorder->context, ",") ||
        !emit_quoted(recorder->sink, recorder->context, unit_name))
        return latch_failure(recorder);
    if (responder_available) {
        written = responder_extended
            ? snprintf(row, sizeof(row),
                       ",%u,0x%08X,1,\"\",\"\",\"\"\n",
                       favourite ? 1U : 0U,
                       (unsigned int)responder_id)
            : snprintf(row, sizeof(row),
                       ",%u,0x%03X,0,\"\",\"\",\"\"\n",
                       favourite ? 1U : 0U,
                       (unsigned int)responder_id);
    } else {
        written = snprintf(row, sizeof(row),
                           ",%u,\"\",\"\",\"\",\"\",\"\"\n",
                           favourite ? 1U : 0U);
    }
    if (written < 0 || (size_t)written >= sizeof(row)) return false;
    return emit(recorder->sink, recorder->context, row)
        ? true : latch_failure(recorder);
}

bool link_telemetry_recorder_record_sample_named(
    LinkTelemetryRecorder *recorder,
    const LinkTelemetrySample *sample,
    bool favourite,
    const char *pid_name,
    const char *unit_name)
{
    if (sample == NULL) return false;
    return recorder_record_sample_named(
        recorder, sample->sequence, sample->timestamp_ms,
        &sample->measurement, favourite, false, 0U, false,
        pid_name, unit_name);
}

bool link_telemetry_recorder_record_responder_sample_named(
    LinkTelemetryRecorder *recorder,
    const LinkResponderTelemetrySample *sample,
    bool favourite,
    const char *pid_name,
    const char *unit_name)
{
    if (sample == NULL) return false;
    return recorder_record_sample_named(
        recorder, sample->sequence, sample->timestamp_ms,
        &sample->measurement, favourite, true, sample->responder_id,
        sample->extended_id, pid_name, unit_name);
}


bool link_telemetry_recorder_record_structured_pid_named(
    LinkTelemetryRecorder *recorder,
    const LinkStructuredTelemetrySample *sample,
    bool favourite,
    const char *pid_name)
{
    char prefix[96];
    char address[32];
    char name[192];
    char value[64];
    char raw[LINK_OBD2_MAX_PID_PAYLOAD * 3U + 5U];
    size_t index;
    int written;

    if (recorder == NULL || !recorder->started || recorder->finished ||
        recorder->failed || sample == NULL ||
        sample->decoded.definition == NULL || pid_name == NULL ||
        (sample->responder_id_available &&
         sample->responder_id >
             (sample->extended_id ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff))) ||
        !format_structured_raw(&sample->decoded, raw, sizeof(raw))) {
        return false;
    }

    if (sample->responder_id_available) {
        written = sample->extended_id
            ? snprintf(address, sizeof(address), "0x%08X",
                       (unsigned int)sample->responder_id)
            : snprintf(address, sizeof(address), "0x%03X",
                       (unsigned int)sample->responder_id);
        if (written < 0 || (size_t)written >= sizeof(address)) return false;
    } else {
        address[0] = '\0';
    }

    if (sample->decoded.signal_count == 0U) {
        written = snprintf(
            prefix, sizeof(prefix), "structured_raw,%llu,%llu,0x%02X,",
            (unsigned long long)sample->sequence,
            (unsigned long long)sample->timestamp_ms,
            (unsigned int)sample->decoded.definition->pid);
        if (written < 0 || (size_t)written >= sizeof(prefix) ||
            !emit(recorder->sink, recorder->context, prefix) ||
            !emit_quoted(recorder->sink, recorder->context, pid_name) ||
            !emit(recorder->sink, recorder->context, ",,,"))
            return latch_failure(recorder);
        written = snprintf(prefix, sizeof(prefix), "%u,",
                           favourite ? 1U : 0U);
        if (written < 0 || (size_t)written >= sizeof(prefix) ||
            !emit(recorder->sink, recorder->context, prefix) ||
            !emit_quoted(recorder->sink, recorder->context, address) ||
            !emit(recorder->sink, recorder->context,
                  sample->responder_id_available
                      ? (sample->extended_id ? ",1,"","","
                                             : ",0,"","",")
                      : ","","","",") ||
            !emit_quoted(recorder->sink, recorder->context, raw) ||
            !emit(recorder->sink, recorder->context, "\n"))
            return latch_failure(recorder);
        return true;
    }

    for (index = 0U; index < sample->decoded.signal_count; ++index) {
        const LinkObd2DecodedSignal *signal = &sample->decoded.signals[index];
        const char *label =
            signal->label != NULL && signal->label[0] != '\0'
                ? signal->label : "value";
        const char *unit =
            signal->unit != NULL ? signal->unit : "";
        if (!isfinite(signal->value) ||
            !format_value(signal->value, value, sizeof(value)))
            return false;
        written = snprintf(name, sizeof(name), "%s · %s", pid_name, label);
        if (written < 0 || (size_t)written >= sizeof(name)) return false;
        written = snprintf(
            prefix, sizeof(prefix), "structured,%llu,%llu,0x%02X,",
            (unsigned long long)sample->sequence,
            (unsigned long long)sample->timestamp_ms,
            (unsigned int)sample->decoded.definition->pid);
        if (written < 0 || (size_t)written >= sizeof(prefix) ||
            !emit(recorder->sink, recorder->context, prefix) ||
            !emit_quoted(recorder->sink, recorder->context, name) ||
            !emit(recorder->sink, recorder->context, ",") ||
            !emit(recorder->sink, recorder->context, value) ||
            !emit(recorder->sink, recorder->context, ",") ||
            !emit_quoted(recorder->sink, recorder->context, unit))
            return latch_failure(recorder);
        written = snprintf(prefix, sizeof(prefix), ",%u,",
                           favourite ? 1U : 0U);
        if (written < 0 || (size_t)written >= sizeof(prefix) ||
            !emit(recorder->sink, recorder->context, prefix) ||
            !emit_quoted(recorder->sink, recorder->context, address) ||
            !emit(recorder->sink, recorder->context,
                  sample->responder_id_available
                      ? (sample->extended_id ? ",1,"","","
                                             : ",0,"","",")
                      : ","","","",") ||
            !emit_quoted(recorder->sink, recorder->context, raw) ||
            !emit(recorder->sink, recorder->context, "\n"))
            return latch_failure(recorder);
    }
    return true;
}

bool link_telemetry_recorder_record_response_named(
    LinkTelemetryRecorder *recorder,
    uint64_t timestamp_ms,
    const char *command,
    const char *result_name,
    const char *response_text)
{
    char line[96];
    int written;
    if (recorder == NULL || !recorder->started || recorder->finished ||
        recorder->failed || recorder->sink == NULL || command == NULL ||
        result_name == NULL || response_text == NULL)
        return false;
    written = snprintf(line, sizeof(line), "transcript,,%llu,,,,,,,,",
                       (unsigned long long)timestamp_ms);
    if (written < 0 || (size_t)written >= sizeof(line)) return false;
    if (!emit(recorder->sink, recorder->context, line) ||
        !emit_quoted(recorder->sink, recorder->context, command) ||
        !emit(recorder->sink, recorder->context, ",") ||
        !emit_quoted(recorder->sink, recorder->context, result_name) ||
        !emit(recorder->sink, recorder->context, ",") ||
        !emit_quoted(recorder->sink, recorder->context, response_text) ||
        !emit(recorder->sink, recorder->context, "\n"))
        return latch_failure(recorder);
    return true;
}

bool link_telemetry_recorder_finish(LinkTelemetryRecorder *recorder,
                                    uint64_t ended_epoch_ms)
{
    char line[96];
    int written;
    if (recorder == NULL || !recorder->started || recorder->finished ||
        recorder->failed)
        return false;
    written = snprintf(line, sizeof(line), "# session_ended_epoch_ms,%llu\n",
                       (unsigned long long)ended_epoch_ms);
    if (written < 0 || (size_t)written >= sizeof(line)) return false;
    if (!emit(recorder->sink, recorder->context, line))
        return latch_failure(recorder);
    recorder->finished = true;
    return true;
}

bool link_telemetry_export_csv_named(
    const LinkTelemetryStore *store,
    const LinkTelemetrySessionMetadata *metadata,
    const char *product_slug,
    LinkTelemetryPidName pid_name,
    LinkTelemetryUnitName unit_name,
    LinkTelemetryResultName result_name,
    LinkTelemetryTextSink sink,
    void *context)
{
    char line[256];
    char value[64];
    int written;
    size_t index;
    if (store == NULL || metadata == NULL || product_slug == NULL ||
        product_slug[0] == '\0' || pid_name == NULL || unit_name == NULL ||
        result_name == NULL || sink == NULL)
        return false;
    written = snprintf(
        line, sizeof(line),
        "# %s_csv_version,1\n# session_started_epoch_ms,%llu\n"
        "# session_ended_epoch_ms,%llu\n",
        product_slug,
        (unsigned long long)metadata->started_epoch_ms,
        (unsigned long long)metadata->ended_epoch_ms);
    if (written < 0 || (size_t)written >= sizeof(line) ||
        !emit(sink, context, line) ||
        !emit_build_identity(sink, context, product_slug) ||
        !emit_metadata(sink, context, "adapter_identifier",
                       metadata->adapter_identifier) ||
        !emit_metadata(sink, context, "vehicle_identifier",
                       metadata->vehicle_identifier) ||
        !emit(sink, context,
              "sequence,timestamp_ms,pid,name,value,unit,favourite\n"))
        return false;
    for (index = 0U; index < store->history_count; ++index) {
        LinkTelemetrySample sample;
        if (!link_telemetry_store_history_at(store, index, &sample) ||
            !format_value(sample.measurement.value, value, sizeof(value)))
            return false;
        written = snprintf(line, sizeof(line), "%llu,%llu,0x%02X,",
                           (unsigned long long)sample.sequence,
                           (unsigned long long)sample.timestamp_ms,
                           (unsigned int)sample.measurement.pid);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !emit(sink, context, line) ||
            !emit_quoted(sink, context, pid_name(sample.measurement.pid)) ||
            !emit(sink, context, ",") ||
            !emit(sink, context, value) ||
            !emit(sink, context, ",") ||
            !emit_quoted(sink, context,
                         unit_name((uint32_t)sample.measurement.unit)))
            return false;
        written = snprintf(line, sizeof(line), ",%u\n",
                           store->favourite[sample.measurement.pid] ? 1U : 0U);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !emit(sink, context, line))
            return false;
    }
    if (!emit(sink, context, "# diagnostic_transcript\n") ||
        !emit(sink, context, "timestamp_ms,command,result,response\n"))
        return false;
    for (index = 0U; index < store->transcript_count; ++index) {
        LinkTelemetryTranscriptEntry entry;
        if (!link_telemetry_store_transcript_at(store, index, &entry))
            return false;
        written = snprintf(line, sizeof(line), "%llu,",
                           (unsigned long long)entry.timestamp_ms);
        if (written < 0 || (size_t)written >= sizeof(line) ||
            !emit(sink, context, line) ||
            !emit_quoted(sink, context, entry.command) ||
            !emit(sink, context, ",") ||
            !emit_quoted(sink, context, result_name(entry.result)) ||
            !emit(sink, context, ",") ||
            !emit_quoted(sink, context, entry.response) ||
            !emit(sink, context, "\n"))
            return false;
    }
    return true;
}
