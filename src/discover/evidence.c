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

    writer->file = fopen(path, "wb");
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
