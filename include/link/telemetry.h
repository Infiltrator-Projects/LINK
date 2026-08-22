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

void link_telemetry_session_metadata_init(LinkTelemetrySessionMetadata *metadata, uint64_t started_epoch_ms, const char *adapter_identifier, const char *vehicle_identifier);
void link_telemetry_session_metadata_set_adapter(LinkTelemetrySessionMetadata *metadata, const char *adapter_identifier);
void link_telemetry_session_metadata_set_vehicle(LinkTelemetrySessionMetadata *metadata, const char *vehicle_identifier);
void link_telemetry_session_metadata_finish(LinkTelemetrySessionMetadata *metadata, uint64_t ended_epoch_ms);

void link_telemetry_recorder_init(LinkTelemetryRecorder *recorder);
bool link_telemetry_recorder_begin(LinkTelemetryRecorder *recorder, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryTextSink sink, void *context);
bool link_telemetry_recorder_record_sample_named(LinkTelemetryRecorder *recorder, const LinkTelemetrySample *sample, bool favourite, const char *pid_name, const char *unit_name);
bool link_telemetry_recorder_record_response_named(LinkTelemetryRecorder *recorder, uint64_t timestamp_ms, const char *command, const char *result_name, const char *response_text);
bool link_telemetry_recorder_finish(LinkTelemetryRecorder *recorder, uint64_t ended_epoch_ms);
bool link_telemetry_export_csv_named(const LinkTelemetryStore *store, const LinkTelemetrySessionMetadata *metadata, const char *product_slug, LinkTelemetryPidName pid_name, LinkTelemetryUnitName unit_name, LinkTelemetryResultName result_name, LinkTelemetryTextSink sink, void *context);

#ifdef __cplusplus
}
#endif
#endif
