// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/flash_journal.h"

#include <limits.h>
#include <string.h>

size_t link_flash_journal_pages_per_slot(const LinkFlashJournal *journal)
{
    if (journal == NULL || journal->page_size == 0U ||
        journal->record_size == 0U) return 0U;
    return journal->record_size / journal->page_size +
           (journal->record_size % journal->page_size != 0U ? 1U : 0U);
}

size_t link_flash_journal_slot_count(const LinkFlashJournal *journal)
{
    const size_t pages = link_flash_journal_pages_per_slot(journal);
    return pages == 0U ? 0U : journal->page_count / pages;
}

bool link_flash_journal_valid(const LinkFlashJournal *journal)
{
    size_t i, j;
    if (journal == NULL || journal->read == NULL ||
        journal->erase_page == NULL || journal->program == NULL ||
        journal->page_addresses == NULL ||
        link_flash_journal_slot_count(journal) < 2U) return false;
    for (i = 0U; i < journal->page_count; ++i) {
        const uint32_t address = journal->page_addresses[i];
        if (address % journal->page_size != 0U ||
            address > UINT32_MAX - (journal->page_size - 1U)) return false;
        for (j = 0U; j < i; ++j)
            if (address == journal->page_addresses[j]) return false;
    }
    return true;
}

bool link_flash_journal_slot_address(
    const LinkFlashJournal *journal, size_t slot, uint32_t *address)
{
    if (address == NULL || !link_flash_journal_valid(journal) ||
        slot >= link_flash_journal_slot_count(journal)) return false;
    *address = journal->page_addresses[
        slot * link_flash_journal_pages_per_slot(journal)];
    return true;
}

bool link_flash_journal_read(
    const LinkFlashJournal *journal, size_t slot, void *record)
{
    size_t offset = 0U, page;
    if (record == NULL || !link_flash_journal_valid(journal) ||
        slot >= link_flash_journal_slot_count(journal)) return false;
    for (page = 0U; page < link_flash_journal_pages_per_slot(journal); ++page) {
        const size_t remaining = journal->record_size - offset;
        const size_t chunk = remaining < journal->page_size
            ? remaining : journal->page_size;
        if (!journal->read(journal->context,
                journal->page_addresses[
                    slot * link_flash_journal_pages_per_slot(journal) + page],
                (uint8_t *)record + offset, chunk)) return false;
        offset += chunk;
    }
    return true;
}

bool link_flash_journal_write(
    const LinkFlashJournal *journal, size_t slot, const void *record,
    void *verification, LinkFlashJournalValidFn valid, void *context)
{
    size_t page, offset = 0U;
    const size_t pages = link_flash_journal_pages_per_slot(journal);
    if (record == NULL || verification == NULL || record == verification ||
        valid == NULL || !link_flash_journal_valid(journal) ||
        slot >= link_flash_journal_slot_count(journal)) return false;
    for (page = 0U; page < pages; ++page)
        if (!journal->erase_page(journal->context,
                journal->page_addresses[slot * pages + page])) return false;
    for (page = 0U; page < pages; ++page) {
        const size_t remaining = journal->record_size - offset;
        const size_t chunk = remaining < journal->page_size
            ? remaining : journal->page_size;
        if (!journal->program(journal->context,
                journal->page_addresses[slot * pages + page],
                (const uint8_t *)record + offset, chunk)) return false;
        offset += chunk;
    }
    return link_flash_journal_read(journal, slot, verification) &&
           valid(verification, context) &&
           memcmp(record, verification, journal->record_size) == 0;
}

bool link_flash_journal_latest(
    const LinkFlashJournal *journal, void *output, void *scratch,
    LinkFlashJournalValidFn valid, LinkFlashJournalGenerationFn generation,
    void *context, uint32_t *address)
{
    size_t slot;
    bool found = false;
    uint32_t newest = 0U;
    if (output == NULL || scratch == NULL || output == scratch ||
        valid == NULL || generation == NULL || address == NULL ||
        !link_flash_journal_valid(journal)) return false;
    for (slot = 0U; slot < link_flash_journal_slot_count(journal); ++slot) {
        uint32_t current;
        if (!link_flash_journal_read(journal, slot, scratch) ||
            !valid(scratch, context)) continue;
        current = generation(scratch, context);
        if (!found || (int32_t)(current - newest) > 0) {
            memcpy(output, scratch, journal->record_size);
            newest = current;
            *address = journal->page_addresses[
                slot * link_flash_journal_pages_per_slot(journal)];
            found = true;
        }
    }
    return found;
}

bool link_flash_journal_next(
    const LinkFlashJournal *journal, uint32_t active_address,
    size_t *slot, uint32_t *address)
{
    size_t i;
    const size_t count = link_flash_journal_slot_count(journal);
    if (slot == NULL || address == NULL || !link_flash_journal_valid(journal))
        return false;
    *slot = 0U;
    for (i = 0U; i < count; ++i) {
        if (journal->page_addresses[
                i * link_flash_journal_pages_per_slot(journal)] ==
            active_address) {
            *slot = (i + 1U) % count;
            break;
        }
    }
    *address = journal->page_addresses[
        *slot * link_flash_journal_pages_per_slot(journal)];
    return true;
}
