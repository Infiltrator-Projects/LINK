// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_LINUX_SERIAL_H
#define LINK_LINUX_SERIAL_H

#include "link/transport.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LinkLinuxSerialTransport {
    int fd;
    char device[256];
    unsigned int baud_rate;
    LinkTransportReceiveFn receiver;
    void *receiver_context;
    bool connected;
} LinkLinuxSerialTransport;

void link_linux_serial_init(LinkLinuxSerialTransport *transport);
bool link_linux_serial_configure(LinkLinuxSerialTransport *transport,
                                 const char *device,
                                 unsigned int baud_rate);
void link_linux_serial_disconnect(LinkLinuxSerialTransport *transport);
bool link_linux_serial_is_connected(const LinkLinuxSerialTransport *transport);
bool link_linux_serial_probe_elm327(LinkLinuxSerialTransport *transport,
                                    char *identity,
                                    size_t identity_capacity);
void link_linux_serial_pump(LinkLinuxSerialTransport *transport);
LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport);
size_t link_linux_serial_discover(char paths[][256], size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
