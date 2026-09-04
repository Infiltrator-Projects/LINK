// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_TELEMETRY_H
#define LINK_TELEMETRY_H

#include "link/parameter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_TELEMETRY_HISTORY_CAPACITY 512U
#define LINK_TELEMETRY_ADAPTER_TEXT_LENGTH 96U
#define LINK_TELEMETRY_VEHICLE_TEXT_LENGTH 64U
#define LINK_TELEMETRY_TRANSCRIPT_CAPACITY 64U
#define LINK_TELEMETRY_TRANSCRIPT_COMMAND_LENGTH 64U
#define LINK_TELEMETRY_TRANSCRIPT_RESPONSE_LENGTH 192U
#define LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY 1024U
#define LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY 256U

typedef struct {
    uint8_t pid;
    double value;
    LinkObd2UnitCode unit;
} LinkTelemetryMeasurement;

typedef struct {
    uint64_t sequence;
    uint64_t timestamp_ms;
    LinkTelemetryMeasurement measurement;
} LinkTelemetrySample;

typedef struct {
    uint64_t sequence;
    uint64_t timestamp_ms;
    uint32_t responder_id;
    bool extended_id;
    LinkTelemetryMeasurement measurement;
} LinkResponderTelemetrySample;

/**
 * Independent bounded history for source-attributed OBD replies. The legacy
 * PID store remains unchanged and continues to expose one preferred value per
 * request to existing consumers.
 */
typedef struct {
    LinkResponderTelemetrySample history[
        LINK_RESPONDER_TELEMETRY_HISTORY_CAPACITY];
    size_t history_head;
    size_t history_count;
    uint64_t next_sequence;
    uint64_t total_sample_count;
} LinkResponderTelemetryStore;

/**
 * Bounded history for Mode 01 values that cannot be losslessly flattened to
 * one scalar. The complete decoded payload is retained together with source
 * attribution so DPF/NOx/aftertreatment and raw assigned SAE items have the
 * same history/evidence lifecycle as legacy scalar PIDs.
 */
typedef struct {
    uint64_t sequence;
    uint64_t timestamp_ms;
    bool responder_id_available;
    uint32_t responder_id;
    bool extended_id;
    LinkObd2DecodedPid decoded;
} LinkStructuredTelemetrySample;

typedef struct {
    LinkStructuredTelemetrySample history[
        LINK_STRUCTURED_TELEMETRY_HISTORY_CAPACITY];
    size_t history_head;
    size_t history_count;
    uint64_t next_sequence;
    uint64_t total_sample_count;
} LinkStructuredTelemetryStore;

typedef struct {
    uint64_t timestamp_ms;
    uint32_t result;
    char command[LINK_TELEMETRY_TRANSCRIPT_COMMAND_LENGTH];
    char response[LINK_TELEMETRY_TRANSCRIPT_RESPONSE_LENGTH];
} LinkTelemetryTranscriptEntry;

typedef struct {
    LinkTelemetrySample history[LINK_TELEMETRY_HISTORY_CAPACITY];
    LinkTelemetrySample latest[256U];
    bool latest_valid[256U];
    bool favourite[256U];
    LinkTelemetryTranscriptEntry transcript[LINK_TELEMETRY_TRANSCRIPT_CAPACITY];
    size_t transcript_head;
    size_t transcript_count;
    size_t history_head;
    size_t history_count;
    uint64_t next_sequence;
    uint64_t total_sample_count;
} LinkTelemetryStore;

typedef struct {
    uint64_t started_epoch_ms;
    uint64_t ended_epoch_ms;
    char adapter_identifier[LINK_TELEMETRY_ADAPTER_TEXT_LENGTH];
    char vehicle_identifier[LINK_TELEMETRY_VEHICLE_TEXT_LENGTH];
} LinkTelemetrySessionMetadata;

typedef bool (*LinkTelemetryTextSink)(void *context, const char *bytes, size_t length);
typedef const char *(*LinkTelemetryPidName)(uint8_t pid);
typedef const char *(*LinkTelemetryUnitName)(uint32_t unit);
typedef const char *(*LinkTelemetryResultName)(uint32_t result);

typedef struct {
    LinkTelemetryTextSink sink;
    void *context;
    bool started;
    bool finished;
    bool failed;
} LinkTelemetryRecorder;

void link_telemetry_store_init(LinkTelemetryStore *store);
void link_telemetry_store_clear_samples(LinkTelemetryStore *store);
bool link_telemetry_store_record(LinkTelemetryStore *store, uint64_t timestamp_ms, const LinkTelemetryMeasurement *measurement);
bool link_telemetry_store_latest(const LinkTelemetryStore *store, uint8_t pid, LinkTelemetrySample *sample);
size_t link_telemetry_store_history_count(const LinkTelemetryStore *store);
uint64_t link_telemetry_store_total_sample_count(const LinkTelemetryStore *store);
bool link_telemetry_store_history_at(const LinkTelemetryStore *store, size_t chronological_index, LinkTelemetrySample *sample);
void link_telemetry_store_set_favourite(LinkTelemetryStore *store, uint8_t pid, bool favourite);
bool link_telemetry_store_is_favourite(const LinkTelemetryStore *store, uint8_t pid);
bool link_telemetry_store_record_transcript(LinkTelemetryStore *store, uint64_t timestamp_ms, const char *command, uint32_t result, const char *response_text);
size_t link_telemetry_store_transcript_count(const LinkTelemetryStore *store);
bool link_telemetry_store_transcript_at(const LinkTelemetryStore *store, size_t chronological_index, LinkTelemetryTranscriptEntry *entry);

void link_responder_telemetry_store_init(LinkResponderTelemetryStore *store);
void link_responder_telemetry_store_clear(LinkResponderTelemetryStore *store);
bool link_responder_telemetry_store_record(
    LinkResponderTelemetryStore *store,
    uint64_t timestamp_ms,
    uint32_t responder_id,
    bool extended_id,
    const LinkTelemetryMeasurement *measurement);
size_t link_responder_telemetry_store_history_count(
    const LinkResponderTelemetryStore *store);
uint64_t link_responder_telemetry_store_total_sample_count(
    const LinkResponderTelemetryStore *store);
bool link_responder_telemetry_store_history_at(
    const LinkResponderTelemetryStore *store,
    size_t chronological_index,
    LinkResponderTelemetrySample *sample);

void link_structured_telemetry_store_init(LinkStructuredTelemetryStore *store);
void link_structured_telemetry_store_clear(LinkStructuredTelemetryStore *store);
bool link_structured_telemetry_store_record(
    LinkStructuredTelemetryStore *store,
    uint64_t timestamp_ms,
    bool responder_id_available,
    uint32_t responder_id,
    bool extended_id,
    const LinkObd2DecodedPid *decoded);
size_t link_structured_telemetry_store_history_count(
    const LinkStructuredTelemetryStore *store);
uint64_t link_structured_telemetry_store_total_sample_count(
    const LinkStructuredTelemetryStore *store);
bool link_structured_telemetry_store_history_at(
    const LinkStructuredTelemetryStore *store,
    size_t chronological_index,
    LinkStructuredTelemetrySample *sample);

void link_telemetry_session_metadata_init(LinkTelemetrySessionMetadata *metadata, uint64_t started_epoch_ms, const char *adapter_identifier, const char *vehicle_identifier);
void link_telemetry_session_metadata_set_adapter(LinkTelemetrySessionMetadata *metadata, const char *adapter_identifier);
void link_telemetry_session_metadata_set_vehicle(LinkTelemetrySessionMetadata *metadata, const char *vehicle_identifier);
void link_telemetry_session_metadata_finish(LinkTelemetrySessionMetadata *metadata, uint64_t ended_epoch_ms);

void link_telemetry_recorder_init(LinkTelemetryRecorder *recorder);
bool link_telemetry_recorder_begin(LinkTelemetryRecorder *recorder, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryTextSink sink, void *context);
/**
 * Append another session to an existing recorder stream.
 *
 * The stream-version and CSV column headers are deliberately omitted so one
 * exported evidence file can contain multiple connection attempts without
 * becoming a sequence of independently headed CSV documents. The caller must
 * have previously emitted the stream header with link_telemetry_recorder_begin().
 */
bool link_telemetry_recorder_continue(LinkTelemetryRecorder *recorder, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryTextSink sink, void *context);
bool link_telemetry_recorder_record_sample_named(LinkTelemetryRecorder *recorder, const LinkTelemetrySample *sample, bool favourite, const char *pid_name, const char *unit_name);
/**
 * Record one live value with the exact CAN responder that supplied it.
 * Stream schema v2 stores the address and addressing width in dedicated
 * columns so simultaneous functional-OBD responders remain distinguishable.
 */
bool link_telemetry_recorder_record_responder_sample_named(
    LinkTelemetryRecorder *recorder,
    const LinkResponderTelemetrySample *sample,
    bool favourite,
    const char *pid_name,
    const char *unit_name);

/**
 * Record one structured/raw SAE payload. Formula-backed signals are emitted
 * as individual structured rows sharing the same sequence number; the complete
 * raw payload is retained in the response column for forensic replay. A raw
 * or text-only PID still emits one row, so no assigned value disappears from
 * an exported session simply because it is not scalar.
 */
bool link_telemetry_recorder_record_structured_pid_named(
    LinkTelemetryRecorder *recorder,
    const LinkStructuredTelemetrySample *sample,
    bool favourite,
    const char *pid_name);
bool link_telemetry_recorder_record_response_named(LinkTelemetryRecorder *recorder, uint64_t timestamp_ms, const char *command, const char *result_name, const char *response_text);
bool link_telemetry_recorder_finish(LinkTelemetryRecorder *recorder, uint64_t ended_epoch_ms);
bool link_telemetry_export_csv_named(const LinkTelemetryStore *store, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryPidName pid_name, LinkTelemetryUnitName unit_name, LinkTelemetryResultName result_name, LinkTelemetryTextSink sink, void *context);

/**
 * Record a complete scalar OBD-II sample without duplicating the conversion
 * from LINK's OBD representation into the telemetry measurement model.
 */
static inline bool link_telemetry_store_record_obd2_sample(
    LinkTelemetryStore *store,
    uint64_t timestamp_ms,
    const LinkObd2Sample *sample)
{
    LinkTelemetryMeasurement measurement;
    if (sample == NULL) return false;
    measurement.pid = sample->pid;
    measurement.value = sample->value;
    measurement.unit = (LinkObd2UnitCode)sample->unit;
    return link_telemetry_store_record(store, timestamp_ms, &measurement);
}

/**
 * Record an ELM327 response transcript while preserving LINK's canonical
 * parser result code and normalised response text.
 */
static inline bool link_telemetry_store_record_elm327_transcript(
    LinkTelemetryStore *store,
    uint64_t timestamp_ms,
    const char *command,
    const LinkElm327Response *response)
{
    if (response == NULL) return false;
    return link_telemetry_store_record_transcript(
        store,
        timestamp_ms,
        command,
        (uint32_t)response->result,
        response->text);
}

#ifdef __cplusplus
}
#endif
#endif
