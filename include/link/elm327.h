// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file elm327.h
 * @brief Transport-independent ELM327 command, parser and initialisation API.
 *
 * This layer owns serial command framing, bounded response accumulation,
 * prompt/echo handling, adapter-status classification and deterministic adapter
 * initialisation. It intentionally knows nothing about BLE, sockets or serial
 * device APIs; those concerns terminate at link/transport.h.
 */
#ifndef LINK_ELM327_H
#define LINK_ELM327_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_ELM327_MAX_COMMAND 64U
#define LINK_ELM327_MAX_RESPONSE 4096U
#define LINK_ELM327_MAX_ADAPTER_ID 96U

typedef enum LinkElm327Result {
    LINK_ELM327_RESULT_OK = 0,
    LINK_ELM327_RESULT_MORE_DATA,
    LINK_ELM327_RESULT_INVALID_ARGUMENT,
    LINK_ELM327_RESULT_COMMAND_TOO_LONG,
    LINK_ELM327_RESULT_RESPONSE_TOO_LONG,
    LINK_ELM327_RESULT_NO_DATA,
    LINK_ELM327_RESULT_STOPPED,
    LINK_ELM327_RESULT_UNABLE_TO_CONNECT,
    LINK_ELM327_RESULT_BUS_INIT_ERROR,
    LINK_ELM327_RESULT_CAN_ERROR,
    LINK_ELM327_RESULT_BUFFER_FULL,
    LINK_ELM327_RESULT_UNSUPPORTED_COMMAND,
    LINK_ELM327_RESULT_ADAPTER_ERROR,
    LINK_ELM327_RESULT_MALFORMED_RESPONSE
} LinkElm327Result;

/**
 * ELM327 protocol numbers used by first-generation OBD-II scan tools.
 *
 * Protocols 1..9 are the regulated J1979 transport choices exposed by the
 * ELM327 command surface. A/B/C are retained for honest adapter reporting but
 * are not classified as classic SAE J1979 OBD-II transports.
 */
typedef enum LinkElm327ProtocolNumber {
    LINK_ELM327_PROTOCOL_AUTOMATIC = 0x00,
    LINK_ELM327_PROTOCOL_SAE_J1850_PWM = 0x01,
    LINK_ELM327_PROTOCOL_SAE_J1850_VPW = 0x02,
    LINK_ELM327_PROTOCOL_ISO_9141_2 = 0x03,
    LINK_ELM327_PROTOCOL_ISO_14230_4_SLOW = 0x04,
    LINK_ELM327_PROTOCOL_ISO_14230_4_FAST = 0x05,
    LINK_ELM327_PROTOCOL_ISO_15765_4_11_500 = 0x06,
    LINK_ELM327_PROTOCOL_ISO_15765_4_29_500 = 0x07,
    LINK_ELM327_PROTOCOL_ISO_15765_4_11_250 = 0x08,
    LINK_ELM327_PROTOCOL_ISO_15765_4_29_250 = 0x09,
    LINK_ELM327_PROTOCOL_SAE_J1939 = 0x0A,
    LINK_ELM327_PROTOCOL_USER1_CAN = 0x0B,
    LINK_ELM327_PROTOCOL_USER2_CAN = 0x0C
} LinkElm327ProtocolNumber;

typedef enum LinkElm327ProtocolFamily {
    LINK_ELM327_PROTOCOL_FAMILY_AUTOMATIC = 0,
    LINK_ELM327_PROTOCOL_FAMILY_SAE_J1850,
    LINK_ELM327_PROTOCOL_FAMILY_ISO_9141_2,
    LINK_ELM327_PROTOCOL_FAMILY_ISO_14230_4,
    LINK_ELM327_PROTOCOL_FAMILY_ISO_15765_4,
    LINK_ELM327_PROTOCOL_FAMILY_SAE_J1939,
    LINK_ELM327_PROTOCOL_FAMILY_USER_DEFINED
} LinkElm327ProtocolFamily;

typedef enum LinkElm327ProtocolInit {
    LINK_ELM327_PROTOCOL_INIT_NONE = 0,
    LINK_ELM327_PROTOCOL_INIT_FIVE_BAUD,
    LINK_ELM327_PROTOCOL_INIT_FAST
} LinkElm327ProtocolInit;

typedef struct LinkElm327ProtocolDefinition {
    uint8_t number;
    const char *name;
    LinkElm327ProtocolFamily family;
    uint32_t bit_rate;
    bool extended_can_id;
    LinkElm327ProtocolInit init;
    bool classic_j1979_obd;
} LinkElm327ProtocolDefinition;

size_t link_elm327_protocol_definition_count(void);
const LinkElm327ProtocolDefinition *link_elm327_protocol_definition_at(size_t index);
const LinkElm327ProtocolDefinition *link_elm327_protocol_definition(uint8_t number);
const char *link_elm327_protocol_family_name(LinkElm327ProtocolFamily family);
LinkElm327Result link_elm327_build_set_protocol_command(
    uint8_t protocol_number,
    char *buffer,
    size_t buffer_size);

typedef struct LinkElm327Response {
    LinkElm327Result result;
    bool prompt_seen;
    bool echo_removed;
    bool searching_seen;
    bool ok_seen;
    size_t line_count;
    size_t length;
    char text[LINK_ELM327_MAX_RESPONSE];
} LinkElm327Response;

typedef struct LinkElm327Parser {
    char command[LINK_ELM327_MAX_COMMAND];
    uint8_t raw[LINK_ELM327_MAX_RESPONSE];
    size_t raw_length;
    bool prompt_seen;
    bool overflowed;
} LinkElm327Parser;

typedef enum LinkElm327InitStage {
    LINK_ELM327_INIT_RESET = 0,
    LINK_ELM327_INIT_ECHO_OFF,
    LINK_ELM327_INIT_LINEFEEDS_OFF,
    LINK_ELM327_INIT_SPACES_OFF,
    LINK_ELM327_INIT_HEADERS_OFF,
    LINK_ELM327_INIT_PROTOCOL_AUTO,
    LINK_ELM327_INIT_IDENTIFY,
    LINK_ELM327_INIT_COMPLETE,
    LINK_ELM327_INIT_FAILED
} LinkElm327InitStage;

typedef struct LinkElm327InitState {
    LinkElm327InitStage stage;
    LinkElm327Result failure;
    char adapter_id[LINK_ELM327_MAX_ADAPTER_ID];
} LinkElm327InitState;

const char *link_elm327_result_name(LinkElm327Result result);

/**
 * Validate and frame one printable ASCII ELM command.
 * Leading/trailing whitespace is removed and exactly one CR is appended.
 */
LinkElm327Result link_elm327_build_command(const char *command,
                                           uint8_t *buffer,
                                           size_t buffer_size,
                                           size_t *written);

/** Reset a bounded parser for one outstanding command. */
LinkElm327Result link_elm327_parser_begin(LinkElm327Parser *parser,
                                          const char *command);

/**
 * Feed an arbitrary response fragment. `consumed` stops at the first prompt so
 * bytes belonging to a subsequent command are never silently discarded.
 */
LinkElm327Result link_elm327_parser_feed(LinkElm327Parser *parser,
                                         const uint8_t *data,
                                         size_t size,
                                         size_t *consumed);

/** Normalise and classify a response after a prompt has been observed. */
LinkElm327Result link_elm327_parser_finish(const LinkElm327Parser *parser,
                                           LinkElm327Response *response);

void link_elm327_init_begin(LinkElm327InitState *state);
const char *link_elm327_init_command(const LinkElm327InitState *state);

/** Advance the deterministic ATZ/ATE0/ATL0/ATS0/ATH0/ATSP0/ATI sequence. */
LinkElm327Result link_elm327_init_accept(LinkElm327InitState *state,
                                         const LinkElm327Response *response);

#ifdef __cplusplus
}
#endif

#endif
