// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mercedes_me_diagnostic.h
 * @brief Bridge from LINK diagnostic requests to the retired Mercedes me Adapter.
 *
 * This module is deliberately small: it translates a transport-neutral ISO-TP
 * request into the exact GDK I/i command strings recovered from the archived
 * Mercedes me Adapter 4.7.61 native libraries. Bluetooth lifecycle and session
 * authentication remain separate provider concerns.
 */
#ifndef LINK_MERCEDES_ME_DIAGNOSTIC_H
#define LINK_MERCEDES_ME_DIAGNOSTIC_H

#include "link/diagnostic_request.h"
#include "link/mercedes_me_native_protocol.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_MERCEDES_ME_ISOTP_CONFIG_WIRE_SIZE 16U
#define LINK_MERCEDES_ME_ISOTP_TRANSACTION_WIRE_MAX_SIZE 144U

typedef struct LinkMercedesMeDiagnosticPlan {
    uint8_t config_command[LINK_MERCEDES_ME_ISOTP_CONFIG_WIRE_SIZE];
    size_t config_command_size;
    uint8_t transaction_command[LINK_MERCEDES_ME_ISOTP_TRANSACTION_WIRE_MAX_SIZE];
    size_t transaction_command_size;
} LinkMercedesMeDiagnosticPlan;

/**
 * Translate one 11-bit normal-addressed diagnostic request to the native
 * Mercedes me Adapter command pair.
 *
 * The genuine adapter command encoding currently proves one request and one
 * response CAN identifier. Consequently response_can_id_known is required.
 * Extended-ID requests remain rejected until the archived/native evidence
 * proves that command encoding.
 */
LinkMercedesMeNativeResult link_mercedes_me_build_diagnostic_plan(
    const LinkDiagnosticRequestDefinition *request,
    bool allow_raw_can_responses,
    LinkMercedesMeDiagnosticPlan *plan);

/**
 * Wrap either command from a plan in the recovered AES-256/Base64 secure
 * envelope. The caller owns session establishment and supplies the exact
 * derived 32-byte session key.
 */
LinkMercedesMeNativeResult link_mercedes_me_secure_diagnostic_command(
    const uint8_t session_key[LINK_MERCEDES_ME_SESSION_KEY_SIZE],
    const uint8_t *command,
    size_t command_size,
    uint8_t *wire,
    size_t wire_capacity,
    size_t *wire_size);

#ifdef __cplusplus
}
#endif
#endif
