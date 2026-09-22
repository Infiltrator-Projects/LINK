// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/uds_bootloader.h"

#include <stdio.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) {     fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #c);     return 1; } } while (0)

typedef struct {
    uint32_t installed_version;
    uint8_t slot;
    uint8_t data[64U];
    size_t written;
    unsigned int begin_calls;
    unsigned int finish_calls;
    unsigned int integrity_calls;
    unsigned int authenticity_calls;
    unsigned int stage_calls;
    unsigned int secure_boot_calls;
    unsigned int commit_calls;
    unsigned int abort_calls;
    bool integrity_ok;
    bool authenticity_ok;
    bool secure_boot_ok;
} FakeBackend;

static bool fake_read_version(void *context, uint32_t *version)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || version == NULL) return false;
    *version = fake->installed_version;
    return true;
}

static bool fake_select_slot(void *context, uint8_t *slot)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot == NULL) return false;
    *slot = fake->slot;
    return true;
}

static bool fake_begin(
    void *context, uint8_t slot, uint32_t version, uint64_t image_size)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot || version == 0U ||
        image_size > sizeof(fake->data)) return false;
    fake->written = 0U;
    fake->begin_calls++;
    return true;
}

static bool fake_write(
    void *context, uint8_t slot, uint64_t offset,
    const uint8_t *data, size_t length)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || data == NULL || slot != fake->slot ||
        offset != (uint64_t)fake->written ||
        length > sizeof(fake->data) - fake->written) return false;
    memcpy(fake->data + fake->written, data, length);
    fake->written += length;
    return true;
}

static bool fake_finish(void *context, uint8_t slot, uint64_t image_size)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot ||
        image_size != (uint64_t)fake->written) return false;
    fake->finish_calls++;
    return true;
}

static bool fake_integrity(
    void *context, uint8_t slot, uint32_t version, uint64_t image_size)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot || version == 0U ||
        image_size != (uint64_t)fake->written) return false;
    fake->integrity_calls++;
    return fake->integrity_ok;
}

static bool fake_authenticity(
    void *context, uint8_t slot, uint32_t version, uint64_t image_size)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot || version == 0U ||
        image_size != (uint64_t)fake->written) return false;
    fake->authenticity_calls++;
    return fake->authenticity_ok;
}

static bool fake_stage(void *context, uint8_t slot, uint32_t version)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot || version == 0U) return false;
    fake->stage_calls++;
    return true;
}

static bool fake_secure_boot(void *context, uint8_t slot, uint32_t version)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || slot != fake->slot || version == 0U) return false;
    fake->secure_boot_calls++;
    return fake->secure_boot_ok;
}

static bool fake_commit(void *context, uint32_t version)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake == NULL || version <= fake->installed_version) return false;
    fake->installed_version = version;
    fake->commit_calls++;
    return true;
}

static void fake_abort(void *context, uint8_t slot)
{
    FakeBackend *fake = (FakeBackend *)context;
    if (fake != NULL && slot == fake->slot) fake->abort_calls++;
}

static LinkUdsBootloaderConfig make_config(FakeBackend *fake)
{
    LinkUdsBootloaderConfig config = LINK_UDS_BOOTLOADER_CONFIG_INIT;
    config.allow_programming = true;
    config.backend.read_monotonic_version = fake_read_version;
    config.backend.select_inactive_slot = fake_select_slot;
    config.backend.begin_image = fake_begin;
    config.backend.write_block = fake_write;
    config.backend.finish_image = fake_finish;
    config.backend.verify_integrity = fake_integrity;
    config.backend.verify_authenticity = fake_authenticity;
    config.backend.stage_inactive_slot = fake_stage;
    config.backend.secure_boot_validate_candidate = fake_secure_boot;
    config.backend.commit_monotonic_version = fake_commit;
    config.backend.abort_image = fake_abort;
    config.backend.context = fake;
    return config;
}

static int test_fail_closed_default(void)
{
    LinkUdsBootloader bootloader;
    LinkUdsBootloaderConfig config = LINK_UDS_BOOTLOADER_CONFIG_INIT;

    CHECK(link_uds_bootloader_init(&bootloader, &config));
    CHECK(bootloader.state == LINK_UDS_BOOTLOADER_STATE_DISARMED);
    CHECK(link_uds_bootloader_arm(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_LOCKED);

    config.allow_programming = true;
    CHECK(link_uds_bootloader_init(&bootloader, &config));
    CHECK(link_uds_bootloader_arm(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_LOCKED);
    return 0;
}

static int test_complete_sequence(void)
{
    static const uint8_t first[] = {1U,2U,3U};
    static const uint8_t second[] = {4U,5U,6U};
    FakeBackend fake;
    LinkUdsBootloaderConfig config;
    LinkUdsBootloader bootloader;

    memset(&fake, 0, sizeof(fake));
    fake.installed_version = 7U;
    fake.slot = 1U;
    fake.integrity_ok = true;
    fake.authenticity_ok = true;
    fake.secure_boot_ok = true;
    config = make_config(&fake);

    CHECK(link_uds_bootloader_init(&bootloader, &config));
    CHECK(link_uds_bootloader_arm(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(bootloader.installed_version == 7U);

    CHECK(link_uds_bootloader_request_download(&bootloader, 8U, 6U) ==
          LINK_UDS_BOOTLOADER_RESULT_BAD_STATE);
    CHECK(link_uds_bootloader_enter_programming_session(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_request_download(&bootloader, 8U, 6U) ==
          LINK_UDS_BOOTLOADER_RESULT_BAD_STATE);
    CHECK(link_uds_bootloader_grant_security(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_set_dtc_recording_disabled(
              &bootloader, true) == LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_set_communication_disabled(
              &bootloader, true) == LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(bootloader.state == LINK_UDS_BOOTLOADER_STATE_QUIESCED);

    CHECK(link_uds_bootloader_request_download(&bootloader, 7U, 6U) ==
          LINK_UDS_BOOTLOADER_RESULT_ROLLBACK_REJECTED);
    CHECK(link_uds_bootloader_request_download(&bootloader, 8U, 6U) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(fake.begin_calls == 1U);
    CHECK(bootloader.candidate_slot == 1U);

    CHECK(link_uds_bootloader_transfer_data(
              &bootloader, 2U, first, sizeof(first)) ==
          LINK_UDS_BOOTLOADER_RESULT_SEQUENCE_ERROR);
    CHECK(link_uds_bootloader_transfer_data(
              &bootloader, 1U, first, sizeof(first)) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_request_transfer_exit(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_SIZE_MISMATCH);
    CHECK(link_uds_bootloader_transfer_data(
              &bootloader, 2U, second, sizeof(second)) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_request_transfer_exit(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(fake.finish_calls == 1U);

    CHECK(link_uds_bootloader_check_memory(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(fake.integrity_calls == 1U && fake.authenticity_calls == 1U);
    CHECK(link_uds_bootloader_stage_for_reset(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(fake.stage_calls == 1U && fake.secure_boot_calls == 1U);
    CHECK(link_uds_bootloader_confirm_boot(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(fake.commit_calls == 1U && fake.installed_version == 8U);
    CHECK(bootloader.state == LINK_UDS_BOOTLOADER_STATE_COMPLETE);
    CHECK(memcmp(fake.data, "\x01\x02\x03\x04\x05\x06", 6U) == 0);
    return 0;
}

static int test_verification_failure_aborts_candidate(void)
{
    static const uint8_t image[] = {0xaaU,0xbbU};
    FakeBackend fake;
    LinkUdsBootloaderConfig config;
    LinkUdsBootloader bootloader;

    memset(&fake, 0, sizeof(fake));
    fake.installed_version = 4U;
    fake.slot = 0U;
    fake.integrity_ok = true;
    fake.authenticity_ok = false;
    fake.secure_boot_ok = true;
    config = make_config(&fake);

    CHECK(link_uds_bootloader_init(&bootloader, &config));
    CHECK(link_uds_bootloader_arm(&bootloader) == LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_enter_programming_session(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_grant_security(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_set_dtc_recording_disabled(&bootloader, true) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_set_communication_disabled(&bootloader, true) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_request_download(&bootloader, 5U, sizeof(image)) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_transfer_data(
              &bootloader, 1U, image, sizeof(image)) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_request_transfer_exit(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_OK);
    CHECK(link_uds_bootloader_check_memory(&bootloader) ==
          LINK_UDS_BOOTLOADER_RESULT_AUTHENTICITY_FAILED);
    CHECK(bootloader.state == LINK_UDS_BOOTLOADER_STATE_FAILED);
    CHECK(fake.abort_calls == 1U);
    CHECK(fake.stage_calls == 0U && fake.commit_calls == 0U);
    return 0;
}

int main(void)
{
    if (test_fail_closed_default() != 0) return 1;
    if (test_complete_sequence() != 0) return 1;
    if (test_verification_failure_aborts_candidate() != 0) return 1;
    puts("UDS bootloader policy tests passed");
    return 0;
}
