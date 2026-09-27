// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_FLASH_JOURNAL_H
#define LINK_FLASH_JOURNAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A record may occupy several pages; complete slots are rotated as a unit. */
typedef struct {
    void *context;
    bool (*read)(void *, uint32_t, void *, size_t);
    bool (*erase_page)(void *, uint32_t);
    bool (*program)(void *, uint32_t, const void *, size_t);
    const uint32_t *page_addresses;
    size_t page_count;
    uint32_t page_size;
    size_t record_size;
} LinkFlashJournal;

/* Caller owns record integrity and format. A rejected/torn slot is skipped. */
typedef bool (*LinkFlashJournalValidFn)(const void *record, void *context);
typedef uint32_t (*LinkFlashJournalGenerationFn)(
    const void *record, void *context);

bool link_flash_journal_valid(const LinkFlashJournal *journal);
size_t link_flash_journal_pages_per_slot(const LinkFlashJournal *journal);
size_t link_flash_journal_slot_count(const LinkFlashJournal *journal);
bool link_flash_journal_slot_address(
    const LinkFlashJournal *journal, size_t slot, uint32_t *address);
bool link_flash_journal_read(
    const LinkFlashJournal *journal, size_t slot, void *record);
/* Erases the destination slot, writes, and reads back the complete record. */
bool link_flash_journal_write(
    const LinkFlashJournal *journal, size_t slot, const void *record,
    void *verification, LinkFlashJournalValidFn valid, void *context);
/* Caller supplies scratch storage of record_size bytes, distinct from output. */
bool link_flash_journal_latest(
    const LinkFlashJournal *journal, void *output, void *scratch,
    LinkFlashJournalValidFn valid, LinkFlashJournalGenerationFn generation,
    void *context, uint32_t *address);
/* Returns the next slot after active_address; unknown active address starts at 0. */
bool link_flash_journal_next(
    const LinkFlashJournal *journal, uint32_t active_address,
    size_t *slot, uint32_t *address);

#ifdef __cplusplus
}
#endif
#endif
