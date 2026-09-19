/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "link/discover.h"

#include <stdio.h>
#include <stdlib.h>

struct link_evidence_writer {
    FILE *file;
};

static int json_string(FILE *file, const char *text)
{
    const unsigned char *p =
        (const unsigned char *)(text != NULL ? text : "");

    if (fputc('"', file) == EOF) {
        return -1;
    }

    while (*p != 0U) {
        switch (*p) {
        case '"':
            if (fputs("\\\"", file) == EOF) return -1;
            break;
        case '\\':
            if (fputs("\\\\", file) == EOF) return -1;
            break;
        case '\b':
            if (fputs("\\b", file) == EOF) return -1;
            break;
        case '\f':
            if (fputs("\\f", file) == EOF) return -1;
            break;
        case '\n':
            if (fputs("\\n", file) == EOF) return -1;
            break;
        case '\r':
            if (fputs("\\r", file) == EOF) return -1;
            break;
        case '\t':
            if (fputs("\\t", file) == EOF) return -1;
            break;
        default:
            if (*p < 0x20U) {
                if (fprintf(file, "\\u%04x", (unsigned int)*p) < 0) {
                    return -1;
                }
            } else if (fputc((int)*p, file) == EOF) {
                return -1;
            }
            break;
        }
        ++p;
    }

    return fputc('"', file) == EOF ? -1 : 0;
}

static FILE *open_binary_write(const char *path)
{
#if defined(_MSC_VER)
    FILE *file = NULL;
    if (fopen_s(&file, path, "wb") != 0) {
        return NULL;
    }
    return file;
#else
    return fopen(path, "wb");
#endif
}

link_evidence_writer *link_evidence_open(const char *path)
{
    link_evidence_writer *writer;

    if (path == NULL || path[0] == '\0') {
        return NULL;
    }

    writer = (link_evidence_writer *)calloc(1U, sizeof(*writer));
    if (writer == NULL) {
        return NULL;
    }

    writer->file = open_binary_write(path);
    if (writer->file == NULL) {
        free(writer);
        return NULL;
    }

    return writer;
}

int link_evidence_write_frame(link_evidence_writer *writer,
                              uint64_t timestamp_ns,
                              const char *direction,
                              const char *protocol,
                              uint32_t can_id,
                              const uint8_t *data,
                              size_t length,
                              const char *annotation)
{
    size_t i;

    if (writer == NULL || writer->file == NULL ||
        (length != 0U && data == NULL)) {
        return -1;
    }

    if (fprintf(writer->file,
                "{\"type\":\"frame\",\"timestamp_ns\":%llu,\"direction\":",
                (unsigned long long)timestamp_ns) < 0) {
        return -1;
    }
    if (json_string(writer->file, direction) != 0) {
        return -1;
    }
    if (fputs(",\"protocol\":", writer->file) == EOF ||
        json_string(writer->file, protocol) != 0) {
        return -1;
    }
    if (fprintf(writer->file,
                ",\"can_id\":\"0x%08X\",\"data\":\"",
                (unsigned int)can_id) < 0) {
        return -1;
    }
    for (i = 0U; i < length; ++i) {
        if (fprintf(writer->file, "%02X", (unsigned int)data[i]) < 0) {
            return -1;
        }
    }
    if (fputs("\",\"annotation\":", writer->file) == EOF ||
        json_string(writer->file, annotation) != 0) {
        return -1;
    }

    return fputs("}\n", writer->file) == EOF ? -1 : 0;
}

int link_evidence_write_annotation(link_evidence_writer *writer,
                                   uint64_t timestamp_ns,
                                   const char *text)
{
    if (writer == NULL || writer->file == NULL) {
        return -1;
    }

    if (fprintf(writer->file,
                "{\"type\":\"annotation\",\"timestamp_ns\":%llu,\"text\":",
                (unsigned long long)timestamp_ns) < 0) {
        return -1;
    }
    if (json_string(writer->file, text) != 0) {
        return -1;
    }

    return fputs("}\n", writer->file) == EOF ? -1 : 0;
}

int link_evidence_write_research_session(link_evidence_writer *writer,
                                         uint64_t timestamp_ns,
                                         const char *product,
                                         const char *transport,
                                         uint32_t nominal_bitrate)
{
    if (writer == NULL || writer->file == NULL) return -1;
    if (fprintf(writer->file,
                "{\"type\":\"research-session\",\"timestamp_ns\":%llu,\"product\":",
                (unsigned long long)timestamp_ns) < 0 ||
        json_string(writer->file, product) != 0 ||
        fputs(",\"transport\":", writer->file) == EOF ||
        json_string(writer->file, transport) != 0 ||
        fprintf(writer->file, ",\"nominal_bitrate\":%u}\n",
                (unsigned int)nominal_bitrate) < 0) {
        return -1;
    }
    return 0;
}

int link_evidence_write_research_phase(link_evidence_writer *writer,
                                       uint64_t timestamp_ns,
                                       LinkResearchPhase phase)
{
    if (writer == NULL || writer->file == NULL) return -1;
    if (fprintf(writer->file,
                "{\"type\":\"research-phase\",\"timestamp_ns\":%llu,\"phase\":",
                (unsigned long long)timestamp_ns) < 0 ||
        json_string(writer->file, link_research_phase_name(phase)) != 0) {
        return -1;
    }
    return fputs("}\n", writer->file) == EOF ? -1 : 0;
}

int link_evidence_write_event_marker(link_evidence_writer *writer,
                                     uint64_t timestamp_ns,
                                     size_t marker_index,
                                     const char *text)
{
    if (writer == NULL || writer->file == NULL || marker_index == 0U)
        return -1;
    if (fprintf(writer->file,
                "{\"type\":\"event-marker\",\"timestamp_ns\":%llu,\"index\":%llu,\"text\":",
                (unsigned long long)timestamp_ns,
                (unsigned long long)marker_index) < 0 ||
        json_string(writer->file, text) != 0) {
        return -1;
    }
    return fputs("}\n", writer->file) == EOF ? -1 : 0;
}

int link_evidence_write_research_summary(link_evidence_writer *writer,
                                         uint64_t timestamp_ns,
                                         const LinkResearchState *state)
{
    if (writer == NULL || writer->file == NULL || state == NULL) return -1;
    return fprintf(writer->file,
        "{\"type\":\"research-summary\",\"timestamp_ns\":%llu,"
        "\"phase\":\"%s\",\"frames\":%llu,\"tx_frames\":%llu,"
        "\"rx_frames\":%llu,\"events\":%llu,\"phase_transitions\":%llu}\n",
        (unsigned long long)timestamp_ns,
        link_research_phase_name(state->phase),
        (unsigned long long)state->frame_count,
        (unsigned long long)state->tx_frame_count,
        (unsigned long long)state->rx_frame_count,
        (unsigned long long)state->event_count,
        (unsigned long long)state->phase_transition_count) < 0 ? -1 : 0;
}

int link_evidence_flush(link_evidence_writer *writer)
{
    if (writer == NULL || writer->file == NULL) {
        return -1;
    }
    return fflush(writer->file) == 0 ? 0 : -1;
}

void link_evidence_close(link_evidence_writer *writer)
{
    if (writer == NULL) {
        return;
    }
    if (writer->file != NULL) {
        (void)fclose(writer->file);
    }
    free(writer);
}
