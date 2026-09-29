// SPDX-License-Identifier: GPL-3.0-or-later
/*
 * Standalone STM32F103 flash-journal hardware exercise for LINK issue #49.
 *
 * No CAN, ISO-TP or UDS code is used here. Reserve the final four 2 KiB flash
 * pages in the linker script before running this test:
 *   0x0807E000, 0x0807E800, 0x0807F000, 0x0807F800
 *
 * Inspect the volatile link_flash_test_* globals in the debugger.
 */
#include "main.h"
#include "gpio.h"
#include "stm32f1xx_hal_flash.h"
#include "stm32f1xx_hal_flash_ex.h"

#include "link/flash_journal.h"

#include <string.h>

#define LINK_FLASH_TEST_PAGE_SIZE UINT32_C(0x00000800)
#define LINK_FLASH_TEST_MAGIC UINT32_C(0x4c4a5431)
#ifndef LINK_FLASH_JOURNAL_STANDALONE_EPOCH
#define LINK_FLASH_JOURNAL_STANDALONE_EPOCH UINT32_C(1)
#endif
#define LINK_FLASH_TEST_STEPS UINT32_C(8)
#define LINK_FLASH_TEST_PASS UINT32_C(0x600d600d)

enum {
    LINK_FLASH_TEST_STATUS_RUNNING = 1U,
    LINK_FLASH_TEST_ERROR_JOURNAL = 0xe001U,
    LINK_FLASH_TEST_ERROR_NEXT = 0xe002U,
    LINK_FLASH_TEST_ERROR_WRITE = 0xe003U,
    LINK_FLASH_TEST_ERROR_RECOVERY = 0xe004U,
    LINK_FLASH_TEST_ERROR_CONTENT = 0xe005U
};

typedef struct {
    uint32_t magic;
    uint32_t epoch;
    uint32_t generation;
    uint32_t step;
    uint32_t completed;
    uint8_t payload[64U];
    uint32_t checksum;
} LinkFlashStandaloneRecord;

static const uint32_t test_pages[4U] = {
    UINT32_C(0x0807e000),
    UINT32_C(0x0807e800),
    UINT32_C(0x0807f000),
    UINT32_C(0x0807f800)
};

volatile uint32_t link_flash_test_status;
volatile uint32_t link_flash_test_generation;
volatile uint32_t link_flash_test_step;
volatile uint32_t link_flash_test_active_address;

static bool reserved_page(uint32_t address)
{
    size_t index;
    for (index = 0U; index < sizeof(test_pages) / sizeof(test_pages[0]); ++index)
        if (test_pages[index] == address) return true;
    return false;
}

static bool flash_read(
    void *context, uint32_t address, void *data, size_t length)
{
    (void)context;
    if (!reserved_page(address) || data == NULL ||
        length > LINK_FLASH_TEST_PAGE_SIZE) return false;
    memcpy(data, (const void *)(uintptr_t)address, length);
    return true;
}

static bool flash_erase(void *context, uint32_t address)
{
    FLASH_EraseInitTypeDef erase;
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    (void)context;
    if (!reserved_page(address)) return false;

    memset(&erase, 0, sizeof(erase));
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = address;
    erase.NbPages = 1U;

    HAL_FLASH_Unlock();
    status = HAL_FLASHEx_Erase(&erase, &page_error);
    HAL_FLASH_Lock();
    return status == HAL_OK;
}

static bool flash_program(
    void *context, uint32_t address, const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    size_t offset;

    (void)context;
    if (!reserved_page(address) || data == NULL ||
        length > LINK_FLASH_TEST_PAGE_SIZE) return false;

    HAL_FLASH_Unlock();
    for (offset = 0U; offset < length; offset += 2U) {
        uint16_t halfword = bytes[offset];
        if (offset + 1U < length)
            halfword |= (uint16_t)((uint16_t)bytes[offset + 1U] << 8U);
        else
            halfword |= UINT16_C(0xff00);
        if (HAL_FLASH_Program(
                FLASH_TYPEPROGRAM_HALFWORD,
                address + (uint32_t)offset,
                halfword) != HAL_OK) {
            HAL_FLASH_Lock();
            return false;
        }
    }
    HAL_FLASH_Lock();
    return true;
}

static uint32_t record_checksum(const LinkFlashStandaloneRecord *record)
{
    const uint8_t *bytes = (const uint8_t *)record;
    const size_t length = offsetof(LinkFlashStandaloneRecord, checksum);
    uint32_t hash = UINT32_C(2166136261);
    size_t index;

    for (index = 0U; index < length; ++index) {
        hash ^= bytes[index];
        hash *= UINT32_C(16777619);
    }
    return hash;
}

static bool record_valid(const void *record, void *context)
{
    const LinkFlashStandaloneRecord *candidate =
        (const LinkFlashStandaloneRecord *)record;
    (void)context;
    return candidate != NULL &&
           candidate->magic == LINK_FLASH_TEST_MAGIC &&
           candidate->checksum == record_checksum(candidate);
}

static uint32_t record_generation(const void *record, void *context)
{
    (void)context;
    return ((const LinkFlashStandaloneRecord *)record)->generation;
}

static void fill_record(
    LinkFlashStandaloneRecord *record,
    uint32_t generation,
    uint32_t step)
{
    size_t index;
    memset(record, 0, sizeof(*record));
    record->magic = LINK_FLASH_TEST_MAGIC;
    record->epoch = LINK_FLASH_JOURNAL_STANDALONE_EPOCH;
    record->generation = generation;
    record->step = step;
    record->completed = step == LINK_FLASH_TEST_STEPS ? 1U : 0U;
    for (index = 0U; index < sizeof(record->payload); ++index)
        record->payload[index] =
            (uint8_t)((generation + step + (uint32_t)index) & UINT32_C(0xff));
    record->checksum = record_checksum(record);
}

static bool content_matches(const LinkFlashStandaloneRecord *record)
{
    size_t index;
    if (!record_valid(record, NULL) ||
        record->epoch != LINK_FLASH_JOURNAL_STANDALONE_EPOCH ||
        record->step != LINK_FLASH_TEST_STEPS ||
        record->completed != 1U) return false;
    for (index = 0U; index < sizeof(record->payload); ++index) {
        const uint8_t expected =
            (uint8_t)((record->generation + record->step +
                       (uint32_t)index) & UINT32_C(0xff));
        if (record->payload[index] != expected) return false;
    }
    return true;
}

static uint32_t run_flash_journal_test(void)
{
    LinkFlashJournal journal;
    LinkFlashStandaloneRecord latest;
    LinkFlashStandaloneRecord scratch;
    LinkFlashStandaloneRecord candidate;
    LinkFlashStandaloneRecord verify;
    uint32_t active = 0U;
    uint32_t generation = 0U;
    uint32_t step = 1U;
    bool have_latest;

    memset(&journal, 0, sizeof(journal));
    journal.read = flash_read;
    journal.erase_page = flash_erase;
    journal.program = flash_program;
    journal.page_addresses = test_pages;
    journal.page_count = sizeof(test_pages) / sizeof(test_pages[0]);
    journal.page_size = LINK_FLASH_TEST_PAGE_SIZE;
    journal.record_size = sizeof(LinkFlashStandaloneRecord);

    if (!link_flash_journal_valid(&journal) ||
        link_flash_journal_pages_per_slot(&journal) != 1U ||
        link_flash_journal_slot_count(&journal) != 4U)
        return LINK_FLASH_TEST_ERROR_JOURNAL;

    have_latest = link_flash_journal_latest(
        &journal, &latest, &scratch,
        record_valid, record_generation, NULL, &active);

    if (have_latest) {
        generation = latest.generation;
        if (latest.epoch == LINK_FLASH_JOURNAL_STANDALONE_EPOCH) {
            if (latest.completed != 0U) {
                link_flash_test_generation = latest.generation;
                link_flash_test_step = latest.step;
                link_flash_test_active_address = active;
                return content_matches(&latest)
                    ? LINK_FLASH_TEST_PASS
                    : LINK_FLASH_TEST_ERROR_CONTENT;
            }
            step = latest.step + 1U;
            if (step > LINK_FLASH_TEST_STEPS)
                return LINK_FLASH_TEST_ERROR_CONTENT;
        }
    }

    for (; step <= LINK_FLASH_TEST_STEPS; ++step) {
        size_t slot;
        uint32_t next_address;
        if (!link_flash_journal_next(
                &journal, active, &slot, &next_address))
            return LINK_FLASH_TEST_ERROR_NEXT;
        ++generation;
        fill_record(&candidate, generation, step);
        if (!link_flash_journal_write(
                &journal, slot, &candidate, &verify,
                record_valid, NULL))
            return LINK_FLASH_TEST_ERROR_WRITE;
        active = next_address;
        link_flash_test_generation = generation;
        link_flash_test_step = step;
        link_flash_test_active_address = active;
    }

    if (!link_flash_journal_latest(
            &journal, &latest, &scratch,
            record_valid, record_generation, NULL, &active))
        return LINK_FLASH_TEST_ERROR_RECOVERY;

    link_flash_test_generation = latest.generation;
    link_flash_test_step = latest.step;
    link_flash_test_active_address = active;
    return content_matches(&latest)
        ? LINK_FLASH_TEST_PASS
        : LINK_FLASH_TEST_ERROR_CONTENT;
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    link_flash_test_status = LINK_FLASH_TEST_STATUS_RUNNING;
    link_flash_test_generation = 0U;
    link_flash_test_step = 0U;
    link_flash_test_active_address = 0U;

    link_flash_test_status = run_flash_journal_test();

    for (;;) {
        /*
         * PASS is 0x600D600D. Failure codes are 0xE001..0xE005.
         * Leave the MCU here so all result globals remain debugger-visible.
         */
    }
}
