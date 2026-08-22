// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file transport.c
 * @brief Validation for LINK's platform-neutral byte-stream transport ABI.
 */
#include "link/transport.h"

bool link_transport_is_valid(const LinkTransport *transport)
{
    if (transport == NULL || transport->struct_size < sizeof(*transport) ||
        transport->abi_version != LINK_TRANSPORT_ABI) {
        return false;
    }

    return transport->connect != NULL &&
           transport->disconnect != NULL &&
           transport->is_connected != NULL &&
           transport->write != NULL &&
           transport->set_receiver != NULL;
}
