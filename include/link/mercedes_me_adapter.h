// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_MERCEDES_ME_ADAPTER_H
#define LINK_MERCEDES_ME_ADAPTER_H

#include "link/features.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_MERCEDES_ME_SPP_UUID "00001101-0000-1000-8000-00805F9B34FB"
#define LINK_MERCEDES_ME_NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_NUS_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_NUS_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_CCCD_UUID "00002902-0000-1000-8000-00805F9B34FB"
#define LINK_MERCEDES_ME_TOSHIBA_SERVICE_UUID "e079c6a0-aa8b-11e3-a903-0002a5d5c51b"
#define LINK_MERCEDES_ME_TOSHIBA_CHARACTERISTIC_UUID "b38312c0-aa89-11e3-9cef-0002a5d5c51b"

#define LINK_MERCEDES_ME_REFERENCE_CLASSIC_CONNECT_TIMEOUT_MS 44000U
#define LINK_MERCEDES_ME_REFERENCE_MIN_CONNECTION_DURATION_MS 6000U
#define LINK_MERCEDES_ME_REFERENCE_BLE_MTU 512U

/* Byte-stream facts recovered from the official 4.7.61 Java/native bridge. */
#define LINK_MERCEDES_ME_RECORD_TERMINATOR UINT8_C(0x0D)
#define LINK_MERCEDES_ME_NACK_TERMINATOR UINT8_C(0x07)
#define LINK_MERCEDES_ME_RX_BUFFER_CAPACITY 700U
#define LINK_MERCEDES_ME_RX_CLEAR_THRESHOLD 698U
#define LINK_MERCEDES_ME_STATE_CALLBACK_SLEEP_SENTINEL 4711

typedef enum LinkMercedesMeAdapterFamily {
    LINK_MERCEDES_ME_ADAPTER_UNKNOWN = 0,
    LINK_MERCEDES_ME_ADAPTER_BLE,
    LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION,
    LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION,
    LINK_MERCEDES_ME_ADAPTER_OTHER_APPS
} LinkMercedesMeAdapterFamily;

typedef enum LinkMercedesMeQosState {
    LINK_MERCEDES_ME_QOS_STOPPED = 0,
    LINK_MERCEDES_ME_QOS_GOOD = 1,
    LINK_MERCEDES_ME_QOS_WEAK = 2,
    LINK_MERCEDES_ME_QOS_BAD = 3,
    LINK_MERCEDES_ME_QOS_UNKNOWN = 255
} LinkMercedesMeQosState;

typedef enum LinkMercedesMeExecutionState {
    LINK_MERCEDES_ME_EXECUTION_SUCCESS = 0,
    LINK_MERCEDES_ME_EXECUTION_STATE_ERROR = 1,
    LINK_MERCEDES_ME_EXECUTION_COMMUNICATION_ERROR = 2,
    LINK_MERCEDES_ME_EXECUTION_ADAPTER_ERROR = 3,
    LINK_MERCEDES_ME_EXECUTION_UNKNOWN = 255
} LinkMercedesMeExecutionState;

typedef enum LinkMercedesMeReason {
    LINK_MERCEDES_ME_REASON_STOPPED = 0,
    LINK_MERCEDES_ME_REASON_STARTED = 1,
    LINK_MERCEDES_ME_REASON_NAME_NOT_FOUND = 2,
    LINK_MERCEDES_ME_REASON_NO_PING = 3,
    LINK_MERCEDES_ME_REASON_WRONG_PARTMU_VERSION = 4,
    LINK_MERCEDES_ME_REASON_BT_NOT_ACTIVE = 5,
    LINK_MERCEDES_ME_REASON_DEVICE_DISAPPEARED = 6,
    LINK_MERCEDES_ME_REASON_DEVICE_NO_SPP = 7,
    LINK_MERCEDES_ME_REASON_NOT_CONNECTABLE = 8,
    LINK_MERCEDES_ME_REASON_ILLEGAL_BT_DISCOVERY_STATE = 9,
    LINK_MERCEDES_ME_REASON_CONNECT_TIMEOUT = 10,
    LINK_MERCEDES_ME_REASON_SMK_NOT_AVAILABLE = 11,
    LINK_MERCEDES_ME_REASON_SMK_INVALID_PASSKEY = 12,
    LINK_MERCEDES_ME_REASON_SMK_BLOCK_TEMPORARY = 13,
    LINK_MERCEDES_ME_REASON_ADAPTER_NO_AUTHENTICATION_01 = 14,
    LINK_MERCEDES_ME_REASON_ADAPTER_AUTHENTICATION_FAILED_02 = 15,
    LINK_MERCEDES_ME_REASON_ADAPTER_ADC_SELFTEST_ERROR_03 = 16,
    LINK_MERCEDES_ME_REASON_ADAPTER_MAC_ERROR_04 = 17,
    LINK_MERCEDES_ME_REASON_ADAPTER_SELFTEST_ERROR_05 = 18,
    LINK_MERCEDES_ME_REASON_ADAPTER_INVALID_MAC_MAPPING_11 = 19,
    LINK_MERCEDES_ME_REASON_ADAPTER_BT_RX_OVERFLOW_18 = 20,
    LINK_MERCEDES_ME_REASON_ADAPTER_CAN_OFF_21 = 21,
    LINK_MERCEDES_ME_REASON_ADAPTER_BT_TX_OVERFLOW_30 = 22,
    LINK_MERCEDES_ME_REASON_ADAPTER_CAN_RX_OVERFLOW_31 = 23,
    LINK_MERCEDES_ME_REASON_ADAPTER_CAN_TX_OVERFLOW_32 = 24,
    LINK_MERCEDES_ME_REASON_BUS_ERROR_35 = 25,
    LINK_MERCEDES_ME_REASON_GATT_FAILURE = 26,
    LINK_MERCEDES_ME_REASON_UNKNOWN = 255
} LinkMercedesMeReason;

typedef enum LinkMercedesMeCommState {
    LINK_MERCEDES_ME_COMM_STARTED = 1,
    LINK_MERCEDES_ME_COMM_NAME_NOT_FOUND = 2,
    LINK_MERCEDES_ME_COMM_BT_NOT_ACTIVE = 5,
    LINK_MERCEDES_ME_COMM_DEVICE_NO_SPP = 7,
    LINK_MERCEDES_ME_COMM_NOT_CONNECTABLE = 8,
    LINK_MERCEDES_ME_COMM_ILLEGAL_BT_DISCOVERY_STATE = 9,
    LINK_MERCEDES_ME_COMM_CONNECT_TIMEOUT = 10,
    LINK_MERCEDES_ME_COMM_GATT_FAILURE = 11,
    LINK_MERCEDES_ME_COMM_UNKNOWN = 255
} LinkMercedesMeCommState;

typedef struct LinkMercedesMeConnectionProblemDefinition {
    const char *name;
    int error_code;
} LinkMercedesMeConnectionProblemDefinition;

typedef enum LinkMercedesMeStreamEventKind {
    LINK_MERCEDES_ME_STREAM_RECORD = 0,
    LINK_MERCEDES_ME_STREAM_NACK,
    LINK_MERCEDES_ME_STREAM_OVERFLOW
} LinkMercedesMeStreamEventKind;

typedef void (*LinkMercedesMeStreamEventFn)(
    void *context,
    LinkMercedesMeStreamEventKind kind,
    const uint8_t *bytes,
    size_t size);

typedef struct LinkMercedesMeStreamParser {
    uint8_t bytes[LINK_MERCEDES_ME_RX_BUFFER_CAPACITY];
    size_t used;
    size_t overflow_count;
} LinkMercedesMeStreamParser;

#if LINK_ENABLE_MERCEDES_ME_NATIVE

LinkMercedesMeAdapterFamily link_mercedes_me_adapter_family_from_name(
    const char *name);
const char *link_mercedes_me_adapter_family_name(
    LinkMercedesMeAdapterFamily family);
bool link_mercedes_me_adapter_prefers_ble(
    LinkMercedesMeAdapterFamily family);
bool link_mercedes_me_adapter_prefers_classic_spp(
    LinkMercedesMeAdapterFamily family);

const char *link_mercedes_me_qos_state_name(int ordinal);
const char *link_mercedes_me_execution_state_name(int ordinal);
const char *link_mercedes_me_reason_name(int ordinal);
const char *link_mercedes_me_comm_state_name(int ordinal);

size_t link_mercedes_me_connection_problem_count(void);
const LinkMercedesMeConnectionProblemDefinition *
link_mercedes_me_connection_problem_at(size_t ordinal);
const LinkMercedesMeConnectionProblemDefinition *
link_mercedes_me_connection_problem_find_error_code(int error_code);

bool link_mercedes_me_command_has_valid_terminator(
    const uint8_t *bytes,
    size_t size);

void link_mercedes_me_stream_parser_init(
    LinkMercedesMeStreamParser *parser);

size_t link_mercedes_me_stream_parser_feed(
    LinkMercedesMeStreamParser *parser,
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeStreamEventFn event_fn,
    void *event_context);

#else

/*
 * Provider-disabled facade. The shared transport/controller can retain one
 * source path, while a non-Mercedes product resolves every provider hook
 * locally and can omit all Mercedes implementation translation units.
 */
static inline LinkMercedesMeAdapterFamily
link_mercedes_me_adapter_family_from_name(const char *name)
{
    (void)name;
    return LINK_MERCEDES_ME_ADAPTER_UNKNOWN;
}

static inline const char *link_mercedes_me_adapter_family_name(
    LinkMercedesMeAdapterFamily family)
{
    (void)family;
    return "disabled";
}

static inline bool link_mercedes_me_adapter_prefers_ble(
    LinkMercedesMeAdapterFamily family)
{
    (void)family;
    return false;
}

static inline bool link_mercedes_me_adapter_prefers_classic_spp(
    LinkMercedesMeAdapterFamily family)
{
    (void)family;
    return false;
}

static inline const char *link_mercedes_me_qos_state_name(int ordinal)
{
    (void)ordinal;
    return "disabled";
}

static inline const char *link_mercedes_me_execution_state_name(int ordinal)
{
    (void)ordinal;
    return "disabled";
}

static inline const char *link_mercedes_me_reason_name(int ordinal)
{
    (void)ordinal;
    return "disabled";
}

static inline const char *link_mercedes_me_comm_state_name(int ordinal)
{
    (void)ordinal;
    return "disabled";
}

static inline size_t link_mercedes_me_connection_problem_count(void)
{
    return 0U;
}

static inline const LinkMercedesMeConnectionProblemDefinition *
link_mercedes_me_connection_problem_at(size_t ordinal)
{
    (void)ordinal;
    return NULL;
}

static inline const LinkMercedesMeConnectionProblemDefinition *
link_mercedes_me_connection_problem_find_error_code(int error_code)
{
    (void)error_code;
    return NULL;
}

static inline bool link_mercedes_me_command_has_valid_terminator(
    const uint8_t *bytes,
    size_t size)
{
    (void)bytes;
    (void)size;
    return false;
}

static inline void link_mercedes_me_stream_parser_init(
    LinkMercedesMeStreamParser *parser)
{
    if (parser != NULL) {
        parser->used = 0U;
        parser->overflow_count = 0U;
    }
}

static inline size_t link_mercedes_me_stream_parser_feed(
    LinkMercedesMeStreamParser *parser,
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeStreamEventFn event_fn,
    void *event_context)
{
    (void)parser;
    (void)bytes;
    (void)size;
    (void)event_fn;
    (void)event_context;
    return 0U;
}

#endif

#ifdef __cplusplus
}
#endif
#endif
