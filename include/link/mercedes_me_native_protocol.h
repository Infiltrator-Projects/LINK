// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_MERCEDES_ME_NATIVE_PROTOCOL_H
#define LINK_MERCEDES_ME_NATIVE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_MERCEDES_ME_SESSION_MASTER_KEY_SIZE 32U
#define LINK_MERCEDES_ME_SESSION_RANDOM_SIZE 16U
#define LINK_MERCEDES_ME_SESSION_KEY_SIZE 32U
#define LINK_MERCEDES_ME_AES_BLOCK_SIZE 16U
#define LINK_MERCEDES_ME_SECURE_HEADER_SIZE 6U
#define LINK_MERCEDES_ME_SECURE_MAX_CIPHERTEXT 512U
#define LINK_MERCEDES_ME_SECURE_MAX_PLAINTEXT 505U
#define LINK_MERCEDES_ME_SECURE_WIRE_MAX_SIZE 686U

#define LINK_MERCEDES_ME_SYNC_COMMAND_TIMEOUT_MS 6900U
#define LINK_MERCEDES_ME_CAN_ID_MIN 0U
#define LINK_MERCEDES_ME_CAN_ID_MAX 0x7FFU
#define LINK_MERCEDES_ME_CAN_FILTER_BLOCK_ALL 0x7FFU
#define LINK_MERCEDES_ME_CAN_FILTER_MAX_IDS 15U
#define LINK_MERCEDES_ME_RAW_CAN_MAX_PAYLOAD 8U
#define LINK_MERCEDES_ME_ISOTP_MAX_PAYLOAD 100U
#define LINK_MERCEDES_ME_OBDII_REQUEST_ID 0x7DFU
#define LINK_MERCEDES_ME_DEFAULT_BAUD_ORDINAL 6U
#define LINK_MERCEDES_ME_ADAPTER_VIN_MIN_LENGTH 1U
#define LINK_MERCEDES_ME_ADAPTER_VIN_MAX_LENGTH 17U
#define LINK_MERCEDES_ME_RAW_RX_TIMEOUT_DEFAULT_MS 400U
#define LINK_MERCEDES_ME_RAW_RX_TIMEOUT_MIN_MS 200U
#define LINK_MERCEDES_ME_RAW_RX_TIMEOUT_MAX_MS 10000U
#define LINK_MERCEDES_ME_ISOTP_P2STAR_MAX_MS 10000U
#define LINK_MERCEDES_ME_ISOTP_P3_MAX_MS 5000U
#define LINK_MERCEDES_ME_X_MODE_MIN 0x01U
#define LINK_MERCEDES_ME_X_MODE_MAX 0x29U
#define LINK_MERCEDES_ME_X_SLEEP_MAX_VALUE 99U
#define LINK_MERCEDES_ME_QOS_LOOP_QUEUE_SIZE 10U
#define LINK_MERCEDES_ME_QOS_LOOP_AGE_MS 3000U
#define LINK_MERCEDES_ME_QOS_STATE_MAX_AGE_MS 90000U
#define LINK_MERCEDES_ME_DEAD_MAN_DEFAULT_STOP_DELAY_S 30U
#define LINK_MERCEDES_ME_DEAD_MAN_STARTING_TIMEOUT_MS 3000U
#define LINK_MERCEDES_ME_DEAD_MAN_MAX_TRY_COUNT 3U
#define LINK_MERCEDES_ME_USE_NO_RESPONSE_MODE 1

#define LINK_MERCEDES_ME_CMD_CAN_CLOSE 'C'
#define LINK_MERCEDES_ME_CMD_CAN_OPEN 'O'
#define LINK_MERCEDES_ME_CMD_SET_BAUD 'S'
#define LINK_MERCEDES_ME_CMD_FILTER_CODE 'M'
#define LINK_MERCEDES_ME_CMD_FILTER_MASK 'm'
#define LINK_MERCEDES_ME_CMD_TIMESTAMP 'Z'
#define LINK_MERCEDES_ME_CMD_ECHO 'E'
#define LINK_MERCEDES_ME_CMD_RAW_CAN 't'
#define LINK_MERCEDES_ME_CMD_ISOTP_CONFIG 'I'
#define LINK_MERCEDES_ME_CMD_ISOTP_TRANSCEIVE 'i'
#define LINK_MERCEDES_ME_CMD_GET_SEED 'y'
#define LINK_MERCEDES_ME_CMD_SET_KEY 'Y'
#define LINK_MERCEDES_ME_CMD_GET_PASSKEY 'p'
#define LINK_MERCEDES_ME_CMD_STATUS 'V'
#define LINK_MERCEDES_ME_CMD_HW_INFO 'N'
#define LINK_MERCEDES_ME_CMD_X 'X'
#define LINK_MERCEDES_ME_CMD_SECURE 'a'

#define LINK_MERCEDES_ME_CAN_THROTTLE_RUN_STATE "F10"
#define LINK_MERCEDES_ME_CAN_THROTTLE_MESSAGE_LIMIT "F11"
#define LINK_MERCEDES_ME_CAN_THROTTLE_FILTER "F12"
#define LINK_MERCEDES_ME_CAN_THROTTLE_RESET "F13"

typedef enum LinkMercedesMeNativeResult {
    LINK_MERCEDES_ME_NATIVE_OK = 0,
    LINK_MERCEDES_ME_NATIVE_INVALID_ARGUMENT,
    LINK_MERCEDES_ME_NATIVE_RANGE,
    LINK_MERCEDES_ME_NATIVE_CAPACITY,
    LINK_MERCEDES_ME_NATIVE_MALFORMED,
    LINK_MERCEDES_ME_NATIVE_BASE64,
    LINK_MERCEDES_ME_NATIVE_CRC_MISMATCH
} LinkMercedesMeNativeResult;

typedef enum LinkMercedesMeXMode {
    LINK_MERCEDES_ME_X_BT_MAC_RESET = 0x01,
    LINK_MERCEDES_ME_X_SETGET_DATA = 0x10,
    LINK_MERCEDES_ME_X_GET_BLUETOOTH_LINK_KEY = 0x19,
    LINK_MERCEDES_ME_X_ENTER_SLEEP = 0x20,
    LINK_MERCEDES_ME_X_SET_ADAPTER_SLEEP_PERIOD = 0x22,
    LINK_MERCEDES_ME_X_SET_CAN_REPEAT_COUNT = 0x23,
    LINK_MERCEDES_ME_X_SET_CAN_REPEAT_WAITTIME = 0x24,
    LINK_MERCEDES_ME_X_SET_IGNITION_OFF_VOLTAGE_THRESHOLD = 0x25,
    LINK_MERCEDES_ME_X_SET_APP_LAUNCH_MODE = 0x28,
    LINK_MERCEDES_ME_X_SET_ADAPTER_SPP_MODE = 0x29
} LinkMercedesMeXMode;

const char *link_mercedes_me_native_result_name(LinkMercedesMeNativeResult result);
uint16_t link_mercedes_me_crc16_ccitt(const uint8_t *bytes, size_t size);
size_t link_mercedes_me_secure_ciphertext_size(size_t plaintext_size);

LinkMercedesMeNativeResult link_mercedes_me_derive_session_key(
    const uint8_t session_master_key[LINK_MERCEDES_ME_SESSION_MASTER_KEY_SIZE],
    const uint8_t random_argument_1[LINK_MERCEDES_ME_SESSION_RANDOM_SIZE],
    const uint8_t random_argument_2[LINK_MERCEDES_ME_SESSION_RANDOM_SIZE],
    uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE]);

LinkMercedesMeNativeResult link_mercedes_me_secure_encode(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *plaintext,
    size_t plaintext_size,
    uint8_t *wire,
    size_t wire_capacity,
    size_t *wire_size);

LinkMercedesMeNativeResult link_mercedes_me_secure_decode(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *wire,
    size_t wire_size,
    uint8_t *plaintext,
    size_t plaintext_capacity,
    size_t *plaintext_size);

LinkMercedesMeNativeResult link_mercedes_me_build_can_open(
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_can_close(
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_status(
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_hw_info(
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_get_passkey(
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_set_baudrate(
    unsigned int baud_ordinal,
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_get_seed(
    const uint8_t *payload, size_t payload_size,
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_set_key(
    const uint8_t *payload, size_t payload_size,
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_get_x(
    unsigned int mode,
    uint8_t *out, size_t capacity, size_t *out_size);
LinkMercedesMeNativeResult link_mercedes_me_build_set_x(
    unsigned int mode,
    const uint8_t *payload, size_t payload_size,
    uint8_t *out, size_t capacity, size_t *out_size);

#ifdef __cplusplus
}
#endif
#endif
