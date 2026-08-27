// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_LINUX_OPENPORT2_H
#define LINK_LINUX_OPENPORT2_H

#include "link/linux_serial.h"

#include <stdbool.h>
#include <stddef.h>

#if defined(__linux__)

/* J2534 RxStatus bits emitted by the vendored OpenPort backend. */
#define LINK_LINUX_OPENPORT2_RX_TX_LOOPBACK 0x01UL
#define LINK_LINUX_OPENPORT2_RX_START_OF_MESSAGE 0x02UL
#define LINK_LINUX_OPENPORT2_RX_TX_DONE 0x08UL

static inline bool link_linux_openport2_rx_status_is_complete_vehicle(
    unsigned long rx_status)
{
    return (rx_status &
        (LINK_LINUX_OPENPORT2_RX_TX_LOOPBACK |
         LINK_LINUX_OPENPORT2_RX_START_OF_MESSAGE |
         LINK_LINUX_OPENPORT2_RX_TX_DONE)) == 0UL;
}

bool link_linux_openport2_is_selection(const char *device);
size_t link_linux_openport2_discover(char paths[][256], size_t capacity);

LinkTransportStatus link_linux_openport2_connect(
    LinkLinuxSerialTransport *transport);
void link_linux_openport2_disconnect(LinkLinuxSerialTransport *transport);
bool link_linux_openport2_is_connected(
    const LinkLinuxSerialTransport *transport);
LinkTransportStatus link_linux_openport2_write(
    LinkLinuxSerialTransport *transport,
    const uint8_t *bytes,
    size_t size);
void link_linux_openport2_pump(LinkLinuxSerialTransport *transport);
bool link_linux_openport2_probe(
    LinkLinuxSerialTransport *transport,
    char *identity,
    size_t identity_capacity);
void link_linux_openport2_destroy(LinkLinuxSerialTransport *transport);

#endif

#endif
