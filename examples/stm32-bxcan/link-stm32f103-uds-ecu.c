// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file link-stm32f103-uds-ecu.c
 * @brief Complete bounded STM32F103 UDS ECU reference application core.
 *
 * The implementation is intentionally target-safe: persistent writes use
 * caller-reserved flash pages arranged as at least two CRC-verified state
 * slots, and a state slot may span multiple physical pages. Programming
 * services operate on a 256-byte non-executable sandbox. Products can replace those
 * handlers with real bootloader/application programming after applying their
 * own signing, rollback and flash-layout policy.
 */
#include "link-stm32f103-uds-ecu.h"

#include "link/aes_cmac.h"
#include "link/version.h"

#include <stddef.h>
#include <string.h>

#define LINK_STM32F103_STATE_MAGIC UINT32_C(0x4c4b4631)
#define LINK_STM32F103_STATE_SCHEMA UINT32_C(3)
#define LINK_STM32F103_MAX_TRANSFER_CHUNK 64U
#define LINK_STM32F103_DYNAMIC_DID_MIN UINT16_C(0xf200)
#define LINK_STM32F103_DYNAMIC_DID_MAX UINT16_C(0xf2ff)
#define LINK_STM32F103_USER_DID UINT16_C(0xf1a0)
#define LINK_STM32F103_VIN_DID UINT16_C(0xf190)
#define LINK_STM32F103_SW_DID UINT16_C(0xf187)
#define LINK_STM32F103_ROUTINE_INTEGRITY UINT16_C(0x0201)
#define LINK_STM32F103_ROUTINE_CHECK_MEMORY UINT16_C(0x0202)
#define LINK_STM32F103_ROUTINE_ERASE_MEMORY UINT16_C(0xff00)
#define LINK_STM32F103_ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES UINT16_C(0xff01)

#define LINK_STM32F103_DID_SPARE_PART_NUMBER UINT16_C(0xf181)
#define LINK_STM32F103_DID_BOOT_SW_IDENTIFIER UINT16_C(0xf182)
#define LINK_STM32F103_DID_ECU_SW_NUMBER UINT16_C(0xf183)
#define LINK_STM32F103_DID_APP_SW_NUMBER UINT16_C(0xf184)
#define LINK_STM32F103_DID_HW_NUMBER UINT16_C(0xf185)
#define LINK_STM32F103_DID_ACTIVE_SESSION UINT16_C(0xf186)
#define LINK_STM32F103_DID_SYSTEM_SUPPLIER UINT16_C(0xf18a)

#define LINK_STM32F103_SNAPSHOT_DID_SUPPLY_VOLTAGE UINT16_C(0xdf00)
#define LINK_STM32F103_SNAPSHOT_DID_VEHICLE_SPEED UINT16_C(0xdf01)
#define LINK_STM32F103_SNAPSHOT_DID_OCCURRENCE_COUNTER UINT16_C(0xdf02)
#define LINK_STM32F103_SNAPSHOT_DID_FIRST_ODOMETER UINT16_C(0xdf03)
#define LINK_STM32F103_SNAPSHOT_DID_LAST_ODOMETER UINT16_C(0xdf04)
#define LINK_STM32F103_SNAPSHOT_DID_TIMESTAMP UINT16_C(0xdd00)

#define LINK_STM32F103_SECURITY_MASK \
    (LINK_UDS_SECURITY_LEVEL_MASK(1U) | LINK_UDS_SECURITY_LEVEL_MASK(2U))

/*
 * LINK #42 workbook catalogue. These 66 definitions replace the previous
 * three synthetic placeholder DTCs. The identities, names, severity and
 * functional-unit values were cross-checked against an independent
 * implementation of the same reporter-supplied Diagnostic workbook.
 *
 * U300614/U300615 use functional group 0x33 in the supplied reference data.
 * The C1006xx safety/chassis records use 0xD0. Debounce, confirmation and
 * aging thresholds remain explicit LINK reference-policy values.
 */
static const LinkUdsDtcLifecycleDefinition
stm32f103_dtc_definitions[LINK_STM32F103_UDS_DTC_COUNT] = {
    { UINT32_C(0xf00614), 0x33U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* U300614 ASW_DTC_BatteryVoltHighWarn */
    { UINT32_C(0xf00615), 0x33U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* U300615 ASW_DTC_BatteryVoltLowWarn */
    { UINT32_C(0xd00616), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100616 ASW_DTC_BatteryVoltHigh */
    { UINT32_C(0xd00617), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100617 ASW_DTC_BatteryVoltLow */
    { UINT32_C(0xd00618), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100618 ASW_DTC_CanBusOff */
    { UINT32_C(0xd00619), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100619 ASW_DTC_SysPrs_SigLost */
    { UINT32_C(0xd00620), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100620 ASW_DTC_SysPrs_OverLimit */
    { UINT32_C(0xd00621), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100621 ASW_DTC_SysPrs_JumpErr */
    { UINT32_C(0xd00622), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100622 ASW_DTC_AccuPrs_SigLost */
    { UINT32_C(0xd00623), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100623 ASW_DTC_AccuPrs_OverLimit */
    { UINT32_C(0xd00624), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100624 ASW_DTC_AccuPrs_JumpErr */
    { UINT32_C(0xd00625), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100625 ASW_DTC_PfsPrs_SigLost */
    { UINT32_C(0xd00626), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100626 ASW_DTC_PfsPrs_OverLimit */
    { UINT32_C(0xd00627), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100627 ASW_DTC_PfsPrs_JumpErr */
    { UINT32_C(0xd00628), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100628 ASW_DTC_PipeLineErrFL */
    { UINT32_C(0xd00629), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100629 ASW_DTC_PipeLineErrFR */
    { UINT32_C(0xd00630), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100630 ASW_DTC_PipeLineErrRL */
    { UINT32_C(0xd00631), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100631 ASW_DTC_PipeLineErrRR */
    { UINT32_C(0xd00632), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100632 ASW_DTC_WssFL_Err */
    { UINT32_C(0xd00633), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100633 ASW_DTC_WssFR_Err */
    { UINT32_C(0xd00634), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100634 ASW_DTC_WssRL_Err */
    { UINT32_C(0xd00635), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100635 ASW_DTC_WssRR_Err */
    { UINT32_C(0xd00636), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100636 ASW_DTC_AccuLowPrs_Err */
    { UINT32_C(0xd00637), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100637 ASW_DTC_AccuPrsHoldErr */
    { UINT32_C(0xd00638), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100638 ASW_DTC_PconErr */
    { UINT32_C(0xd00639), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100639 ASW_DTC_AccuPump_DG */
    { UINT32_C(0xd00640), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100640 ASW_DTC_PumpMotLockedRotFault */
    { UINT32_C(0xd00641), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100641 ASW_DTC_AccuPump_Err */
    { UINT32_C(0xd00642), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100642 ASW_DTC_PTS_Err */
    { UINT32_C(0xd00643), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100643 ASW_DTC_PTS_Chanl_1Err */
    { UINT32_C(0xd00644), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100644 ASW_DTC_PTS_Chanl_2Err */
    { UINT32_C(0xd00645), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100645 ASW_DTC_MC_OutleakFault */
    { UINT32_C(0xd00646), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100646 ASW_DTC_MC_InleakFault */
    { UINT32_C(0xd00647), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100647 ASW_DTC_IMUyawrateSignalErrLevel */
    { UINT32_C(0xd00648), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100648 ASW_DTC_IMUaySignalErrLevel */
    { UINT32_C(0xd00649), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100649 ASW_DTC_IMUaxSignalErrLevel */
    { UINT32_C(0xd00650), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100650 ASW_DTC_SASSignalErrLevel */
    { UINT32_C(0xd00651), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100651 ASW_DTC_BrkConValveDriverFault */
    { UINT32_C(0xd00652), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100652 ASW_DTC_WhelPrsConValveDriverFault */
    { UINT32_C(0xd00653), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100653 ASW_DTC_ISV1_Fault */
    { UINT32_C(0xd00654), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100654 ASW_DTC_ISV1_OverTempWarn */
    { UINT32_C(0xd00655), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100655 ASW_DTC_ISV2_Fault */
    { UINT32_C(0xd00656), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100656 ASW_DTC_ISV2_OverTempWarn */
    { UINT32_C(0xd00657), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100657 ASW_DTC_ISV3_Fault */
    { UINT32_C(0xd00658), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100658 ASW_DTC_ISV3_OverTempWarn */
    { UINT32_C(0xd00659), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100659 ASW_DTC_ISV4_Fault */
    { UINT32_C(0xd00660), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100660 ASW_DTC_ISV4_OverTempWarn */
    { UINT32_C(0xd00661), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100661 ASW_DTC_PAV_Fault */
    { UINT32_C(0xd00662), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100662 ASW_DTC_PAV_OverTempWarn */
    { UINT32_C(0xd00663), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100663 ASW_DTC_PRV_Fault */
    { UINT32_C(0xd00664), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100664 ASW_DTC_PRV_OverTempWarn */
    { UINT32_C(0xd00665), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100665 ASW_DTC_BAV_Fault */
    { UINT32_C(0xd00666), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100666 ASW_DTC_CSV_Fault */
    { UINT32_C(0xd00667), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100667 ASW_DTC_SSV_Fault */
    { UINT32_C(0xd00668), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100668 ASW_DTC_USV_Fault */
    { UINT32_C(0xd00669), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100669 ASW_DTC_SASangErr */
    { UINT32_C(0xd00670), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100670 ASW_DTC_BrkSwitchErr */
    { UINT32_C(0xd00671), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100671 ASW_DTC_PowerSysErr */
    { UINT32_C(0xd00672), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100672 ASW_DTC_AccPosErr */
    { UINT32_C(0xd00673), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100673 ASW_DTC_GearPosErr */
    { UINT32_C(0xd00674), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100674 ASW_DTC_BtrErr */
    { UINT32_C(0xd00675), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100675 ASW_DTC_BrkFluidLevel_Low */
    { UINT32_C(0xd00676), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100676 ASW_DTC_SeatBltErr */
    { UINT32_C(0xd00677), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100677 ASW_DTC_DoorsErr */
    { UINT32_C(0xd00678), 0xd0U, 0xa0U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100678 ASW_DTC_AdasConnectErr */
    { UINT32_C(0xd00679), 0xd0U, 0x80U, 0x01U,
      64U, 64U, 2U, 3U }, /* C100679 ASW_DTC_ExternalEPBErr */
};

static const uint8_t stm32f103_default_vin[17U] = {
    'L','I','N','K','S','T','M','3','2','F','1','0','3','0','0','1','A'
};

static const uint8_t stm32f103_spare_part_number[] =
    "ER0101-000000-REV00";
static const uint8_t stm32f103_boot_sw_identifier[] = "UdsBt.01";
static const uint8_t stm32f103_ecu_sw_number[] = {12U};
static const uint8_t stm32f103_app_sw_number[] = {13U};
static const uint8_t stm32f103_hw_number[] = "HIBV3.5.0";
static const uint8_t stm32f103_system_supplier[] = "HIB01-TW0000";

/*
 * Deterministic DEBUG/REFERENCE SecurityAccess values requested in LINK #43.
 * These are intentionally fixed for bench interoperability and MUST NOT be
 * treated as production vehicle secrets.
 */
static const uint8_t stm32f103_level1_seed[4U] = {
    0x12U, 0x34U, 0x56U, 0x78U
};
static const uint8_t stm32f103_level1_key[4U] = {
    0x87U, 0x65U, 0x43U, 0x21U
};
static const uint8_t stm32f103_level2_seed[16U] = {
    0x6bU,0xc1U,0xbeU,0xe2U,0x2eU,0x40U,0x9fU,0x96U,
    0xe9U,0x3dU,0x7eU,0x11U,0x73U,0x93U,0x17U,0x2aU
};

static const LinkUdsServerPolicy stm32f103_policies[] = {
    {
        /*
         * ISO 14229 permits ClearDiagnosticInformation in the default and
         * non-default diagnostic sessions. Do not invent a SecurityAccess
         * prerequisite in the portable reference. Product/OEM policy may
         * still add one explicitly above LINK when required.
         */
        LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION,
        false, 0U,
        LINK_UDS_SESSION_MASK_ALL,
        LINK_UDS_SECURITY_LEVEL_MASK_ALL,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_COMMUNICATION_CONTROL,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_ACCESS_TIMING_PARAMETER,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_RESPONSE_ON_EVENT,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_LINK_CONTROL,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_ROUTINE_CONTROL,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_REQUEST_DOWNLOAD,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_REQUEST_UPLOAD,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_TRANSFER_DATA,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_REQUEST_TRANSFER_EXIT,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_REQUEST_FILE_TRANSFER,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_WRITE_MEMORY_BY_ADDRESS,
        false, 0U,
        LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_SECURED_DATA_TRANSMISSION,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    },
    {
        LINK_UDS_SERVICE_CONTROL_DTC_SETTING,
        false, 0U,
        LINK_UDS_SESSION_MASK_EXTENDED | LINK_UDS_SESSION_MASK_PROGRAMMING,
        LINK_STM32F103_SECURITY_MASK,
        LINK_UDS_ADDRESSING_MASK_PHYSICAL
    }
};

static uint32_t stm32f103_crc32(const void *data, size_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = UINT32_C(0xffffffff);
    size_t index;
    unsigned int bit;

    for (index = 0U; index < length; ++index) {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask =
                (uint32_t)(0U - (uint32_t)(crc & UINT32_C(1)));
            crc = (crc >> 1U) ^ (UINT32_C(0xedb88320) & mask);
        }
    }
    return ~crc;
}

static bool stm32f103_state_valid(const LinkStm32F103PersistentState *state)
{
    uint32_t expected;
    if (state == NULL ||
        state->magic != LINK_STM32F103_STATE_MAGIC ||
        state->schema != LINK_STM32F103_STATE_SCHEMA ||
        state->dtc_setting_enabled > 1U ||
        state->dynamic_defined > 1U) {
        return false;
    }
    expected = stm32f103_crc32(
        state, offsetof(LinkStm32F103PersistentState, crc32));
    return expected == state->crc32;
}

static void stm32f103_state_defaults(LinkStm32F103PersistentState *state)
{
    static const uint8_t user_did[LINK_STM32F103_UDS_DID_BYTES] = {
        'L','I','N','K','-','F','1','0','3','-','E','C','U','0','1',0
    };
    size_t index;
    size_t byte_index;

    memset(state, 0, sizeof(*state));
    state->magic = LINK_STM32F103_STATE_MAGIC;
    state->schema = LINK_STM32F103_STATE_SCHEMA;
    state->dtc_status[0U] =
        LINK_UDS_DTC_STATUS_TEST_FAILED |
        LINK_UDS_DTC_STATUS_CONFIRMED_DTC;
    state->dtc_status[1U] = LINK_UDS_DTC_STATUS_CONFIRMED_DTC;
    state->dtc_status[2U] = LINK_UDS_DTC_STATUS_PENDING_DTC;
    /*
     * FDC is operation-cycle-local. Persistent default DTC statuses model
     * historical fault state, while a newly started monitor cycle begins at 0.
     */
    state->dtc_fdc[0U] = 0;
    state->dtc_fdc[1U] = 0;
    state->dtc_fdc[2U] = 0;
    state->dtc_failure_cycles[0U] = 2U;
    state->dtc_failure_cycles[1U] = 2U;
    state->dtc_failure_cycles[2U] = 1U;
    state->dtc_permanent[0U] = 1U;
    state->dtc_setting_enabled = 1U;
    memcpy(state->user_did, user_did, sizeof(user_did));
    memset(state->sandbox, 0xff, sizeof(state->sandbox));

    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        for (byte_index = 0U;
             byte_index < LINK_STM32F103_UDS_RECORD_BYTES;
             ++byte_index) {
            state->snapshot[index][byte_index] =
                (uint8_t)(0x10U + (uint8_t)(index * 0x10U) +
                          (uint8_t)byte_index);
            state->stored[index][byte_index] =
                (uint8_t)(0x40U + (uint8_t)(index * 0x10U) +
                          (uint8_t)byte_index);
            state->extended[index][byte_index] =
                (uint8_t)(0x70U + (uint8_t)(index * 0x10U) +
                          (uint8_t)byte_index);
        }
    }
}

static size_t stm32f103_flash_page_count(
    const LinkStm32F103FlashStore *flash)
{
    if (flash != NULL &&
        flash->page_addresses != NULL &&
        flash->page_count >= 2U) {
        return flash->page_count;
    }
    return 2U;
}

static bool stm32f103_flash_page_address(
    const LinkStm32F103FlashStore *flash,
    size_t index,
    uint32_t *address)
{
    if (flash == NULL || address == NULL) return false;

    if (flash->page_addresses != NULL && flash->page_count >= 2U) {
        if (index >= flash->page_count) return false;
        *address = flash->page_addresses[index];
        return true;
    }

    if (index == 0U) {
        *address = flash->page_a_address;
        return true;
    }
    if (index == 1U) {
        *address = flash->page_b_address;
        return true;
    }
    return false;
}

static size_t stm32f103_state_pages_per_slot(
    const LinkStm32F103FlashStore *flash)
{
    const size_t state_bytes = sizeof(LinkStm32F103PersistentState);
    size_t page_bytes;

    if (flash == NULL || flash->page_size == 0U) return 0U;
    page_bytes = (size_t)flash->page_size;
    return state_bytes / page_bytes +
           ((state_bytes % page_bytes) != 0U ? 1U : 0U);
}

static size_t stm32f103_state_slot_count(
    const LinkStm32F103FlashStore *flash)
{
    const size_t pages_per_slot = stm32f103_state_pages_per_slot(flash);
    if (pages_per_slot == 0U) return 0U;
    return stm32f103_flash_page_count(flash) / pages_per_slot;
}

static bool stm32f103_state_slot_first_address(
    const LinkStm32F103FlashStore *flash,
    size_t slot_index,
    uint32_t *address)
{
    const size_t pages_per_slot = stm32f103_state_pages_per_slot(flash);
    const size_t slot_count = stm32f103_state_slot_count(flash);

    if (pages_per_slot == 0U || slot_index >= slot_count) return false;
    return stm32f103_flash_page_address(
        flash, slot_index * pages_per_slot, address);
}

static bool stm32f103_read_state_slot(
    const LinkStm32F103FlashStore *flash,
    size_t slot_index,
    LinkStm32F103PersistentState *state);

static bool stm32f103_active_persisted_state_valid(
    const LinkStm32F103UdsEcu *ecu)
{
    LinkStm32F103PersistentState persisted;
    const LinkStm32F103FlashStore *flash;
    size_t slot_count;
    size_t slot;

    if (ecu == NULL) return false;
    flash = &ecu->config.flash;
    slot_count = stm32f103_state_slot_count(flash);

    for (slot = 0U; slot < slot_count; ++slot) {
        uint32_t first_address = 0U;
        if (!stm32f103_state_slot_first_address(
                flash, slot, &first_address)) {
            return false;
        }
        if (first_address != ecu->active_page) continue;
        return stm32f103_read_state_slot(
                   flash, slot, &persisted) &&
               stm32f103_state_valid(&persisted);
    }
    return false;
}

static bool stm32f103_read_state_slot(
    const LinkStm32F103FlashStore *flash,
    size_t slot_index,
    LinkStm32F103PersistentState *state)
{
    const size_t pages_per_slot = stm32f103_state_pages_per_slot(flash);
    const size_t slot_count = stm32f103_state_slot_count(flash);
    uint8_t *bytes = (uint8_t *)state;
    size_t remaining = sizeof(*state);
    size_t offset = 0U;
    size_t page;

    if (flash == NULL || state == NULL ||
        pages_per_slot == 0U || slot_index >= slot_count) {
        return false;
    }

    for (page = 0U; page < pages_per_slot; ++page) {
        uint32_t address = 0U;
        const size_t chunk =
            remaining < (size_t)flash->page_size
                ? remaining : (size_t)flash->page_size;
        if (!stm32f103_flash_page_address(
                flash, slot_index * pages_per_slot + page, &address) ||
            !flash->read(flash->context, address, bytes + offset, chunk)) {
            return false;
        }
        offset += chunk;
        remaining -= chunk;
    }
    return remaining == 0U;
}

static bool stm32f103_write_state_slot(
    const LinkStm32F103FlashStore *flash,
    size_t slot_index,
    const LinkStm32F103PersistentState *state)
{
    const size_t pages_per_slot = stm32f103_state_pages_per_slot(flash);
    const size_t slot_count = stm32f103_state_slot_count(flash);
    const uint8_t *bytes = (const uint8_t *)state;
    size_t remaining = sizeof(*state);
    size_t offset = 0U;
    size_t page;

    if (flash == NULL || state == NULL ||
        pages_per_slot == 0U || slot_index >= slot_count) {
        return false;
    }

    /*
     * Erase the entire destination slot before programming any part of the
     * new generation. The previous slot remains untouched until the new full
     * state has been programmed and CRC-verified, preserving rollback after a
     * torn multi-page write.
     */
    for (page = 0U; page < pages_per_slot; ++page) {
        uint32_t address = 0U;
        if (!stm32f103_flash_page_address(
                flash, slot_index * pages_per_slot + page, &address) ||
            !flash->erase_page(flash->context, address)) {
            return false;
        }
    }

    for (page = 0U; page < pages_per_slot; ++page) {
        uint32_t address = 0U;
        const size_t chunk =
            remaining < (size_t)flash->page_size
                ? remaining : (size_t)flash->page_size;
        if (!stm32f103_flash_page_address(
                flash, slot_index * pages_per_slot + page, &address) ||
            !flash->program(
                flash->context, address, bytes + offset, chunk)) {
            return false;
        }
        offset += chunk;
        remaining -= chunk;
    }
    return remaining == 0U;
}

static bool stm32f103_generation_newer(uint32_t candidate, uint32_t current)
{
    return (int32_t)(candidate - current) > 0;
}

static bool stm32f103_flash_config_valid(
    const LinkStm32F103UdsEcuConfig *config)
{
    const LinkStm32F103FlashStore *flash;
    size_t count;
    size_t pages_per_slot;
    size_t slots;
    size_t i;
    size_t j;

    if (config == NULL || config->clock_ms == NULL ||
        config->security_key == NULL) {
        return false;
    }

    flash = &config->flash;
    if (flash->read == NULL ||
        flash->erase_page == NULL ||
        flash->program == NULL ||
        flash->page_size == 0U) {
        return false;
    }

    count = stm32f103_flash_page_count(flash);
    pages_per_slot = stm32f103_state_pages_per_slot(flash);
    slots = stm32f103_state_slot_count(flash);
    if (count < 2U || pages_per_slot == 0U || slots < 2U) return false;

    for (i = 0U; i < count; ++i) {
        uint32_t address_i = 0U;
        if (!stm32f103_flash_page_address(flash, i, &address_i) ||
            (address_i % flash->page_size) != 0U) {
            return false;
        }
        for (j = 0U; j < i; ++j) {
            uint32_t address_j = 0U;
            if (!stm32f103_flash_page_address(flash, j, &address_j) ||
                address_i == address_j) {
                return false;
            }
        }
    }
    return true;
}

static void stm32f103_hydrate_dtc_lifecycle(LinkStm32F103UdsEcu *ecu)
{
    size_t index;

    if (ecu == NULL) return;
    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        LinkUdsDtcLifecycleState *lifecycle = &ecu->dtc_lifecycle[index];
        lifecycle->status = ecu->state.dtc_status[index];
        lifecycle->fault_detection_counter = ecu->state.dtc_fdc[index];
        lifecycle->aging_counter = ecu->state.dtc_aging[index];
        lifecycle->failure_cycle_count = ecu->state.dtc_failure_cycles[index];
        lifecycle->tested_this_cycle = false;
        lifecycle->failed_this_cycle = false;
        lifecycle->passed_this_cycle = false;
    }
}

static void stm32f103_sync_dtc_lifecycle(
    LinkStm32F103UdsEcu *ecu,
    size_t index)
{
    const LinkUdsDtcLifecycleState *lifecycle;

    if (ecu == NULL || index >= LINK_STM32F103_UDS_DTC_COUNT) return;
    lifecycle = &ecu->dtc_lifecycle[index];
    ecu->state.dtc_status[index] = lifecycle->status;
    ecu->state.dtc_fdc[index] = lifecycle->fault_detection_counter;
    ecu->state.dtc_aging[index] = lifecycle->aging_counter;
    ecu->state.dtc_failure_cycles[index] = lifecycle->failure_cycle_count;
}

static void stm32f103_snapshot_put_u16(
    uint8_t *payload,
    size_t *offset,
    uint16_t value)
{
    payload[(*offset)++] = (uint8_t)(value >> 8U);
    payload[(*offset)++] = (uint8_t)value;
}

static void stm32f103_snapshot_put_u32(
    uint8_t *payload,
    size_t *offset,
    uint32_t value)
{
    payload[(*offset)++] = (uint8_t)(value >> 24U);
    payload[(*offset)++] = (uint8_t)(value >> 16U);
    payload[(*offset)++] = (uint8_t)(value >> 8U);
    payload[(*offset)++] = (uint8_t)value;
}

static bool stm32f103_encode_workbook_snapshot(
    LinkStm32F103UdsEcu *ecu,
    size_t index)
{
    LinkStm32F103DtcSnapshotValues values;
    uint8_t *payload;
    size_t offset = 0U;

    if (ecu == NULL || index >= LINK_STM32F103_UDS_DTC_COUNT) return false;
    memset(&values, 0, sizeof(values));

    if (ecu->config.read_dtc_snapshot != NULL) {
        if (!ecu->config.read_dtc_snapshot(
                ecu->config.dtc_snapshot_context,
                stm32f103_dtc_definitions[index].code,
                &values)) {
            ecu->dtc_snapshot_available[index] = false;
            memset(
                ecu->dtc_snapshot_payload[index], 0,
                sizeof(ecu->dtc_snapshot_payload[index]));
            return false;
        }
    } else {
        /*
         * Deterministic BENCH/REFERENCE values. They prove the exact Sheet 7
         * wire layout without pretending LINK knows target sensor history.
         * A real ECU supplies captured/persisted values through the callback.
         */
        values.supply_voltage_mv = UINT16_C(12000);
    }

    payload = ecu->dtc_snapshot_payload[index];

#define SNAPSHOT_DID(did_) \
    do { \
        stm32f103_snapshot_put_u16(payload, &offset, (did_)); \
    } while (0)

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_SUPPLY_VOLTAGE);
    stm32f103_snapshot_put_u16(payload, &offset, values.supply_voltage_mv);

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_VEHICLE_SPEED);
    stm32f103_snapshot_put_u16(
        payload, &offset, values.vehicle_speed_1_256_kph);

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_OCCURRENCE_COUNTER);
    payload[offset++] = values.occurrence_counter;

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_FIRST_ODOMETER);
    stm32f103_snapshot_put_u32(payload, &offset, values.first_odometer_5m);

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_LAST_ODOMETER);
    stm32f103_snapshot_put_u32(payload, &offset, values.last_odometer_5m);

    SNAPSHOT_DID(LINK_STM32F103_SNAPSHOT_DID_TIMESTAMP);
    payload[offset++] = values.timestamp_second;
    payload[offset++] = values.timestamp_minute;
    payload[offset++] = values.timestamp_hour;
    payload[offset++] = values.timestamp_month;
    payload[offset++] = values.timestamp_day;
    payload[offset++] = values.timestamp_year;

#undef SNAPSHOT_DID

    ecu->dtc_snapshot_available[index] =
        offset == LINK_STM32F103_UDS_SNAPSHOT_PAYLOAD_BYTES;
    return ecu->dtc_snapshot_available[index];
}

static void stm32f103_refresh_dtc_store(LinkStm32F103UdsEcu *ecu)
{
    size_t index;

    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        LinkUdsServerDtcDetail *detail = &ecu->dtc_details[index];

        const LinkUdsDtcLifecycleDefinition *definition =
            &stm32f103_dtc_definitions[index];
        const LinkUdsDtcLifecycleState *lifecycle =
            &ecu->dtc_lifecycle[index];

        ecu->dtc_records[index].code = definition->code;
        ecu->dtc_records[index].status = lifecycle->status;

        *detail = (LinkUdsServerDtcDetail)LINK_UDS_SERVER_DTC_DETAIL_INIT;
        detail->code = definition->code;
        detail->severity = definition->severity;
        detail->functional_unit = definition->functional_unit;
        {
            uint8_t reportable_counter = 0U;
            detail->fault_detection_counter =
                link_uds_dtc_lifecycle_reportable_fault_counter(
                    lifecycle, &reportable_counter)
                    ? reportable_counter : 0U;
        }
        detail->first_test_failed_sequence = (uint32_t)(index + 1U);
        detail->confirmed_sequence = (uint32_t)(index + 2U);
        detail->mirror_memory = index != 2U;
        detail->emissions_obd = index == 0U;
        detail->permanent_status = ecu->state.dtc_permanent[index] != 0U;
        detail->functional_group_identifier =
            stm32f103_dtc_definitions[index].functional_group_identifier;
        detail->user_memory_selection = 0x01U;
        if (stm32f103_encode_workbook_snapshot(ecu, index)) {
            detail->snapshot_record_number = 0x01U;
            detail->snapshot_identifier_count =
                LINK_STM32F103_UDS_SNAPSHOT_IDENTIFIER_COUNT;
            detail->snapshot_data = ecu->dtc_snapshot_payload[index];
            detail->snapshot_data_length =
                LINK_STM32F103_UDS_SNAPSHOT_PAYLOAD_BYTES;
        }
        detail->stored_data_record_number = 0x01U;
        detail->stored_data_identifier_count = 0x01U;
        detail->stored_data = ecu->state.stored[index];
        detail->stored_data_length = LINK_STM32F103_UDS_RECORD_BYTES;
        ecu->state.extended[index][0U] = lifecycle->aging_counter;
        ecu->state.extended[index][1U] = lifecycle->failure_cycle_count;
        ecu->state.extended[index][2U] =
            (uint8_t)lifecycle->fault_detection_counter;
        ecu->state.extended[index][3U] =
            definition->functional_group_identifier;
        detail->ext_data_record_number = 0x01U;
        detail->ext_data = ecu->state.extended[index];
        detail->ext_data_length = LINK_STM32F103_UDS_RECORD_BYTES;
    }

    ecu->dtc_store.records = ecu->dtc_records;
    ecu->dtc_store.record_count = LINK_STM32F103_UDS_DTC_COUNT;
    /*
     * The reference lifecycle owns ISO status bits 0..6. It does not model a
     * warning-indicator request, so advertising bit 7 as available would be
     * misleading. Product ECUs that own a warning indicator can provide a
     * wider availability mask in their own DTC store.
     */
    ecu->dtc_store.status_availability_mask =
        LINK_UDS_DTC_STATUS_TEST_FAILED |
        LINK_UDS_DTC_STATUS_TEST_FAILED_THIS_OPERATION_CYCLE |
        LINK_UDS_DTC_STATUS_PENDING_DTC |
        LINK_UDS_DTC_STATUS_CONFIRMED_DTC |
        LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_SINCE_LAST_CLEAR |
        LINK_UDS_DTC_STATUS_TEST_FAILED_SINCE_LAST_CLEAR |
        LINK_UDS_DTC_STATUS_TEST_NOT_COMPLETED_THIS_OPERATION_CYCLE;
    ecu->dtc_store.severity_availability_mask = 0xffU;
    ecu->dtc_store.dtc_format_identifier = 0x01U;
    ecu->dtc_store.details = ecu->dtc_details;
    ecu->dtc_store.detail_count = LINK_STM32F103_UDS_DTC_COUNT;
    ecu->dtc_store.wwh_dtc_format_identifier = 0x04U;
}

bool link_stm32f103_uds_ecu_begin_operation_cycle(
    LinkStm32F103UdsEcu *ecu)
{
    size_t index;

    if (ecu == NULL || ecu->state.dtc_setting_enabled == 0U) return false;

    /*
     * Debounce state is operation-cycle-local and changes frequently. Keep it
     * in RAM while the cycle is active; persistence is performed once at the
     * completed-cycle boundary rather than erasing flash per monitor sample.
     */
    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        link_uds_dtc_lifecycle_begin_operation_cycle(
            &ecu->dtc_lifecycle[index]);
        stm32f103_sync_dtc_lifecycle(ecu, index);
    }
    stm32f103_refresh_dtc_store(ecu);
    return true;
}

bool link_stm32f103_uds_ecu_report_dtc_test(
    LinkStm32F103UdsEcu *ecu,
    uint32_t code,
    LinkUdsDtcTestResult result)
{
    size_t index;

    if (ecu == NULL || ecu->state.dtc_setting_enabled == 0U) return false;
    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        if (stm32f103_dtc_definitions[index].code != code) continue;

        if (!link_uds_dtc_lifecycle_report_test(
                &stm32f103_dtc_definitions[index],
                &ecu->dtc_lifecycle[index],
                result)) {
            return false;
        }
        stm32f103_sync_dtc_lifecycle(ecu, index);
        stm32f103_refresh_dtc_store(ecu);
        return true;
    }
    return false;
}

bool link_stm32f103_uds_ecu_end_operation_cycle(
    LinkStm32F103UdsEcu *ecu)
{
    LinkStm32F103PersistentState old_state;
    LinkUdsDtcLifecycleState old_lifecycle[LINK_STM32F103_UDS_DTC_COUNT];
    size_t index;

    if (ecu == NULL || ecu->state.dtc_setting_enabled == 0U) return false;
    old_state = ecu->state;
    memcpy(old_lifecycle, ecu->dtc_lifecycle, sizeof(old_lifecycle));

    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        if (!link_uds_dtc_lifecycle_end_operation_cycle(
                &stm32f103_dtc_definitions[index],
                &ecu->dtc_lifecycle[index])) {
            ecu->state = old_state;
            memcpy(ecu->dtc_lifecycle, old_lifecycle, sizeof(old_lifecycle));
            stm32f103_refresh_dtc_store(ecu);
            return false;
        }
        stm32f103_sync_dtc_lifecycle(ecu, index);
    }

    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        memcpy(ecu->dtc_lifecycle, old_lifecycle, sizeof(old_lifecycle));
        stm32f103_refresh_dtc_store(ecu);
        return false;
    }
    return true;
}

bool link_stm32f103_uds_ecu_flush(LinkStm32F103UdsEcu *ecu)
{
    LinkStm32F103PersistentState candidate;
    LinkStm32F103PersistentState verify;
    const LinkStm32F103FlashStore *flash;
    size_t slot_count;
    size_t active_slot = 0U;
    size_t target_slot;
    size_t slot;
    bool active_found = false;
    uint32_t target = 0U;

    if (ecu == NULL || !stm32f103_flash_config_valid(&ecu->config)) {
        return false;
    }

    flash = &ecu->config.flash;
    slot_count = stm32f103_state_slot_count(flash);
    for (slot = 0U; slot < slot_count; ++slot) {
        uint32_t address = 0U;
        if (!stm32f103_state_slot_first_address(flash, slot, &address)) {
            return false;
        }
        if (address == ecu->active_page) {
            active_slot = slot;
            active_found = true;
            break;
        }
    }
    target_slot = active_found ? (active_slot + 1U) % slot_count : 0U;
    if (!stm32f103_state_slot_first_address(
            flash, target_slot, &target)) {
        return false;
    }

    candidate = ecu->state;
    candidate.magic = LINK_STM32F103_STATE_MAGIC;
    candidate.schema = LINK_STM32F103_STATE_SCHEMA;
    candidate.generation++;
    candidate.crc32 = stm32f103_crc32(
        &candidate, offsetof(LinkStm32F103PersistentState, crc32));

    if (!stm32f103_write_state_slot(flash, target_slot, &candidate) ||
        !stm32f103_read_state_slot(flash, target_slot, &verify) ||
        !stm32f103_state_valid(&verify) ||
        memcmp(&candidate, &verify, sizeof(candidate)) != 0) {
        return false;
    }

    ecu->state = candidate;
    ecu->active_page = target;
    stm32f103_refresh_dtc_store(ecu);
    return true;
}

static bool stm32f103_load_state(LinkStm32F103UdsEcu *ecu)
{
    LinkStm32F103PersistentState candidate;
    LinkStm32F103PersistentState newest = {0};
    const LinkStm32F103FlashStore *flash = &ecu->config.flash;
    const size_t slot_count = stm32f103_state_slot_count(flash);
    size_t slot;
    uint32_t newest_address = 0U;
    bool have_newest = false;

    for (slot = 0U; slot < slot_count; ++slot) {
        uint32_t address = 0U;
        bool valid;

        if (!stm32f103_state_slot_first_address(flash, slot, &address)) {
            return false;
        }
        valid = stm32f103_read_state_slot(flash, slot, &candidate) &&
                stm32f103_state_valid(&candidate);
        if (!valid) continue;

        if (!have_newest ||
            stm32f103_generation_newer(candidate.generation, newest.generation)) {
            newest = candidate;
            newest_address = address;
            have_newest = true;
        }
    }

    if (have_newest) {
        ecu->state = newest;
        ecu->active_page = newest_address;
        return true;
    }

    stm32f103_state_defaults(&ecu->state);
    if (!stm32f103_state_slot_first_address(
            flash, slot_count - 1U, &ecu->active_page)) {
        return false;
    }
    return link_stm32f103_uds_ecu_flush(ecu);
}

static LinkUdsServerHandlerResult stm32f103_bad_length(void)
{
    return link_uds_server_handler_negative(
        LINK_UDS_NRC_INCORRECT_MESSAGE_LENGTH_OR_INVALID_FORMAT);
}

static LinkUdsServerHandlerResult stm32f103_out_of_range(void)
{
    return link_uds_server_handler_negative(LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
}

static bool stm32f103_put_bytes(
    uint8_t *response,
    size_t capacity,
    size_t *offset,
    const uint8_t *data,
    size_t length)
{
    if (response == NULL || offset == NULL ||
        (length != 0U && data == NULL) ||
        *offset > capacity || capacity - *offset < length) {
        return false;
    }
    if (length != 0U) {
        memcpy(response + *offset, data, length);
        *offset += length;
    }
    return true;
}

static bool stm32f103_put_u16(
    uint8_t *response,
    size_t capacity,
    size_t *offset,
    uint16_t value)
{
    const uint8_t bytes[2U] = {
        (uint8_t)(value >> 8U), (uint8_t)value
    };
    return stm32f103_put_bytes(
        response, capacity, offset, bytes, sizeof(bytes));
}

static bool stm32f103_parse_memory(
    const LinkUdsServerRequest *request,
    bool has_data_format,
    uint32_t *address,
    uint32_t *memory_size,
    size_t *data_offset)
{
    size_t offset;
    uint8_t alfid;
    uint8_t address_width;
    uint8_t size_width;
    uint32_t parsed_address = 0U;
    uint32_t parsed_size = 0U;
    uint8_t index;

    if (request == NULL || request->pdu == NULL ||
        address == NULL || memory_size == NULL || data_offset == NULL) {
        return false;
    }

    offset = has_data_format ? 2U : 1U;
    if (request->pdu_length <= offset) return false;
    alfid = request->pdu[offset++];
    address_width = (uint8_t)(alfid & 0x0fU);
    size_width = (uint8_t)(alfid >> 4U);
    if (address_width == 0U || address_width > 4U ||
        size_width == 0U || size_width > 4U ||
        request->pdu_length < offset + address_width + size_width) {
        return false;
    }

    for (index = 0U; index < address_width; ++index) {
        parsed_address =
            (parsed_address << 8U) | request->pdu[offset++];
    }
    for (index = 0U; index < size_width; ++index) {
        parsed_size =
            (parsed_size << 8U) | request->pdu[offset++];
    }
    if (parsed_size == 0U) return false;
    *address = parsed_address;
    *memory_size = parsed_size;
    *data_offset = offset;
    return true;
}

static bool stm32f103_sandbox_range(uint32_t address, uint32_t length)
{
    return address <= LINK_STM32F103_UDS_SANDBOX_BYTES &&
           length <= LINK_STM32F103_UDS_SANDBOX_BYTES - address;
}

static LinkUdsServerHandlerResult stm32f103_read_did(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    uint16_t did;
    const uint8_t *data = NULL;
    uint8_t scalar_data[1U];
    size_t data_length = 0U;
    size_t offset = 0U;

    if (request->pdu_length != 3U) return stm32f103_bad_length();
    did = (uint16_t)(((uint16_t)request->pdu[1] << 8U) | request->pdu[2]);

    if (did == LINK_STM32F103_VIN_DID) {
        data = ecu->vin;
        data_length = sizeof(ecu->vin);
    } else if (did == LINK_STM32F103_USER_DID) {
        data = ecu->state.user_did;
        data_length = sizeof(ecu->state.user_did);
    } else if (did == LINK_STM32F103_SW_DID) {
        data = (const uint8_t *)LINK_VERSION_STRING;
        data_length = sizeof(LINK_VERSION_STRING) - 1U;
    } else if (did == LINK_STM32F103_DID_SPARE_PART_NUMBER) {
        data = stm32f103_spare_part_number;
        data_length = sizeof(stm32f103_spare_part_number) - 1U;
    } else if (did == LINK_STM32F103_DID_BOOT_SW_IDENTIFIER) {
        data = stm32f103_boot_sw_identifier;
        data_length = sizeof(stm32f103_boot_sw_identifier) - 1U;
    } else if (did == LINK_STM32F103_DID_ECU_SW_NUMBER) {
        data = stm32f103_ecu_sw_number;
        data_length = sizeof(stm32f103_ecu_sw_number);
    } else if (did == LINK_STM32F103_DID_APP_SW_NUMBER) {
        data = stm32f103_app_sw_number;
        data_length = sizeof(stm32f103_app_sw_number);
    } else if (did == LINK_STM32F103_DID_HW_NUMBER) {
        data = stm32f103_hw_number;
        data_length = sizeof(stm32f103_hw_number) - 1U;
    } else if (did == LINK_STM32F103_DID_ACTIVE_SESSION) {
        scalar_data[0U] = link_uds_server_active_session(&ecu->server);
        data = scalar_data;
        data_length = sizeof(scalar_data);
    } else if (did == LINK_STM32F103_DID_SYSTEM_SUPPLIER) {
        data = stm32f103_system_supplier;
        data_length = sizeof(stm32f103_system_supplier) - 1U;
    } else if (ecu->state.dynamic_defined != 0U &&
               did == ecu->state.dynamic_did) {
        if (ecu->state.dynamic_source_did == LINK_STM32F103_VIN_DID) {
            data = ecu->vin;
            data_length = sizeof(ecu->vin);
        } else if (ecu->state.dynamic_source_did == LINK_STM32F103_USER_DID) {
            data = ecu->state.user_did;
            data_length = sizeof(ecu->state.user_did);
        }
    }

    if (data == NULL) return stm32f103_out_of_range();
    if (!stm32f103_put_u16(response, capacity, &offset, did) ||
        !stm32f103_put_bytes(
            response, capacity, &offset, data, data_length)) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    return link_uds_server_handler_positive(offset);
}

static bool stm32f103_group_matches(uint32_t group, uint32_t code)
{
    if (group == UINT32_C(0x00ffffff) || group == code) return true;
    if ((group & UINT32_C(0x0000ffff)) == UINT32_C(0x0000ffff)) {
        return (group & UINT32_C(0x00ff0000)) ==
               (code & UINT32_C(0x00ff0000));
    }
    if ((group & UINT32_C(0x000000ff)) == UINT32_C(0x000000ff)) {
        return (group & UINT32_C(0x00ffff00)) ==
               (code & UINT32_C(0x00ffff00));
    }
    return false;
}

static LinkUdsServerHandlerResult stm32f103_clear_dtc(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request)
{
    LinkStm32F103PersistentState old_state;
    LinkUdsDtcLifecycleState
        old_lifecycle[LINK_STM32F103_UDS_DTC_COUNT];
    uint32_t group;
    size_t index;
    bool matched = false;

    if (request->pdu_length != 4U) return stm32f103_bad_length();
    group = ((uint32_t)request->pdu[1] << 16U) |
            ((uint32_t)request->pdu[2] << 8U) |
            request->pdu[3];
    old_state = ecu->state;
    memcpy(old_lifecycle, ecu->dtc_lifecycle, sizeof(old_lifecycle));
    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        if (!stm32f103_group_matches(group, stm32f103_dtc_definitions[index].code)) {
            continue;
        }
        matched = true;
        link_uds_dtc_lifecycle_clear(&ecu->dtc_lifecycle[index]);
        stm32f103_sync_dtc_lifecycle(ecu, index);
        ecu->state.dtc_permanent[index] = 0U;
    }
    if (!matched) {
        ecu->state = old_state;
        memcpy(ecu->dtc_lifecycle, old_lifecycle, sizeof(old_lifecycle));
        return stm32f103_out_of_range();
    }
    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        memcpy(ecu->dtc_lifecycle, old_lifecycle, sizeof(old_lifecycle));
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    return link_uds_server_handler_positive(0U);
}

static LinkUdsServerHandlerResult stm32f103_read_memory(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    uint32_t address;
    uint32_t memory_size;
    size_t data_offset;

    if (!stm32f103_parse_memory(
            request, false, &address, &memory_size, &data_offset) ||
        data_offset != request->pdu_length) {
        return stm32f103_bad_length();
    }
    if (!stm32f103_sandbox_range(address, memory_size)) {
        return stm32f103_out_of_range();
    }
    if (memory_size > capacity) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    memcpy(response, ecu->state.sandbox + address, memory_size);
    return link_uds_server_handler_positive(memory_size);
}

static LinkUdsServerHandlerResult stm32f103_scaling(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    uint16_t did;
    const uint8_t scaling[] = {0x01U, 0x00U, 0x10U};
    size_t offset = 0U;

    if (request->pdu_length != 3U) return stm32f103_bad_length();
    did = (uint16_t)(((uint16_t)request->pdu[1] << 8U) | request->pdu[2]);
    if (did != LINK_STM32F103_USER_DID &&
        did != LINK_STM32F103_VIN_DID) {
        return stm32f103_out_of_range();
    }
    if (!stm32f103_put_u16(response, capacity, &offset, did) ||
        !stm32f103_put_bytes(
            response, capacity, &offset, scaling, sizeof(scaling))) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult stm32f103_communication_control(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    if (request->pdu_length != 3U) return stm32f103_bad_length();
    if (capacity < 2U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    old_state = ecu->state;
    ecu->state.communication_control = request->subfunction;
    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    response[0] = request->subfunction;
    response[1] = request->pdu[2];
    return link_uds_server_handler_positive(2U);
}

static LinkUdsServerHandlerResult stm32f103_authentication(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    if (request->pdu_length != 2U) return stm32f103_bad_length();
    if (request->subfunction != 0U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    (void)link_uds_server_set_security_level(&ecu->server, 0U);
    response[0] = 0U;
    return link_uds_server_handler_positive(1U);
}

static LinkUdsServerHandlerResult stm32f103_periodic(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    size_t index;
    size_t offset = 0U;

    if (request->pdu_length < 3U) return stm32f103_bad_length();
    for (index = 2U; index < request->pdu_length; ++index) {
        const uint8_t record[2U] = {
            request->pdu[index],
            (uint8_t)(request->pdu[index] ^ UINT8_C(0x5a))
        };
        if (!stm32f103_put_bytes(
                response, capacity, &offset, record, sizeof(record))) {
            return link_uds_server_handler_negative(
                LINK_UDS_NRC_RESPONSE_TOO_LONG);
        }
    }
    return link_uds_server_handler_positive(offset);
}

static LinkUdsServerHandlerResult stm32f103_dynamic_did(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    uint16_t dynamic_did;
    uint16_t source_did;

    if (capacity < 3U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    if (request->subfunction == 0x01U) {
        if (request->pdu_length < 8U) return stm32f103_bad_length();
        dynamic_did = (uint16_t)(
            ((uint16_t)request->pdu[2] << 8U) | request->pdu[3]);
        source_did = (uint16_t)(
            ((uint16_t)request->pdu[4] << 8U) | request->pdu[5]);
        if (dynamic_did < LINK_STM32F103_DYNAMIC_DID_MIN ||
            dynamic_did > LINK_STM32F103_DYNAMIC_DID_MAX ||
            (source_did != LINK_STM32F103_VIN_DID &&
             source_did != LINK_STM32F103_USER_DID)) {
            return stm32f103_out_of_range();
        }
        old_state = ecu->state;
        ecu->state.dynamic_defined = 1U;
        ecu->state.dynamic_did = dynamic_did;
        ecu->state.dynamic_source_did = source_did;
    } else if (request->subfunction == 0x03U) {
        if (request->pdu_length != 4U) return stm32f103_bad_length();
        dynamic_did = (uint16_t)(
            ((uint16_t)request->pdu[2] << 8U) | request->pdu[3]);
        old_state = ecu->state;
        ecu->state.dynamic_defined = 0U;
        ecu->state.dynamic_did = dynamic_did;
        ecu->state.dynamic_source_did = 0U;
    } else {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }

    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    response[0] = request->subfunction;
    response[1] = (uint8_t)(dynamic_did >> 8U);
    response[2] = (uint8_t)dynamic_did;
    return link_uds_server_handler_positive(3U);
}

static LinkUdsServerHandlerResult stm32f103_write_did(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    uint16_t did;

    if (request->pdu_length != 3U + LINK_STM32F103_UDS_DID_BYTES) {
        return stm32f103_bad_length();
    }
    did = (uint16_t)(((uint16_t)request->pdu[1] << 8U) | request->pdu[2]);
    if (did != LINK_STM32F103_USER_DID) return stm32f103_out_of_range();
    if (capacity < 2U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    old_state = ecu->state;
    memcpy(
        ecu->state.user_did, request->pdu + 3U,
        LINK_STM32F103_UDS_DID_BYTES);
    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    response[0] = (uint8_t)(did >> 8U);
    response[1] = (uint8_t)did;
    return link_uds_server_handler_positive(2U);
}

static LinkUdsServerHandlerResult stm32f103_io_control(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    uint16_t did;
    if (request->pdu_length < 4U) return stm32f103_bad_length();
    did = (uint16_t)(((uint16_t)request->pdu[1] << 8U) | request->pdu[2]);
    if (did != LINK_STM32F103_USER_DID) return stm32f103_out_of_range();
    if (capacity < 4U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->pdu[1];
    response[1] = request->pdu[2];
    response[2] = request->pdu[3];
    response[3] = ecu->state.user_did[0U];
    return link_uds_server_handler_positive(4U);
}

static LinkUdsServerHandlerResult stm32f103_routine(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    uint16_t routine;
    uint8_t status = 0U;

    if (request->pdu_length < 4U) return stm32f103_bad_length();
    routine = (uint16_t)(((uint16_t)request->pdu[2] << 8U) | request->pdu[3]);
    if (request->subfunction < 1U || request->subfunction > 3U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }

    switch (routine) {
    case LINK_STM32F103_ROUTINE_INTEGRITY:
    case LINK_STM32F103_ROUTINE_CHECK_MEMORY:
        /*
         * The live RAM state may legitimately differ from its last persisted
         * CRC while monitor state is being refreshed. CheckMemory validates
         * the active flash journal slot instead of treating a stale in-RAM
         * CRC as flash corruption.
         */
        status = stm32f103_active_persisted_state_valid(ecu) ? 0U : 1U;
        break;

    case LINK_STM32F103_ROUTINE_ERASE_MEMORY:
        /*
         * Reference-target safety boundary: "EraseMemory" erases only the
         * non-executable programming sandbox, never application/boot flash.
         */
        if (request->subfunction == 1U) {
            old_state = ecu->state;
            memset(ecu->state.sandbox, 0xff, sizeof(ecu->state.sandbox));
            if (!link_stm32f103_uds_ecu_flush(ecu)) {
                ecu->state = old_state;
                stm32f103_refresh_dtc_store(ecu);
                return link_uds_server_handler_negative(
                    LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
            }
        }
        break;

    case LINK_STM32F103_ROUTINE_CHECK_PROGRAMMING_DEPENDENCIES:
        status =
            ecu->transfer_mode == LINK_STM32F103_TRANSFER_NONE &&
            stm32f103_active_persisted_state_valid(ecu)
                ? 0U : 1U;
        break;

    default:
        return stm32f103_out_of_range();
    }

    if (capacity < 4U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->subfunction;
    response[1] = request->pdu[2];
    response[2] = request->pdu[3];
    response[3] = status;
    return link_uds_server_handler_positive(4U);
}

static LinkUdsServerHandlerResult stm32f103_begin_transfer(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    bool upload,
    uint8_t *response,
    size_t capacity)
{
    uint32_t address;
    uint32_t memory_size;
    size_t offset;

    if (!stm32f103_parse_memory(
            request, true, &address, &memory_size, &offset) ||
        offset != request->pdu_length) {
        return stm32f103_bad_length();
    }
    if (!stm32f103_sandbox_range(address, memory_size)) {
        return stm32f103_out_of_range();
    }
    if (capacity < 3U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    ecu->transfer_mode = upload
        ? LINK_STM32F103_TRANSFER_UPLOAD
        : LINK_STM32F103_TRANSFER_DOWNLOAD;
    ecu->transfer_address = address;
    ecu->transfer_size = memory_size;
    ecu->transfer_offset = 0U;
    ecu->transfer_block = 1U;
    response[0] = 0x20U;
    response[1] = 0x01U;
    response[2] = 0x00U;
    return link_uds_server_handler_positive(3U);
}

static LinkUdsServerHandlerResult stm32f103_transfer_data(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    uint32_t remaining;
    size_t chunk;

    if (request->pdu_length < 2U) return stm32f103_bad_length();
    if (ecu->transfer_mode == LINK_STM32F103_TRANSFER_NONE) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_SEQUENCE_ERROR);
    }
    if (request->pdu[1] != ecu->transfer_block) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_SEQUENCE_ERROR);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }

    remaining = ecu->transfer_size - ecu->transfer_offset;
    response[0] = ecu->transfer_block;

    if (ecu->transfer_mode == LINK_STM32F103_TRANSFER_DOWNLOAD) {
        chunk = request->pdu_length - 2U;
        if (chunk == 0U || chunk > remaining) {
            return stm32f103_out_of_range();
        }
        memcpy(
            ecu->state.sandbox + ecu->transfer_address + ecu->transfer_offset,
            request->pdu + 2U, chunk);
        ecu->transfer_offset += (uint32_t)chunk;
        ecu->transfer_block++;
        return link_uds_server_handler_positive(1U);
    }

    if (request->pdu_length != 2U) return stm32f103_bad_length();
    chunk = remaining;
    if (chunk > LINK_STM32F103_MAX_TRANSFER_CHUNK) {
        chunk = LINK_STM32F103_MAX_TRANSFER_CHUNK;
    }
    if (capacity - 1U < chunk) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    memcpy(
        response + 1U,
        ecu->state.sandbox + ecu->transfer_address + ecu->transfer_offset,
        chunk);
    ecu->transfer_offset += (uint32_t)chunk;
    ecu->transfer_block++;
    return link_uds_server_handler_positive(chunk + 1U);
}

static LinkUdsServerHandlerResult stm32f103_transfer_exit(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request)
{
    LinkStm32F103PersistentState old_state;

    if (request->pdu_length != 1U) return stm32f103_bad_length();
    if (ecu->transfer_mode == LINK_STM32F103_TRANSFER_NONE) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_SEQUENCE_ERROR);
    }
    if (ecu->transfer_offset != ecu->transfer_size) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_SEQUENCE_ERROR);
    }

    old_state = ecu->state;
    if (ecu->transfer_mode == LINK_STM32F103_TRANSFER_DOWNLOAD &&
        !link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    ecu->transfer_mode = LINK_STM32F103_TRANSFER_NONE;
    ecu->transfer_address = 0U;
    ecu->transfer_size = 0U;
    ecu->transfer_offset = 0U;
    ecu->transfer_block = 0U;
    return link_uds_server_handler_positive(0U);
}

static LinkUdsServerHandlerResult stm32f103_file_transfer(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    if (request->pdu_length < 2U) return stm32f103_bad_length();
    if (request->pdu[1] == 0U || request->pdu[1] > 5U) {
        return stm32f103_out_of_range();
    }
    if (capacity < 4U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->pdu[1];
    response[1] = 0x20U;
    response[2] = 0x01U;
    response[3] = 0x00U;
    return link_uds_server_handler_positive(4U);
}

static LinkUdsServerHandlerResult stm32f103_write_memory(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    uint32_t address;
    uint32_t memory_size;
    size_t data_offset;
    size_t echo_length;

    if (!stm32f103_parse_memory(
            request, false, &address, &memory_size, &data_offset) ||
        request->pdu_length != data_offset + memory_size) {
        return stm32f103_bad_length();
    }
    if (!stm32f103_sandbox_range(address, memory_size)) {
        return stm32f103_out_of_range();
    }
    echo_length = data_offset - 1U;
    if (capacity < echo_length) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    old_state = ecu->state;
    memcpy(ecu->state.sandbox + address, request->pdu + data_offset, memory_size);
    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    memcpy(response, request->pdu + 1U, echo_length);
    return link_uds_server_handler_positive(echo_length);
}

static LinkUdsServerHandlerResult stm32f103_timing(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    if (request->pdu_length != 2U) return stm32f103_bad_length();
    if (request->subfunction == 0U || request->subfunction > 4U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 5U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->subfunction;
    response[1] = (uint8_t)(ecu->server.config.p2_server_max_ms >> 8U);
    response[2] = (uint8_t)ecu->server.config.p2_server_max_ms;
    response[3] = (uint8_t)(ecu->server.config.p2_star_server_max_10ms >> 8U);
    response[4] = (uint8_t)ecu->server.config.p2_star_server_max_10ms;
    return link_uds_server_handler_positive(5U);
}

static LinkUdsServerHandlerResult stm32f103_secured_data(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    const size_t record_length = request->pdu_length - 1U;
    if (request->pdu_length < 2U) return stm32f103_bad_length();
    if (capacity < record_length) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    memcpy(response, request->pdu + 1U, record_length);
    return link_uds_server_handler_positive(record_length);
}

static LinkUdsServerHandlerResult stm32f103_control_dtc_setting(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103PersistentState old_state;
    if (request->pdu_length != 2U) return stm32f103_bad_length();
    if (request->subfunction != 1U && request->subfunction != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    old_state = ecu->state;
    ecu->state.dtc_setting_enabled =
        request->subfunction == 1U ? 1U : 0U;
    if (!link_stm32f103_uds_ecu_flush(ecu)) {
        ecu->state = old_state;
        stm32f103_refresh_dtc_store(ecu);
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_GENERAL_PROGRAMMING_FAILURE);
    }
    response[0] = request->subfunction;
    return link_uds_server_handler_positive(1U);
}

static LinkUdsServerHandlerResult stm32f103_response_on_event(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    if (request->pdu_length != 2U) return stm32f103_bad_length();
    if (request->subfunction > 6U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
    if (capacity < 2U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->subfunction;
    response[1] = 0U;
    return link_uds_server_handler_positive(2U);
}

static LinkUdsServerHandlerResult stm32f103_link_control(
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    if (request->pdu_length < 2U) return stm32f103_bad_length();
    if (request->subfunction != 1U && request->subfunction != 2U) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_CONDITIONS_NOT_CORRECT);
    }
    if (capacity < 1U) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    response[0] = request->subfunction;
    return link_uds_server_handler_positive(1U);
}

static LinkUdsServerHandlerResult stm32f103_generic_handler(
    void *context,
    const LinkUdsServerRequest *request,
    uint8_t *response,
    size_t capacity)
{
    LinkStm32F103UdsEcu *ecu = (LinkStm32F103UdsEcu *)context;

    if (ecu == NULL || request == NULL || request->pdu == NULL ||
        response == NULL) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_GENERAL_REJECT);
    }

    switch (request->service) {
    case LINK_UDS_SERVICE_CLEAR_DIAGNOSTIC_INFORMATION:
        return stm32f103_clear_dtc(ecu, request);
    case LINK_UDS_SERVICE_READ_DTC_INFORMATION:
        return link_uds_server_dtc_handler(
            &ecu->dtc_store, request, response, capacity);
    case LINK_UDS_SERVICE_READ_DATA_BY_IDENTIFIER:
        return stm32f103_read_did(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_READ_MEMORY_BY_ADDRESS:
        return stm32f103_read_memory(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_READ_SCALING_DATA_BY_IDENTIFIER:
        return stm32f103_scaling(request, response, capacity);
    case LINK_UDS_SERVICE_COMMUNICATION_CONTROL:
        return stm32f103_communication_control(
            ecu, request, response, capacity);
    case LINK_UDS_SERVICE_AUTHENTICATION:
        return stm32f103_authentication(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_READ_DATA_BY_PERIODIC_IDENTIFIER:
        return stm32f103_periodic(request, response, capacity);
    case LINK_UDS_SERVICE_DYNAMICALLY_DEFINE_DATA_IDENTIFIER:
        return stm32f103_dynamic_did(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_WRITE_DATA_BY_IDENTIFIER:
        return stm32f103_write_did(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_INPUT_OUTPUT_CONTROL_BY_IDENTIFIER:
        return stm32f103_io_control(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_ROUTINE_CONTROL:
        return stm32f103_routine(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_REQUEST_DOWNLOAD:
        return stm32f103_begin_transfer(
            ecu, request, false, response, capacity);
    case LINK_UDS_SERVICE_REQUEST_UPLOAD:
        return stm32f103_begin_transfer(
            ecu, request, true, response, capacity);
    case LINK_UDS_SERVICE_TRANSFER_DATA:
        return stm32f103_transfer_data(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_REQUEST_TRANSFER_EXIT:
        return stm32f103_transfer_exit(ecu, request);
    case LINK_UDS_SERVICE_REQUEST_FILE_TRANSFER:
        return stm32f103_file_transfer(request, response, capacity);
    case LINK_UDS_SERVICE_WRITE_MEMORY_BY_ADDRESS:
        return stm32f103_write_memory(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_ACCESS_TIMING_PARAMETER:
        return stm32f103_timing(ecu, request, response, capacity);
    case LINK_UDS_SERVICE_SECURED_DATA_TRANSMISSION:
        return stm32f103_secured_data(request, response, capacity);
    case LINK_UDS_SERVICE_CONTROL_DTC_SETTING:
        return stm32f103_control_dtc_setting(
            ecu, request, response, capacity);
    case LINK_UDS_SERVICE_RESPONSE_ON_EVENT:
        return stm32f103_response_on_event(request, response, capacity);
    case LINK_UDS_SERVICE_LINK_CONTROL:
        return stm32f103_link_control(request, response, capacity);
    default:
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_SERVICE_NOT_SUPPORTED);
    }
}

static LinkUdsServerHandlerResult stm32f103_security_seed(
    void *context,
    uint8_t security_level,
    const uint8_t *request_record,
    size_t request_record_length,
    uint8_t *seed,
    size_t seed_capacity)
{
    LinkStm32F103UdsEcu *ecu = (LinkStm32F103UdsEcu *)context;
    const uint8_t *selected_seed;
    size_t selected_length;

    if (ecu == NULL || seed == NULL ||
        request_record_length != 0U ||
        request_record != NULL) {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
    }

    if (security_level == 1U) {
        selected_seed = stm32f103_level1_seed;
        selected_length = sizeof(stm32f103_level1_seed);
    } else if (security_level == 2U) {
        selected_seed = stm32f103_level2_seed;
        selected_length = sizeof(stm32f103_level2_seed);
    } else {
        return link_uds_server_handler_negative(
            LINK_UDS_NRC_REQUEST_OUT_OF_RANGE);
    }

    if (seed_capacity < selected_length) {
        return link_uds_server_handler_negative(LINK_UDS_NRC_RESPONSE_TOO_LONG);
    }
    memset(ecu->security_seed, 0, sizeof(ecu->security_seed));
    memcpy(ecu->security_seed, selected_seed, selected_length);
    memcpy(seed, selected_seed, selected_length);
    return link_uds_server_handler_positive(selected_length);
}

static bool stm32f103_security_verify(
    void *context,
    uint8_t security_level,
    const uint8_t *key,
    size_t key_length)
{
    LinkStm32F103UdsEcu *ecu = (LinkStm32F103UdsEcu *)context;

    if (ecu == NULL || key == NULL) return false;

    if (security_level == 1U) {
        uint8_t difference = 0U;
        size_t index;
        if (key_length != sizeof(stm32f103_level1_key)) return false;
        for (index = 0U; index < sizeof(stm32f103_level1_key); ++index) {
            difference |= (uint8_t)(key[index] ^ stm32f103_level1_key[index]);
        }
        return difference == 0U;
    }

    return security_level == 2U &&
           key_length == LINK_AES_CMAC_TAG_BYTES &&
           link_aes_cmac_128_verify(
               ecu->config.security_key,
               stm32f103_level2_seed,
               sizeof(stm32f103_level2_seed),
               key);
}

static bool stm32f103_register_handlers(LinkStm32F103UdsEcu *ecu)
{
    size_t index;
    for (index = 0U; index < link_uds_standard_service_count(); ++index) {
        const LinkUdsServiceDefinition *definition =
            link_uds_standard_service_at(index);
        if (definition == NULL) return false;
        switch (definition->service) {
        case LINK_UDS_SERVICE_DIAGNOSTIC_SESSION_CONTROL:
        case LINK_UDS_SERVICE_ECU_RESET:
        case LINK_UDS_SERVICE_SECURITY_ACCESS:
        case LINK_UDS_SERVICE_TESTER_PRESENT:
            break;
        default:
            if (!link_uds_server_set_handler(
                    &ecu->server, definition->service,
                    stm32f103_generic_handler, ecu)) {
                return false;
            }
            break;
        }
    }
    return true;
}

bool link_stm32f103_uds_ecu_init(
    LinkStm32F103UdsEcu *ecu,
    const LinkStm32F103UdsEcuConfig *config)
{
    LinkUdsServerConfig server_config = LINK_UDS_SERVER_CONFIG_INIT;

    if (ecu == NULL || !stm32f103_flash_config_valid(config)) return false;
    memset(ecu, 0, sizeof(*ecu));
    ecu->config = *config;
    memcpy(
        ecu->vin,
        config->vin != NULL ? config->vin : stm32f103_default_vin,
        sizeof(ecu->vin));
    if (!stm32f103_load_state(ecu)) return false;
    stm32f103_hydrate_dtc_lifecycle(ecu);
    stm32f103_refresh_dtc_store(ecu);

    server_config.enforce_session_sequence = true;
    server_config.s3_server_timeout_ms = UINT32_C(5000);
    server_config.clock_ms = config->clock_ms;
    server_config.clock_context = config->clock_context;
    server_config.policies = stm32f103_policies;
    server_config.policy_count =
        sizeof(stm32f103_policies) / sizeof(stm32f103_policies[0]);
    server_config.security_access.seed = stm32f103_security_seed;
    server_config.security_access.verify_key = stm32f103_security_verify;
    server_config.security_access.context = ecu;
    server_config.security_access.max_invalid_key_attempts = 3U;
    server_config.security_access.delay_ms = UINT32_C(1000);

    return link_uds_server_init(&ecu->server, &server_config) &&
           stm32f103_register_handlers(ecu);
}

LinkUdsServer *link_stm32f103_uds_ecu_server(LinkStm32F103UdsEcu *ecu)
{
    return ecu == NULL ? NULL : &ecu->server;
}

LinkUdsServerResult link_stm32f103_uds_ecu_handle(
    LinkStm32F103UdsEcu *ecu,
    const LinkUdsServerRequestContext *context,
    const uint8_t *request,
    size_t request_length,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    const LinkUdsServerRequestContext physical =
        LINK_UDS_SERVER_REQUEST_CONTEXT_INIT;
    if (ecu == NULL) return LINK_UDS_SERVER_RESULT_INVALID_ARGUMENT;
    return link_uds_server_handle_with_context(
        &ecu->server,
        context != NULL ? context : &physical,
        request, request_length,
        response, response_capacity, response_length);
}

bool link_stm32f103_uds_ecu_report_dtc(
    LinkStm32F103UdsEcu *ecu,
    uint32_t code,
    uint8_t status,
    int8_t fault_detection_counter,
    bool permanent_status)
{
    LinkStm32F103PersistentState old_state;
    size_t index;

    if (ecu == NULL || ecu->state.dtc_setting_enabled == 0U) return false;
    for (index = 0U; index < LINK_STM32F103_UDS_DTC_COUNT; ++index) {
        if (stm32f103_dtc_definitions[index].code != code) continue;
        LinkUdsDtcLifecycleState old_lifecycle =
            ecu->dtc_lifecycle[index];
        old_state = ecu->state;
        ecu->dtc_lifecycle[index].status = status;
        ecu->dtc_lifecycle[index].fault_detection_counter =
            fault_detection_counter;
        ecu->dtc_lifecycle[index].aging_counter = 0U;
        ecu->dtc_lifecycle[index].failure_cycle_count =
            (status & LINK_UDS_DTC_STATUS_CONFIRMED_DTC) != 0U
                ? stm32f103_dtc_definitions[index].confirmation_threshold_cycles
                : 0U;
        ecu->dtc_lifecycle[index].tested_this_cycle = false;
        ecu->dtc_lifecycle[index].failed_this_cycle = false;
        ecu->dtc_lifecycle[index].passed_this_cycle = false;
        stm32f103_sync_dtc_lifecycle(ecu, index);
        ecu->state.dtc_permanent[index] = permanent_status ? 1U : 0U;
        if (!link_stm32f103_uds_ecu_flush(ecu)) {
            ecu->state = old_state;
            ecu->dtc_lifecycle[index] = old_lifecycle;
            stm32f103_refresh_dtc_store(ecu);
            return false;
        }
        return true;
    }
    return false;
}
