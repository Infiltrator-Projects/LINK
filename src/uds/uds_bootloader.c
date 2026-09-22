// SPDX-License-Identifier: GPL-3.0-or-later
/** @file uds_bootloader.c @brief Fail-closed UDS OTA programming state machine. */
#include "link/uds_bootloader.h"

#include <string.h>

static bool backend_complete(const LinkUdsBootloaderConfig *config)
{
    const LinkUdsBootloaderBackend *backend;
    if (config == NULL) return false;
    backend = &config->backend;
    return backend->read_monotonic_version != NULL &&
           backend->select_inactive_slot != NULL &&
           backend->begin_image != NULL &&
           backend->write_block != NULL &&
           backend->finish_image != NULL &&
           backend->verify_integrity != NULL &&
           (!config->require_authenticity ||
            backend->verify_authenticity != NULL) &&
           backend->stage_inactive_slot != NULL &&
           (!config->require_secure_boot_validation ||
            backend->secure_boot_validate_candidate != NULL) &&
           backend->commit_monotonic_version != NULL;
}

static void backend_abort(LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return;
    if (bootloader->candidate_slot != LINK_UDS_BOOTLOADER_SLOT_NONE &&
        bootloader->config.backend.abort_image != NULL) {
        bootloader->config.backend.abort_image(
            bootloader->config.backend.context,
            bootloader->candidate_slot);
    }
}

static LinkUdsBootloaderResult fail(
    LinkUdsBootloader *bootloader, LinkUdsBootloaderResult result)
{
    backend_abort(bootloader);
    if (bootloader != NULL) bootloader->state = LINK_UDS_BOOTLOADER_STATE_FAILED;
    return result;
}

static bool prerequisites_met(const LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return false;
    if (bootloader->config.require_security_access &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_QUIESCED) {
        return false;
    }
    if (bootloader->config.require_quiesce &&
        (!bootloader->dtc_recording_disabled ||
         !bootloader->communication_disabled)) {
        return false;
    }
    return bootloader->state == LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION ||
           bootloader->state == LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED ||
           bootloader->state == LINK_UDS_BOOTLOADER_STATE_QUIESCED;
}

static void update_quiesced_state(LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return;
    if (bootloader->dtc_recording_disabled &&
        bootloader->communication_disabled &&
        (bootloader->state == LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION ||
         bootloader->state == LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED)) {
        bootloader->state = LINK_UDS_BOOTLOADER_STATE_QUIESCED;
    }
}

bool link_uds_bootloader_init(
    LinkUdsBootloader *bootloader, const LinkUdsBootloaderConfig *config)
{
    if (bootloader == NULL || config == NULL) return false;
    memset(bootloader, 0, sizeof(*bootloader));
    bootloader->config = *config;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_DISARMED;
    bootloader->candidate_slot = LINK_UDS_BOOTLOADER_SLOT_NONE;
    return true;
}

LinkUdsBootloaderResult link_uds_bootloader_arm(
    LinkUdsBootloader *bootloader)
{
    uint32_t version = 0U;
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_DISARMED)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (!bootloader->config.allow_programming || !backend_complete(&bootloader->config))
        return LINK_UDS_BOOTLOADER_RESULT_LOCKED;
    if (!bootloader->config.backend.read_monotonic_version(
            bootloader->config.backend.context, &version)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    bootloader->installed_version = version;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_ARMED;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_enter_programming_session(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_ARMED)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    bootloader->dtc_recording_disabled = false;
    bootloader->communication_disabled = false;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_grant_security(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED;
    update_quiesced_state(bootloader);
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_set_dtc_recording_disabled(
    LinkUdsBootloader *bootloader, bool disabled)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_QUIESCED) {
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    }
    bootloader->dtc_recording_disabled = disabled;
    if (!disabled && bootloader->state == LINK_UDS_BOOTLOADER_STATE_QUIESCED)
        bootloader->state = bootloader->config.require_security_access
            ? LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED
            : LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION;
    update_quiesced_state(bootloader);
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_set_communication_disabled(
    LinkUdsBootloader *bootloader, bool disabled)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED &&
        bootloader->state != LINK_UDS_BOOTLOADER_STATE_QUIESCED) {
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    }
    bootloader->communication_disabled = disabled;
    if (!disabled && bootloader->state == LINK_UDS_BOOTLOADER_STATE_QUIESCED)
        bootloader->state = bootloader->config.require_security_access
            ? LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED
            : LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION;
    update_quiesced_state(bootloader);
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_request_download(
    LinkUdsBootloader *bootloader, uint32_t version, uint64_t image_size)
{
    uint8_t slot = LINK_UDS_BOOTLOADER_SLOT_NONE;
    if (bootloader == NULL || image_size == 0U)
        return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (!prerequisites_met(bootloader))
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (version <= bootloader->installed_version)
        return LINK_UDS_BOOTLOADER_RESULT_ROLLBACK_REJECTED;
    if (!bootloader->config.backend.select_inactive_slot(
            bootloader->config.backend.context, &slot) ||
        slot == LINK_UDS_BOOTLOADER_SLOT_NONE) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    if (!bootloader->config.backend.begin_image(
            bootloader->config.backend.context, slot, version, image_size)) {
        bootloader->candidate_slot = slot;
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }

    bootloader->candidate_version = version;
    bootloader->candidate_size = image_size;
    bootloader->candidate_slot = slot;
    bootloader->transferred = 0U;
    bootloader->expected_block_sequence_counter = 1U;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_DOWNLOADING;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_transfer_data(
    LinkUdsBootloader *bootloader, uint8_t block_sequence_counter,
    const uint8_t *data, size_t length)
{
    uint64_t remaining;
    if (bootloader == NULL || data == NULL || length == 0U)
        return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_DOWNLOADING)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (block_sequence_counter != bootloader->expected_block_sequence_counter)
        return LINK_UDS_BOOTLOADER_RESULT_SEQUENCE_ERROR;
    if (bootloader->transferred > bootloader->candidate_size)
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_SIZE_MISMATCH);
    remaining = bootloader->candidate_size - bootloader->transferred;
    if ((uint64_t)length > remaining)
        return LINK_UDS_BOOTLOADER_RESULT_SIZE_MISMATCH;
    if (!bootloader->config.backend.write_block(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->transferred, data, length)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    bootloader->transferred += (uint64_t)length;
    bootloader->expected_block_sequence_counter =
        (uint8_t)(bootloader->expected_block_sequence_counter + 1U);
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_request_transfer_exit(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_DOWNLOADING)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (bootloader->transferred != bootloader->candidate_size)
        return LINK_UDS_BOOTLOADER_RESULT_SIZE_MISMATCH;
    if (!bootloader->config.backend.finish_image(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->candidate_size)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_TRANSFER_COMPLETE;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_check_memory(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_TRANSFER_COMPLETE)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (!bootloader->config.backend.verify_integrity(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->candidate_version, bootloader->candidate_size)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_INTEGRITY_FAILED);
    }
    if (bootloader->config.require_authenticity &&
        !bootloader->config.backend.verify_authenticity(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->candidate_version, bootloader->candidate_size)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_AUTHENTICITY_FAILED);
    }
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_VERIFIED;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_stage_for_reset(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_VERIFIED)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (!bootloader->config.backend.stage_inactive_slot(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->candidate_version)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    if (bootloader->config.require_secure_boot_validation &&
        !bootloader->config.backend.secure_boot_validate_candidate(
            bootloader->config.backend.context, bootloader->candidate_slot,
            bootloader->candidate_version)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_SECURE_BOOT_FAILED);
    }
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_STAGED;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

LinkUdsBootloaderResult link_uds_bootloader_confirm_boot(
    LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT;
    if (bootloader->state != LINK_UDS_BOOTLOADER_STATE_STAGED)
        return LINK_UDS_BOOTLOADER_RESULT_BAD_STATE;
    if (!bootloader->config.backend.commit_monotonic_version(
            bootloader->config.backend.context,
            bootloader->candidate_version)) {
        return fail(bootloader, LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE);
    }
    bootloader->installed_version = bootloader->candidate_version;
    bootloader->state = LINK_UDS_BOOTLOADER_STATE_COMPLETE;
    return LINK_UDS_BOOTLOADER_RESULT_OK;
}

void link_uds_bootloader_abort(LinkUdsBootloader *bootloader)
{
    if (bootloader == NULL) return;
    backend_abort(bootloader);
    bootloader->candidate_version = 0U;
    bootloader->candidate_size = 0U;
    bootloader->transferred = 0U;
    bootloader->candidate_slot = LINK_UDS_BOOTLOADER_SLOT_NONE;
    bootloader->expected_block_sequence_counter = 0U;
    bootloader->dtc_recording_disabled = false;
    bootloader->communication_disabled = false;
    bootloader->state = bootloader->config.allow_programming
        ? LINK_UDS_BOOTLOADER_STATE_ARMED
        : LINK_UDS_BOOTLOADER_STATE_DISARMED;
}
