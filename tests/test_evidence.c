/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "link/discover.h"

#include <stdio.h>
#include <string.h>

static int require_substring(const char *text, const char *needle)
{
    if (strstr(text, needle) != NULL) {
        return 0;
    }
    (void)fprintf(stderr, "evidence output is missing: %s\n", needle);
    return 1;
}

static FILE *open_binary_read(const char *path)
{
#if defined(_MSC_VER)
    FILE *file = NULL;
    if (fopen_s(&file, path, "rb") != 0) {
        return NULL;
    }
    return file;
#else
    return fopen(path, "rb");
#endif
}

int main(void)
{
    const char *path = "link-evidence-test.jsonl";
    const unsigned char bytes[] = {0x01U, 0x09U, 0x0AU, 0xFFU};
    char buffer[2048];
    size_t used;
    FILE *file;
    link_evidence_writer *writer;
    int failures = 0;

    writer = link_evidence_open(path);
    if (writer == NULL) {
        (void)fprintf(stderr, "cannot open evidence writer\n");
        return 1;
    }

    if (link_evidence_write_frame(writer,
                                  123456789ULL,
                                  "rx",
                                  "CAN",
                                  0x7E8U,
                                  bytes,
                                  sizeof(bytes),
                                  "operator \"note\"") != 0) {
        (void)fprintf(stderr, "cannot write evidence frame\n");
        link_evidence_close(writer);
        (void)remove(path);
        return 1;
    }

    if (link_evidence_write_annotation(writer,
                                       123456790ULL,
                                       "line1\nline2") != 0) {
        (void)fprintf(stderr, "cannot write evidence annotation\n");
        link_evidence_close(writer);
        (void)remove(path);
        return 1;
    }

    {
        LinkResearchState research;
        size_t event_index;
        (void)link_research_begin(&research, UINT64_C(123456791));
        (void)link_research_set_phase(
            &research, LINK_RESEARCH_PHASE_PASSIVE_CAPTURE,
            UINT64_C(123456792));
        (void)link_research_record_frame(
            &research, LINK_RESEARCH_DIRECTION_RX);
        event_index = link_research_mark_event(&research);
        if (link_evidence_write_research_session(
                writer, UINT64_C(123456791), "TEST", "mock", 500000U) != 0 ||
            link_evidence_write_research_phase(
                writer, UINT64_C(123456792), research.phase) != 0 ||
            link_evidence_write_event_marker(
                writer, UINT64_C(123456793), event_index, "accelerate") != 0 ||
            link_evidence_write_research_summary(
                writer, UINT64_C(123456794), &research) != 0) {
            (void)fprintf(stderr, "cannot write research evidence\n");
            link_evidence_close(writer);
            (void)remove(path);
            return 1;
        }
    }

    if (link_evidence_flush(writer) != 0) {
        (void)fprintf(stderr, "cannot flush evidence writer\n");
        link_evidence_close(writer);
        (void)remove(path);
        return 1;
    }
    link_evidence_close(writer);

    file = open_binary_read(path);
    if (file == NULL) {
        (void)fprintf(stderr, "cannot reopen evidence output\n");
        (void)remove(path);
        return 1;
    }

    used = fread(buffer, 1U, sizeof(buffer) - 1U, file);
    if (ferror(file) != 0) {
        (void)fprintf(stderr, "cannot read evidence output\n");
        (void)fclose(file);
        (void)remove(path);
        return 1;
    }
    buffer[used] = '\0';
    (void)fclose(file);
    (void)remove(path);

    failures += require_substring(buffer, "\"timestamp_ns\":123456789");
    failures += require_substring(buffer, "\"can_id\":\"0x000007E8\"");
    failures += require_substring(buffer, "\"data\":\"01090AFF\"");
    failures += require_substring(buffer, "operator \\\"note\\\"");
    failures += require_substring(buffer, "line1\\nline2");
    failures += require_substring(buffer, "\"type\":\"research-session\"");
    failures += require_substring(buffer, "\"product\":\"TEST\"");
    failures += require_substring(buffer, "\"type\":\"research-phase\"");
    failures += require_substring(buffer, "\"phase\":\"passive-capture\"");
    failures += require_substring(buffer, "\"type\":\"event-marker\"");
    failures += require_substring(buffer, "\"text\":\"accelerate\"");
    failures += require_substring(buffer, "\"type\":\"research-summary\"");
    failures += require_substring(buffer, "\"frames\":1");

    return failures == 0 ? 0 : 1;
}
