/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "link/discover.h"

static link_safety_result result(link_safety_decision decision,
                                 link_safety_reason reason,
                                 uint8_t service)
{
    link_safety_result value;
    value.decision = decision;
    value.reason = reason;
    value.service = service;
    return value;
}

link_safety_result link_safety_classify(const uint8_t *payload, size_t length)
{
    uint8_t service;

    if (payload == NULL || length == 0U) {
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_EMPTY_REQUEST,
                      0U);
    }

    service = payload[0];
    switch (service) {
    case 0x01U:
    case 0x03U:
    case 0x07U:
    case 0x09U:
    case 0x0AU:
        return result(LINK_SAFETY_ALLOW_READ_ONLY,
                      LINK_SAFETY_REASON_ALLOWED_OBD_READ,
                      service);
    case 0x04U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_DTC_CLEAR,
                      service);
    case 0x08U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_WRITE_OR_CONTROL,
                      service);
    default:
        break;
    }

    /*
     * Discover is intentionally more restrictive than the codec layer.
     * Adding a standards-correct request builder must never make a state-
     * changing, security, or programming service transmissible by accident.
     * TesterPresent (0x3E) is a non-mutating keepalive and is permitted only
     * with the same read-only Discover boundary as identity/DTC requests.
     */
    switch (service) {
    case 0x19U:
    case 0x22U:
    case 0x3EU:
        return result(LINK_SAFETY_ALLOW_READ_ONLY,
                      LINK_SAFETY_REASON_ALLOWED_UDS_READ,
                      service);
    case 0x18U:
    case 0x1AU:
    case 0x21U:
        return result(LINK_SAFETY_ALLOW_READ_ONLY,
                      LINK_SAFETY_REASON_ALLOWED_KWP_READ,
                      service);
    case 0x11U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_ECU_RESET,
                      service);
    case 0x14U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_DTC_CLEAR,
                      service);
    case 0x27U:
    case 0x29U:
    case 0x84U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_SECURITY_ACCESS,
                      service);
    case 0x10U:
    case 0x28U:
    case 0x2CU:
    case 0x2EU:
    case 0x2FU:
    case 0x3DU:
    case 0x83U:
    case 0x85U:
    case 0x86U:
    case 0x87U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_WRITE_OR_CONTROL,
                      service);
    case 0x31U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_ROUTINE_CONTROL,
                      service);
    case 0x34U:
    case 0x35U:
    case 0x36U:
    case 0x37U:
    case 0x38U:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_PROGRAMMING,
                      service);
    default:
        return result(LINK_SAFETY_BLOCK,
                      LINK_SAFETY_REASON_DENY_BY_DEFAULT,
                      service);
    }
}

const char *link_safety_reason_string(link_safety_reason reason)
{
    switch (reason) {
    case LINK_SAFETY_REASON_ALLOWED_OBD_READ:
        return "allowed OBD read";
    case LINK_SAFETY_REASON_ALLOWED_UDS_READ:
        return "allowed UDS read/keepalive";
    case LINK_SAFETY_REASON_ALLOWED_KWP_READ:
        return "allowed KWP2000 read";
    case LINK_SAFETY_REASON_EMPTY_REQUEST:
        return "empty request";
    case LINK_SAFETY_REASON_WRITE_OR_CONTROL:
        return "write/control blocked";
    case LINK_SAFETY_REASON_ECU_RESET:
        return "ECU reset blocked";
    case LINK_SAFETY_REASON_SECURITY_ACCESS:
        return "security/authentication blocked";
    case LINK_SAFETY_REASON_ROUTINE_CONTROL:
        return "routine control blocked";
    case LINK_SAFETY_REASON_DTC_CLEAR:
        return "DTC clear blocked";
    case LINK_SAFETY_REASON_PROGRAMMING:
        return "programming/transfer blocked";
    case LINK_SAFETY_REASON_DENY_BY_DEFAULT:
        return "blocked by default";
    default:
        return "unknown safety decision";
    }
}
