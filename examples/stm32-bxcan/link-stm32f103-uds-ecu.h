// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_STM32F103_UDS_ECU_H
#define LINK_STM32F103_UDS_ECU_H

#include "link/uds_server.h"
#include "link/uds_dtc_lifecycle.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_STM32F103_FLASH_BYTES UINT32_C(0x00080000)
#define LINK_STM32F103_FLASH_PAGE_BYTES UINT32_C(0x00000800)
#define LINK_STM32F103_FLASH_PAGE_COUNT UINT32_C(256)
#define LINK_STM32F103_FLASH_BASE UINT32_C(0x08000000)
#define LINK_STM32F103_UDS_STATE_PAGE_A UINT32_C(0x0807f000)
#define LINK_STM32F103_UDS_STATE_PAGE_B UINT32_C(0x0807f800)

#define LINK_STM32F103_UDS_DTC_COUNT 3U
#define LINK_STM32F103_UDS_DID_BYTES 16U
#define LINK_STM32F103_UDS_SANDBOX_BYTES 256U
#define LINK_STM32F103_UDS_RECORD_BYTES 4U
#define LINK_STM32F103_UDS_SECURITY_KEY_BYTES 16U
#define LINK_STM32F103_UDS_SECURITY_SEED_BYTES 16U

typedef bool (*LinkStm32F103FlashReadFn)(
    void *context, uint32_t address, void *data, size_t length);
typedef bool (*LinkStm32F103FlashErasePageFn)(
    void *context, uint32_t page_address);
typedef bool (*LinkStm32F103FlashProgramFn)(
    void *context, uint32_t address, const void *data, size_t length);
typedef uint32_t (*LinkStm32F103ClockMsFn)(void *context);

typedef struct {
    void *context;
    LinkStm32F103FlashReadFn read;
    LinkStm32F103FlashErasePageFn erase_page;
    LinkStm32F103FlashProgramFn program;

    /*
     * Legacy two-page mode remains supported through page_a/page_b.
     *
     * For wear-levelled operation, provide page_addresses/page_count with at
     * least two crash-consistent state slots. A slot uses as many flash pages
     * as are required to hold LinkStm32F103PersistentState; LINK rotates each
     * complete, CRC-verified generation to the next slot instead of repeatedly
     * erasing the same two pages. This means the persistent state may span
     * multiple physical pages instead of being artificially capped at one.
     *
     * page_addresses defines the ordered pages that make up the slot ring.
     * The number of usable slots is:
     *
     *   page_count / ceil(sizeof(LinkStm32F103PersistentState) / page_size)
     *
     * and must be at least two. Any trailing pages that do not form a complete
     * slot are deliberately ignored. The address array is borrowed for the
     * lifetime of the ECU object and must remain valid while the ECU is in use.
     */
    uint32_t page_a_address;
    uint32_t page_b_address;
    const uint32_t *page_addresses;
    size_t page_count;
    uint32_t page_size;
} LinkStm32F103FlashStore;

typedef struct {
    LinkStm32F103FlashStore flash;
    LinkStm32F103ClockMsFn clock_ms;
    void *clock_context;
    const uint8_t *vin;
    const uint8_t *security_key;
} LinkStm32F103UdsEcuConfig;

typedef struct {
    uint32_t magic;
    uint32_t schema;
    uint32_t generation;
    uint8_t dtc_status[LINK_STM32F103_UDS_DTC_COUNT];
    int8_t dtc_fdc[LINK_STM32F103_UDS_DTC_COUNT];
    uint8_t dtc_aging[LINK_STM32F103_UDS_DTC_COUNT];
    uint8_t dtc_failure_cycles[LINK_STM32F103_UDS_DTC_COUNT];
    uint8_t dtc_permanent[LINK_STM32F103_UDS_DTC_COUNT];
    uint8_t dtc_setting_enabled;
    uint8_t communication_control;
    uint8_t user_did[LINK_STM32F103_UDS_DID_BYTES];
    uint8_t sandbox[LINK_STM32F103_UDS_SANDBOX_BYTES];
    uint8_t dynamic_defined;
    uint8_t reserved0;
    uint16_t dynamic_did;
    uint16_t dynamic_source_did;
    uint16_t reserved1;
    uint8_t snapshot[LINK_STM32F103_UDS_DTC_COUNT]
                    [LINK_STM32F103_UDS_RECORD_BYTES];
    uint8_t stored[LINK_STM32F103_UDS_DTC_COUNT]
                  [LINK_STM32F103_UDS_RECORD_BYTES];
    uint8_t extended[LINK_STM32F103_UDS_DTC_COUNT]
                    [LINK_STM32F103_UDS_RECORD_BYTES];
    uint32_t crc32;
} LinkStm32F103PersistentState;

typedef enum {
    LINK_STM32F103_TRANSFER_NONE = 0,
    LINK_STM32F103_TRANSFER_DOWNLOAD,
    LINK_STM32F103_TRANSFER_UPLOAD
} LinkStm32F103TransferMode;

typedef struct {
    LinkStm32F103UdsEcuConfig config;
    LinkUdsServer server;
    LinkStm32F103PersistentState state;
    uint32_t active_page;
    uint8_t vin[17U];
    uint8_t security_seed[LINK_STM32F103_UDS_SECURITY_SEED_BYTES];
    LinkStm32F103TransferMode transfer_mode;
    uint32_t transfer_address;
    uint32_t transfer_size;
    uint32_t transfer_offset;
    uint8_t transfer_block;
    LinkUdsDtcLifecycleState dtc_lifecycle[LINK_STM32F103_UDS_DTC_COUNT];
    LinkUdsDtcRecord dtc_records[LINK_STM32F103_UDS_DTC_COUNT];
    LinkUdsServerDtcDetail dtc_details[LINK_STM32F103_UDS_DTC_COUNT];
    LinkUdsServerDtcStore dtc_store;
} LinkStm32F103UdsEcu;

bool link_stm32f103_uds_ecu_init(
    LinkStm32F103UdsEcu *ecu,
    const LinkStm32F103UdsEcuConfig *config);

LinkUdsServer *link_stm32f103_uds_ecu_server(LinkStm32F103UdsEcu *ecu);

LinkUdsServerResult link_stm32f103_uds_ecu_handle(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequestContext *context,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

bool link_stm32f103_uds_ecu_report_dtc(
    LinkStm32F103UdsEcu *ecu,
    uint32_t code,
    uint8_t status,
    int8_t fault_detection_counter,
    bool permanent_status);

/**
 * Begin a new diagnostic operation cycle for every reference DTC.
 */
bool link_stm32f103_uds_ecu_begin_operation_cycle(
    LinkStm32F103UdsEcu *ecu);

/**
 * Feed one monitor result into the shared DTC lifecycle engine.
 */
bool link_stm32f103_uds_ecu_report_dtc_test(
    LinkStm32F103UdsEcu *ecu,
    uint32_t code,
    LinkUdsDtcTestResult result);

/**
 * Complete the current diagnostic operation cycle and apply confirmation/aging.
 */
bool link_stm32f103_uds_ecu_end_operation_cycle(
    LinkStm32F103UdsEcu *ecu);

bool link_stm32f103_uds_ecu_flush(LinkStm32F103UdsEcu *ecu);

#ifdef __cplusplus
}
#endif

#endif
