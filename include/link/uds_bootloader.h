// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file uds_bootloader.h
 * @brief Fail-closed UDS programming/OTA bootloader state machine.
 *
 * LINK owns sequencing, transfer accounting, A/B activation policy and
 * anti-rollback/integrity/authenticity gates. Target code owns flash erase/write
 * primitives, secure key material, HSM/signature verification and boot-vector
 * handoff. Programming is disabled by default and no hardware address is
 * embedded here.
 */
#ifndef LINK_UDS_BOOTLOADER_H
#define LINK_UDS_BOOTLOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_UDS_BOOTLOADER_SLOT_NONE UINT8_C(0xff)

typedef enum {
    LINK_UDS_BOOTLOADER_RESULT_OK = 0,
    LINK_UDS_BOOTLOADER_RESULT_INVALID_ARGUMENT,
    LINK_UDS_BOOTLOADER_RESULT_LOCKED,
    LINK_UDS_BOOTLOADER_RESULT_BAD_STATE,
    LINK_UDS_BOOTLOADER_RESULT_SEQUENCE_ERROR,
    LINK_UDS_BOOTLOADER_RESULT_SIZE_MISMATCH,
    LINK_UDS_BOOTLOADER_RESULT_ROLLBACK_REJECTED,
    LINK_UDS_BOOTLOADER_RESULT_BACKEND_FAILURE,
    LINK_UDS_BOOTLOADER_RESULT_INTEGRITY_FAILED,
    LINK_UDS_BOOTLOADER_RESULT_AUTHENTICITY_FAILED,
    LINK_UDS_BOOTLOADER_RESULT_SECURE_BOOT_FAILED
} LinkUdsBootloaderResult;

typedef enum {
    LINK_UDS_BOOTLOADER_STATE_DISARMED = 0,
    LINK_UDS_BOOTLOADER_STATE_ARMED,
    LINK_UDS_BOOTLOADER_STATE_PROGRAMMING_SESSION,
    LINK_UDS_BOOTLOADER_STATE_SECURITY_GRANTED,
    LINK_UDS_BOOTLOADER_STATE_QUIESCED,
    LINK_UDS_BOOTLOADER_STATE_DOWNLOADING,
    LINK_UDS_BOOTLOADER_STATE_TRANSFER_COMPLETE,
    LINK_UDS_BOOTLOADER_STATE_VERIFIED,
    LINK_UDS_BOOTLOADER_STATE_STAGED,
    LINK_UDS_BOOTLOADER_STATE_COMPLETE,
    LINK_UDS_BOOTLOADER_STATE_FAILED
} LinkUdsBootloaderState;

typedef struct {
    bool (*read_monotonic_version)(void *context, uint32_t *version);
    bool (*select_inactive_slot)(void *context, uint8_t *slot);
    bool (*begin_image)(
        void *context, uint8_t slot, uint32_t version, uint64_t image_size);
    bool (*write_block)(
        void *context, uint8_t slot, uint64_t offset,
        const uint8_t *data, size_t length);
    bool (*finish_image)(
        void *context, uint8_t slot, uint64_t image_size);
    bool (*verify_integrity)(
        void *context, uint8_t slot, uint32_t version, uint64_t image_size);
    bool (*verify_authenticity)(
        void *context, uint8_t slot, uint32_t version, uint64_t image_size);
    bool (*stage_inactive_slot)(
        void *context, uint8_t slot, uint32_t version);
    bool (*secure_boot_validate_candidate)(
        void *context, uint8_t slot, uint32_t version);
    bool (*commit_monotonic_version)(void *context, uint32_t version);
    void (*abort_image)(void *context, uint8_t slot);
    void *context;
} LinkUdsBootloaderBackend;

typedef struct {
    /*
     * Deliberate two-key safety model:
     *  - allow_programming must be explicitly true;
     *  - every mandatory backend operation must be supplied before arming.
     */
    bool allow_programming;
    bool require_security_access;
    bool require_quiesce;
    bool require_authenticity;
    bool require_secure_boot_validation;
    LinkUdsBootloaderBackend backend;
} LinkUdsBootloaderConfig;

#define LINK_UDS_BOOTLOADER_CONFIG_INIT     { false, true, true, true, true,       { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL } }

typedef struct {
    LinkUdsBootloaderConfig config;
    LinkUdsBootloaderState state;
    uint32_t installed_version;
    uint32_t candidate_version;
    uint64_t candidate_size;
    uint64_t transferred;
    uint8_t candidate_slot;
    uint8_t expected_block_sequence_counter;
    bool dtc_recording_disabled;
    bool communication_disabled;
} LinkUdsBootloader;

bool link_uds_bootloader_init(
    LinkUdsBootloader *bootloader, const LinkUdsBootloaderConfig *config);

LinkUdsBootloaderResult link_uds_bootloader_arm(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_enter_programming_session(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_grant_security(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_set_dtc_recording_disabled(
    LinkUdsBootloader *bootloader, bool disabled);

LinkUdsBootloaderResult link_uds_bootloader_set_communication_disabled(
    LinkUdsBootloader *bootloader, bool disabled);

LinkUdsBootloaderResult link_uds_bootloader_request_download(
    LinkUdsBootloader *bootloader, uint32_t version, uint64_t image_size);

LinkUdsBootloaderResult link_uds_bootloader_transfer_data(
    LinkUdsBootloader *bootloader, uint8_t block_sequence_counter,
    const uint8_t *data, size_t length);

LinkUdsBootloaderResult link_uds_bootloader_request_transfer_exit(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_check_memory(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_stage_for_reset(
    LinkUdsBootloader *bootloader);

LinkUdsBootloaderResult link_uds_bootloader_confirm_boot(
    LinkUdsBootloader *bootloader);

void link_uds_bootloader_abort(LinkUdsBootloader *bootloader);

#ifdef __cplusplus
}
#endif

#endif
