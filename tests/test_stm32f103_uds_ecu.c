// SPDX-License-Identifier: GPL-3.0-or-later
#include "link-stm32f103-uds-ecu.h"

#include "link/aes_cmac.h"
#include "link/uds_services.h"
#include "link/uds_dtc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { \
    if (!(c)) { \
        fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c); \
        return 1; \
    } \
} while (0)

#define TEST_STATE_PAGE_C (LINK_STM32F103_UDS_STATE_PAGE_A - UINT32_C(0x1000))
#define TEST_STATE_PAGE_D (LINK_STM32F103_UDS_STATE_PAGE_A - UINT32_C(0x0800))

typedef struct {
    uint8_t page_a[LINK_STM32F103_FLASH_PAGE_BYTES];
    uint8_t page_b[LINK_STM32F103_FLASH_PAGE_BYTES];
    uint8_t page_c[LINK_STM32F103_FLASH_PAGE_BYTES];
    uint8_t page_d[LINK_STM32F103_FLASH_PAGE_BYTES];
    unsigned int erase_count[4U];
    uint32_t now_ms;
} TestPlatform;

static const uint32_t test_wear_pages[4U] = {
    TEST_STATE_PAGE_C,
    TEST_STATE_PAGE_D,
    LINK_STM32F103_UDS_STATE_PAGE_A,
    LINK_STM32F103_UDS_STATE_PAGE_B
};

static const uint8_t test_security_key[LINK_STM32F103_UDS_SECURITY_KEY_BYTES] = {
    0x2bU,0x7eU,0x15U,0x16U,0x28U,0xaeU,0xd2U,0xa6U,
    0xabU,0xf7U,0x15U,0x88U,0x09U,0xcfU,0x4fU,0x3cU
};

static int test_page_index(uint32_t address)
{
    if (address == TEST_STATE_PAGE_C) return 0;
    if (address == TEST_STATE_PAGE_D) return 1;
    if (address == LINK_STM32F103_UDS_STATE_PAGE_A) return 2;
    if (address == LINK_STM32F103_UDS_STATE_PAGE_B) return 3;
    return -1;
}

static uint8_t *test_page(TestPlatform *platform, uint32_t address)
{
    const int index = test_page_index(address);
    if (platform == NULL) return NULL;
    switch (index) {
    case 0: return platform->page_c;
    case 1: return platform->page_d;
    case 2: return platform->page_a;
    case 3: return platform->page_b;
    default: return NULL;
    }
}

static bool test_flash_read(
    void *context, uint32_t address, void *data, size_t length)
{
    TestPlatform *platform = (TestPlatform *)context;
    uint8_t *page = test_page(platform, address);
    if (page == NULL || data == NULL || length > sizeof(platform->page_a)) {
        return false;
    }
    memcpy(data, page, length);
    return true;
}

static bool test_flash_erase(void *context, uint32_t address)
{
    TestPlatform *platform = (TestPlatform *)context;
    uint8_t *page = test_page(platform, address);
    const int index = test_page_index(address);
    if (page == NULL || index < 0) return false;
    memset(page, 0xff, sizeof(platform->page_a));
    platform->erase_count[(size_t)index]++;
    return true;
}

static bool test_flash_program(
    void *context, uint32_t address, const void *data, size_t length)
{
    TestPlatform *platform = (TestPlatform *)context;
    uint8_t *page = test_page(platform, address);
    size_t index;
    const uint8_t *bytes = (const uint8_t *)data;

    if (page == NULL || data == NULL || length > sizeof(platform->page_a)) {
        return false;
    }
    for (index = 0U; index < length; ++index) {
        if ((uint8_t)(page[index] | bytes[index]) != page[index]) {
            return false;
        }
        page[index] &= bytes[index];
    }
    return true;
}

static uint32_t test_clock(void *context)
{
    return ((TestPlatform *)context)->now_ms;
}

static LinkStm32F103UdsEcuConfig test_config(TestPlatform *platform)
{
    LinkStm32F103UdsEcuConfig config;
    memset(&config, 0, sizeof(config));
    config.flash.context = platform;
    config.flash.read = test_flash_read;
    config.flash.erase_page = test_flash_erase;
    config.flash.program = test_flash_program;
    config.flash.page_a_address = LINK_STM32F103_UDS_STATE_PAGE_A;
    config.flash.page_b_address = LINK_STM32F103_UDS_STATE_PAGE_B;
    config.flash.page_size = LINK_STM32F103_FLASH_PAGE_BYTES;
    config.clock_ms = test_clock;
    config.clock_context = platform;
    config.security_key = test_security_key;
    return config;
}

static int expect_positive(
    LinkStm32F103UdsEcu *ecu,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    LinkUdsServerResult result = link_stm32f103_uds_ecu_handle(
        ecu, NULL, request, request_length,
        response, response_capacity, response_length);
    if (result != LINK_UDS_SERVER_RESULT_POSITIVE ||
        *response_length == 0U ||
        response[0] != (uint8_t)(request[0] + 0x40U)) {
        fprintf(stderr, "service 0x%02x was not positive\n", request[0]);
        return 1;
    }
    return 0;
}

static int enter_programming_and_unlock(
    LinkStm32F103UdsEcu *ecu,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    const uint8_t extended[] = {0x10U,0x03U};
    const uint8_t programming[] = {0x10U,0x02U};
    const uint8_t seed_request[] = {0x27U,0x01U};
    uint8_t key_request[18U] = {0x27U,0x02U};
    uint8_t tag[LINK_AES_CMAC_TAG_BYTES];

    CHECK(expect_positive(
        ecu, extended, sizeof(extended),
        response, response_capacity, response_length) == 0);
    CHECK(expect_positive(
        ecu, programming, sizeof(programming),
        response, response_capacity, response_length) == 0);
    CHECK(expect_positive(
        ecu, seed_request, sizeof(seed_request),
        response, response_capacity, response_length) == 0);
    CHECK(*response_length == 18U);
    CHECK(link_aes_cmac_128(
        test_security_key, response + 2U,
        LINK_STM32F103_UDS_SECURITY_SEED_BYTES, tag));
    memcpy(key_request + 2U, tag, sizeof(tag));
    CHECK(expect_positive(
        ecu, key_request, sizeof(key_request),
        response, response_capacity, response_length) == 0);
    CHECK(link_uds_server_active_security_level(
        link_stm32f103_uds_ecu_server(ecu)) == 1U);
    return 0;
}

static int test_all_27_service_surfaces(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    uint8_t response[512U];
    size_t response_length = 0U;
    const uint8_t session[] = {0x10U,0x03U};
    const uint8_t reset[] = {0x11U,0x01U};
    const uint8_t clear[] = {0x14U,0xffU,0xffU,0xffU};
    const uint8_t read_dtc[] = {0x19U,0x02U,0xffU};
    const uint8_t read_did[] = {0x22U,0xf1U,0x90U};
    const uint8_t read_memory[] = {0x23U,0x11U,0x00U,0x04U};
    const uint8_t scaling[] = {0x24U,0xf1U,0xa0U};
    const uint8_t communication[] = {0x28U,0x00U,0x03U};
    const uint8_t authentication[] = {0x29U,0x00U};
    const uint8_t periodic[] = {0x2aU,0x01U,0x01U};
    const uint8_t dynamic_did[] = {
        0x2cU,0x01U,0xf2U,0x00U,0xf1U,0x90U,0x01U,0x02U
    };
    uint8_t write_did[3U + LINK_STM32F103_UDS_DID_BYTES];
    const uint8_t io_control[] = {0x2fU,0xf1U,0xa0U,0x00U};
    const uint8_t routine[] = {0x31U,0x01U,0x02U,0x01U};
    const uint8_t download[] = {0x34U,0x00U,0x11U,0x00U,0x04U};
    const uint8_t transfer_data[] = {0x36U,0x01U,0x11U,0x22U,0x33U,0x44U};
    const uint8_t transfer_exit[] = {0x37U};
    const uint8_t upload[] = {0x35U,0x00U,0x11U,0x00U,0x04U};
    const uint8_t upload_data[] = {0x36U,0x01U};
    const uint8_t file_transfer[] = {0x38U,0x01U};
    const uint8_t write_memory[] = {
        0x3dU,0x11U,0x08U,0x04U,0xaaU,0xbbU,0xccU,0xddU
    };
    const uint8_t tester_present[] = {0x3eU,0x00U};
    const uint8_t timing[] = {0x83U,0x03U};
    const uint8_t secured[] = {0x84U,0x01U,0x02U,0x03U};
    const uint8_t dtc_setting[] = {0x85U,0x01U};
    const uint8_t response_event[] = {0x86U,0x00U};
    const uint8_t link_control[] = {0x87U,0x01U};
    const uint8_t seed_request[] = {0x27U,0x01U};

    memset(&platform, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    platform.now_ms = 1U;
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));
    CHECK(link_uds_standard_service_count() == 27U);

    /* Built-in/session/read-only surfaces are usable before unlocking. */
    CHECK(expect_positive(
        &ecu, session, sizeof(session),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_dtc, sizeof(read_dtc),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_did, sizeof(read_did),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_memory, sizeof(read_memory),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, scaling, sizeof(scaling),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, periodic, sizeof(periodic),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, authentication, sizeof(authentication),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, tester_present, sizeof(tester_present),
        response, sizeof(response), &response_length) == 0);

    /* 0x27 is present as a real seed/key facility, not a stub. */
    CHECK(expect_positive(
        &ecu, seed_request, sizeof(seed_request),
        response, sizeof(response), &response_length) == 0);

    /*
     * Re-enter from Default through Extended -> Programming because the
     * Authentication de-authentication request deliberately reset security.
     */
    link_uds_server_reset_session(link_stm32f103_uds_ecu_server(&ecu));
    CHECK(enter_programming_and_unlock(
        &ecu, response, sizeof(response), &response_length) == 0);

    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, communication, sizeof(communication),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, dynamic_did, sizeof(dynamic_did),
        response, sizeof(response), &response_length) == 0);

    memset(write_did, 0, sizeof(write_did));
    write_did[0] = 0x2eU;
    write_did[1] = 0xf1U;
    write_did[2] = 0xa0U;
    memcpy(write_did + 3U, "PERSISTENT-DID01", LINK_STM32F103_UDS_DID_BYTES);
    CHECK(expect_positive(
        &ecu, write_did, sizeof(write_did),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, io_control, sizeof(io_control),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, routine, sizeof(routine),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, timing, sizeof(timing),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, secured, sizeof(secured),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, dtc_setting, sizeof(dtc_setting),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, response_event, sizeof(response_event),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, link_control, sizeof(link_control),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, write_memory, sizeof(write_memory),
        response, sizeof(response), &response_length) == 0);

    CHECK(expect_positive(
        &ecu, download, sizeof(download),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, transfer_data, sizeof(transfer_data),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, transfer_exit, sizeof(transfer_exit),
        response, sizeof(response), &response_length) == 0);

    CHECK(expect_positive(
        &ecu, upload, sizeof(upload),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, upload_data, sizeof(upload_data),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 6U);
    CHECK(expect_positive(
        &ecu, transfer_exit, sizeof(transfer_exit),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, file_transfer, sizeof(file_transfer),
        response, sizeof(response), &response_length) == 0);

    /*
     * ECUReset is the remaining catalogue service and is intentionally last
     * because it resets the session/security state.
     */
    CHECK(expect_positive(
        &ecu, reset, sizeof(reset),
        response, sizeof(response), &response_length) == 0);

    return 0;
}

static int test_persistence_and_dtc_clear(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu first;
    LinkStm32F103UdsEcu second;
    LinkStm32F103UdsEcuConfig config;
    uint8_t response[512U];
    size_t response_length = 0U;
    const uint8_t clear_one[] = {0x14U,0x12U,0x34U,0x56U};
    const uint8_t read_did[] = {0x22U,0xf1U,0xa0U};
    const uint8_t read_memory[] = {0x23U,0x11U,0x08U,0x04U};
    const uint8_t write_memory[] = {
        0x3dU,0x11U,0x08U,0x04U,0xdeU,0xadU,0xbeU,0xefU
    };
    uint8_t write_did[3U + LINK_STM32F103_UDS_DID_BYTES];

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    platform.now_ms = 100U;
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&first, &config));
    CHECK(enter_programming_and_unlock(
        &first, response, sizeof(response), &response_length) == 0);

    memset(write_did, 0, sizeof(write_did));
    write_did[0] = 0x2eU;
    write_did[1] = 0xf1U;
    write_did[2] = 0xa0U;
    memcpy(write_did + 3U, "FLASH-PERSIST-001", LINK_STM32F103_UDS_DID_BYTES);
    CHECK(expect_positive(
        &first, write_did, sizeof(write_did),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &first, write_memory, sizeof(write_memory),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &first, clear_one, sizeof(clear_one),
        response, sizeof(response), &response_length) == 0);

    /* Reboot/re-initialise from the same two flash pages. */
    platform.now_ms += 100U;
    CHECK(link_stm32f103_uds_ecu_init(&second, &config));
    CHECK(expect_positive(
        &second, read_did, sizeof(read_did),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 3U + LINK_STM32F103_UDS_DID_BYTES);
    CHECK(memcmp(
        response + 3U, "FLASH-PERSIST-001",
        LINK_STM32F103_UDS_DID_BYTES) == 0);

    CHECK(expect_positive(
        &second, read_memory, sizeof(read_memory),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 5U);
    CHECK(response[1] == 0xdeU && response[2] == 0xadU &&
          response[3] == 0xbeU && response[4] == 0xefU);
    CHECK(second.state.dtc_status[0U] ==
        (LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR |
         LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE));
    CHECK(second.state.dtc_fdc[0U] == 0U);
    CHECK(second.state.dtc_permanent[0U] == 0U);
    CHECK(second.state.generation > 1U);
    return 0;
}

static int test_n_page_wear_level_rotation(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcu reloaded;
    LinkStm32F103UdsEcuConfig config;
    unsigned int i;

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    memset(platform.page_c, 0xff, sizeof(platform.page_c));
    memset(platform.page_d, 0xff, sizeof(platform.page_d));
    platform.now_ms = 50U;

    config = test_config(&platform);
    config.flash.page_addresses = test_wear_pages;
    config.flash.page_count =
        sizeof(test_wear_pages) / sizeof(test_wear_pages[0]);

    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));
    CHECK(ecu.active_page == TEST_STATE_PAGE_C);
    CHECK(ecu.state.generation == 1U);

    for (i = 0U; i < 7U; ++i) {
        CHECK(link_stm32f103_uds_ecu_flush(&ecu));
    }

    CHECK(ecu.state.generation == 8U);
    CHECK(ecu.active_page == LINK_STM32F103_UDS_STATE_PAGE_B);
    CHECK(platform.erase_count[0U] == 2U);
    CHECK(platform.erase_count[1U] == 2U);
    CHECK(platform.erase_count[2U] == 2U);
    CHECK(platform.erase_count[3U] == 2U);

    CHECK(link_stm32f103_uds_ecu_init(&reloaded, &config));
    CHECK(reloaded.state.generation == ecu.state.generation);
    CHECK(reloaded.active_page == ecu.active_page);
    return 0;
}

static int test_multi_page_persistent_state_slots(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcu reloaded;
    LinkStm32F103UdsEcuConfig config;

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    memset(platform.page_c, 0xff, sizeof(platform.page_c));
    memset(platform.page_d, 0xff, sizeof(platform.page_d));
    platform.now_ms = 75U;

    config = test_config(&platform);
    config.flash.page_addresses = test_wear_pages;
    config.flash.page_count =
        sizeof(test_wear_pages) / sizeof(test_wear_pages[0]);
    config.flash.page_size = UINT32_C(256);

    CHECK(sizeof(LinkStm32F103PersistentState) > config.flash.page_size);
    CHECK(sizeof(LinkStm32F103PersistentState) <=
          (size_t)config.flash.page_size * 2U);

    /*
     * Four physical pages now form two crash-consistent two-page slots.
     * Initial publication uses C+D, then the next generation uses A+B.
     */
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));
    CHECK(ecu.active_page == TEST_STATE_PAGE_C);
    CHECK(ecu.state.generation == 1U);
    CHECK(platform.erase_count[0U] == 1U);
    CHECK(platform.erase_count[1U] == 1U);
    CHECK(platform.erase_count[2U] == 0U);
    CHECK(platform.erase_count[3U] == 0U);

    memset(ecu.state.sandbox, 0x5a, sizeof(ecu.state.sandbox));
    CHECK(link_stm32f103_uds_ecu_flush(&ecu));
    CHECK(ecu.active_page == LINK_STM32F103_UDS_STATE_PAGE_A);
    CHECK(ecu.state.generation == 2U);
    CHECK(platform.erase_count[2U] == 1U);
    CHECK(platform.erase_count[3U] == 1U);

    CHECK(link_stm32f103_uds_ecu_init(&reloaded, &config));
    CHECK(reloaded.active_page == LINK_STM32F103_UDS_STATE_PAGE_A);
    CHECK(reloaded.state.generation == 2U);
    CHECK(reloaded.state.sandbox[0U] == 0x5aU);
    CHECK(reloaded.state.sandbox[sizeof(reloaded.state.sandbox) - 1U] == 0x5aU);

    /*
     * Corrupt the second page of the newest slot. Its whole-state CRC must
     * reject that torn/corrupt generation and recover the previous C+D slot.
     */
    platform.page_b[0U] ^= UINT8_C(0x01);
    CHECK(link_stm32f103_uds_ecu_init(&reloaded, &config));
    CHECK(reloaded.active_page == TEST_STATE_PAGE_C);
    CHECK(reloaded.state.generation == 1U);
    return 0;
}

static int test_issue37_clear_sequence_and_status_masks(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    LinkUdsDtcInformationResponse decoded;
    uint8_t response[64U];
    size_t response_length = 0U;
    const uint8_t clear_all[] = {0x14U,0xffU,0xffU,0xffU};
    const uint8_t count_all[] = {0x19U,0x01U,0xffU};
    const uint8_t count_faults[] = {0x19U,0x01U,0x0dU};

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));

    /*
     * LINK #40 corrected the reference policy: ISO 14229 permits 0x14 in the
     * default session, so the all-group clear must succeed without an invented
     * SecurityAccess prerequisite.
     */
    CHECK(expect_positive(
        &ecu, clear_all, sizeof(clear_all),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 1U && response[0] == 0x54U);

    CHECK(expect_positive(
        &ecu, count_faults, sizeof(count_faults),
        response, sizeof(response), &response_length) == 0);
    CHECK(link_uds_decode_read_dtc_information_response(
        LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK,
        response, response_length, &decoded) == LINK_UDS_RESULT_OK);
    CHECK(decoded.dtc_count_available && decoded.dtc_count == 0U);

    /*
     * FF still matches the not-completed-since-clear and
     * not-completed-this-cycle bits deliberately set by a successful clear.
     */
    CHECK(expect_positive(
        &ecu, count_all, sizeof(count_all),
        response, sizeof(response), &response_length) == 0);
    CHECK(link_uds_decode_read_dtc_information_response(
        LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK,
        response, response_length, &decoded) == LINK_UDS_RESULT_OK);
    CHECK(decoded.dtc_count_available && decoded.dtc_count == 3U);
    return 0;
}

static int test_issue38_dtc_lifecycle_engine(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    uint8_t response[64U];
    size_t response_length = 0U;
    const uint8_t clear_all[] = {0x14U,0xffU,0xffU,0xffU};
    const uint8_t read_fdc[] = {0x19U,0x14U};
    const uint32_t code = UINT32_C(0x123456);
    uint32_t persisted_generation;

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));
    CHECK(enter_programming_and_unlock(
        &ecu, response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, clear_all, sizeof(clear_all),
        response, sizeof(response), &response_length) == 0);

    CHECK(ecu.dtc_details[0U].functional_group_identifier == 0x33U);
    CHECK(ecu.dtc_lifecycle[0U].status == 0x50U);
    persisted_generation = ecu.state.generation;

    /*
     * A partial monitor failure is prefailed only. It is visible through
     * reportDTCFaultDetectionCounter as +64 but has not reached testFailed.
     */
    CHECK(link_stm32f103_uds_ecu_begin_operation_cycle(&ecu));
    CHECK(link_stm32f103_uds_ecu_report_dtc_test(
        &ecu, code, LINK_UDS_DTC_TEST_FAILED));
    CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == 64);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_TEST_FAILED) == 0U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_PENDING_DTC) == 0U);
    CHECK(ecu.state.generation == persisted_generation);

    CHECK(expect_positive(
        &ecu, read_fdc, sizeof(read_fdc),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 6U);
    CHECK(response[0] == 0x59U && response[1] == 0x14U);
    CHECK(response[2] == 0x12U && response[3] == 0x34U &&
          response[4] == 0x56U && response[5] == 64U);

    /* The next failed execution in the same cycle reaches +127. */
    CHECK(link_stm32f103_uds_ecu_report_dtc_test(
        &ecu, code, LINK_UDS_DTC_TEST_FAILED));
    CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == 127);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_TEST_FAILED) != 0U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_PENDING_DTC) != 0U);
    CHECK(ecu.state.generation == persisted_generation);
    CHECK(link_stm32f103_uds_ecu_end_operation_cycle(&ecu));
    CHECK(ecu.state.generation > persisted_generation);
    persisted_generation = ecu.state.generation;
    CHECK(ecu.dtc_lifecycle[0U].failure_cycle_count == 1U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);

    /* +127 is fully failed and is not a reportable prefailed FDC. */
    CHECK(expect_positive(
        &ecu, read_fdc, sizeof(read_fdc),
        response, sizeof(response), &response_length) == 0);
    CHECK(response_length == 2U);
    CHECK(response[0] == 0x59U && response[1] == 0x14U);

    /*
     * FDC resets to zero at the next operation-cycle boundary. A second full
     * failed cycle confirms the DTC under the reference two-cycle policy.
     */
    CHECK(link_stm32f103_uds_ecu_begin_operation_cycle(&ecu));
    CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == 0);
    CHECK(link_stm32f103_uds_ecu_report_dtc_test(
        &ecu, code, LINK_UDS_DTC_TEST_FAILED));
    CHECK(link_stm32f103_uds_ecu_report_dtc_test(
        &ecu, code, LINK_UDS_DTC_TEST_FAILED));
    CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == 127);
    CHECK(link_stm32f103_uds_ecu_end_operation_cycle(&ecu));
    CHECK(ecu.dtc_lifecycle[0U].failure_cycle_count == 2U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U);

    /*
     * Three fully passed cycles age the confirmed DTC out. Each cycle needs
     * two passed monitor executions to reach the scaled -128 pass threshold.
     */
    for (unsigned int cycle = 0U; cycle < 3U; ++cycle) {
        CHECK(link_stm32f103_uds_ecu_begin_operation_cycle(&ecu));
        CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == 0);
        CHECK(link_stm32f103_uds_ecu_report_dtc_test(
            &ecu, code, LINK_UDS_DTC_TEST_PASSED));
        CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == -64);
        CHECK(link_stm32f103_uds_ecu_report_dtc_test(
            &ecu, code, LINK_UDS_DTC_TEST_PASSED));
        CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == -128);
        CHECK(link_stm32f103_uds_ecu_end_operation_cycle(&ecu));
    }

    CHECK(ecu.dtc_lifecycle[0U].aging_counter == 3U);
    CHECK(ecu.dtc_lifecycle[0U].fault_detection_counter == -128);
    CHECK(ecu.state.extended[0U][0U] == 3U);
    CHECK(ecu.state.extended[0U][1U] == 0U);
    CHECK(ecu.state.extended[0U][2U] == UINT8_C(0x80));
    CHECK(ecu.state.extended[0U][3U] == 0x33U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_PENDING_DTC) == 0U);
    CHECK((ecu.dtc_lifecycle[0U].status &
           LINK_UDS_DTC_STATUS_CONFIRMED_DTC) == 0U);

    return 0;
}

static int test_issue40_clear_and_read_dtc_policy(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    uint8_t response[64U];
    size_t response_length = 0U;
    const uint8_t clear[] = {0x14U,0xffU,0xffU,0xffU};
    const uint8_t read_dtc[] = {0x19U,0x01U,0x01U};
    const uint8_t extended[] = {0x10U,0x03U};
    const uint8_t programming[] = {0x10U,0x02U};
    const uint8_t safety[] = {0x10U,0x04U};

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));

    /* DefaultSession, security level 0. */
    CHECK(expect_positive(
        &ecu, read_dtc, sizeof(read_dtc),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);

    /* ExtendedDiagnosticSession, still security level 0. */
    CHECK(expect_positive(
        &ecu, extended, sizeof(extended),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_dtc, sizeof(read_dtc),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);

    /* SafetySystemDiagnosticSession, security level 0. */
    link_uds_server_reset_session(link_stm32f103_uds_ecu_server(&ecu));
    CHECK(expect_positive(
        &ecu, safety, sizeof(safety),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_dtc, sizeof(read_dtc),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);

    /*
     * ProgrammingSession with SecurityAccess level 1 proves 0x14 remains
     * available after unlocking as well; the ALL security mask is not merely
     * an alias for the unsecured level.
     */
    link_uds_server_reset_session(link_stm32f103_uds_ecu_server(&ecu));
    CHECK(expect_positive(
        &ecu, extended, sizeof(extended),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, programming, sizeof(programming),
        response, sizeof(response), &response_length) == 0);
    link_uds_server_reset_session(link_stm32f103_uds_ecu_server(&ecu));
    CHECK(enter_programming_and_unlock(
        &ecu, response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, read_dtc, sizeof(read_dtc),
        response, sizeof(response), &response_length) == 0);
    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);
    return 0;
}

static int test_issue41_status_mask_availability(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    LinkUdsDtcInformationResponse decoded;
    uint8_t response[64U];
    size_t response_length = 0U;
    const uint8_t warning_only[] = {0x19U,0x01U,0x80U};
    const uint8_t all_statuses[] = {0x19U,0x01U,0xffU};

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));

    CHECK(ecu.dtc_store.status_availability_mask == UINT8_C(0x7f));

    CHECK(expect_positive(
        &ecu, warning_only, sizeof(warning_only),
        response, sizeof(response), &response_length) == 0);
    CHECK(link_uds_decode_read_dtc_information_response(
        LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK,
        response, response_length, &decoded) == LINK_UDS_RESULT_OK);
    CHECK(decoded.status_availability_mask_available);
    CHECK(decoded.status_availability_mask == UINT8_C(0x7f));
    CHECK(decoded.dtc_count_available && decoded.dtc_count == 0U);

    CHECK(expect_positive(
        &ecu, all_statuses, sizeof(all_statuses),
        response, sizeof(response), &response_length) == 0);
    CHECK(link_uds_decode_read_dtc_information_response(
        LINK_UDS_DTC_REPORT_NUMBER_BY_STATUS_MASK,
        response, response_length, &decoded) == LINK_UDS_RESULT_OK);
    CHECK(decoded.status_availability_mask == UINT8_C(0x7f));
    CHECK(decoded.dtc_count_available && decoded.dtc_count == 3U);
    return 0;
}

static int test_policy_blocks_unsafe_default_session(void)
{
    TestPlatform platform;
    LinkStm32F103UdsEcu ecu;
    LinkStm32F103UdsEcuConfig config;
    uint8_t response[64U];
    size_t response_length = 0U;
    const uint8_t clear[] = {0x14U,0xffU,0xffU,0xffU};
    const uint8_t write_memory[] = {
        0x3dU,0x11U,0x00U,0x01U,0x55U
    };

    memset(&platform, 0, sizeof(platform));
    memset(platform.page_a, 0xff, sizeof(platform.page_a));
    memset(platform.page_b, 0xff, sizeof(platform.page_b));
    config = test_config(&platform);
    CHECK(link_stm32f103_uds_ecu_init(&ecu, &config));

    CHECK(expect_positive(
        &ecu, clear, sizeof(clear),
        response, sizeof(response), &response_length) == 0);

    CHECK(link_stm32f103_uds_ecu_handle(
        &ecu, NULL, write_memory, sizeof(write_memory),
        response, sizeof(response), &response_length) ==
        LINK_UDS_SERVER_RESULT_NEGATIVE);
    CHECK(response[2] == LINK_UDS_NRC_SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION);
    return 0;
}

int main(void)
{
    CHECK(test_all_27_service_surfaces() == 0);
    CHECK(test_persistence_and_dtc_clear() == 0);
    CHECK(test_n_page_wear_level_rotation() == 0);
    CHECK(test_multi_page_persistent_state_slots() == 0);
    CHECK(test_issue37_clear_sequence_and_status_masks() == 0);
    CHECK(test_issue38_dtc_lifecycle_engine() == 0);
    CHECK(test_issue40_clear_and_read_dtc_policy() == 0);
    CHECK(test_issue41_status_mask_availability() == 0);
    CHECK(test_policy_blocks_unsafe_default_session() == 0);
    puts("STM32F103 complete UDS ECU tests passed");
    return EXIT_SUCCESS;
}
