// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/flash_journal.h"

#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

enum { PAGE_SIZE = 32, PAGE_COUNT = 6, RECORD_SIZE = 48 };
typedef struct {
    uint8_t pages[PAGE_COUNT][PAGE_SIZE];
    unsigned int erases[PAGE_COUNT];
    bool fail_program;
} MemoryFlash;
typedef struct {
    uint32_t generation;
    uint8_t payload[RECORD_SIZE - 8U];
    uint32_t checksum;
} Record;

static const uint32_t addresses[PAGE_COUNT] = {
    UINT32_C(0x0801d000), UINT32_C(0x0801d020),
    UINT32_C(0x0801d040), UINT32_C(0x0801d060),
    UINT32_C(0x0801d080), UINT32_C(0x0801d0a0)
};

static bool range(uint32_t address, size_t length, size_t *page)
{
    size_t i;
    for (i = 0U; i < PAGE_COUNT; ++i) {
        if (address == addresses[i] && length <= PAGE_SIZE) {
            *page = i;
            return true;
        }
    }
    return false;
}

static bool read_flash(void *context, uint32_t address, void *data, size_t n)
{
    size_t page;
    MemoryFlash *flash = (MemoryFlash *)context;
    if (!range(address, n, &page)) return false;
    memcpy(data, flash->pages[page], n);
    return true;
}

static bool erase_flash(void *context, uint32_t address)
{
    size_t page;
    MemoryFlash *flash = (MemoryFlash *)context;
    if (!range(address, PAGE_SIZE, &page)) return false;
    memset(flash->pages[page], 0xff, PAGE_SIZE);
    ++flash->erases[page];
    return true;
}

static bool program_flash(
    void *context, uint32_t address, const void *data, size_t n)
{
    size_t page, i;
    MemoryFlash *flash = (MemoryFlash *)context;
    const uint8_t *bytes = (const uint8_t *)data;
    if (!range(address, n, &page)) return false;
    if (flash->fail_program && page % 2U == 1U) return false;
    for (i = 0U; i < n; ++i) {
        if ((flash->pages[page][i] & bytes[i]) != bytes[i]) return false;
        flash->pages[page][i] &= bytes[i];
    }
    return true;
}

static uint32_t checksum(const Record *record)
{
    size_t i;
    uint32_t sum = record->generation;
    for (i = 0U; i < sizeof(record->payload); ++i)
        sum = sum * UINT32_C(33) + record->payload[i];
    return sum;
}

static bool valid_record(const void *record, void *context)
{
    const Record *r = (const Record *)record;
    (void)context;
    return r->checksum == checksum(r);
}

static uint32_t generation(const void *record, void *context)
{
    (void)context;
    return ((const Record *)record)->generation;
}

int main(void)
{
    MemoryFlash flash;
    LinkFlashJournal journal = {0};
    Record record = {0}, verify, recovered, scratch;
    uint32_t active = 0U, next;
    size_t slot, cycle, page;
    memset(&flash, 0xff, sizeof(flash));
    memset(flash.erases, 0, sizeof(flash.erases));
    flash.fail_program = false;
    journal.context = &flash;
    journal.read = read_flash;
    journal.erase_page = erase_flash;
    journal.program = program_flash;
    journal.page_addresses = addresses;
    journal.page_count = PAGE_COUNT;
    journal.page_size = PAGE_SIZE;
    journal.record_size = sizeof(record);
    CHECK(sizeof(record) == RECORD_SIZE);
    CHECK(link_flash_journal_valid(&journal));
    CHECK(link_flash_journal_pages_per_slot(&journal) == 2U);
    CHECK(link_flash_journal_slot_count(&journal) == 3U);
    CHECK(!link_flash_journal_latest(
        &journal, &recovered, &scratch, valid_record, generation,
        NULL, &active));
    for (cycle = 1U; cycle <= 9U; ++cycle) {
        CHECK(link_flash_journal_next(&journal, active, &slot, &next));
        CHECK(slot == (cycle - 1U) % 3U);
        record.generation = (uint32_t)cycle;
        memset(record.payload, (int)cycle, sizeof(record.payload));
        record.checksum = checksum(&record);
        CHECK(link_flash_journal_write(
            &journal, slot, &record, &verify, valid_record, NULL));
        active = next;
        CHECK(link_flash_journal_latest(
            &journal, &recovered, &scratch, valid_record, generation,
            NULL, &next));
        CHECK(next == active &&
              memcmp(&record, &recovered, sizeof(record)) == 0);
    }
    for (page = 0U; page < PAGE_COUNT; ++page)
        CHECK(flash.erases[page] == 3U);

    /* Power loss during the second page leaves the previous generation. */
    CHECK(link_flash_journal_next(&journal, active, &slot, &next));
    record.generation++;
    record.checksum = checksum(&record);
    flash.fail_program = true;
    CHECK(!link_flash_journal_write(
        &journal, slot, &record, &verify, valid_record, NULL));
    flash.fail_program = false;
    CHECK(link_flash_journal_latest(
        &journal, &recovered, &scratch, valid_record, generation,
        NULL, &next));
    CHECK(recovered.generation == 9U && next == active);
    puts("standalone flash journal rotation and torn write passed");
    return 0;
}
