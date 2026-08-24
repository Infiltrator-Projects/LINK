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
    LinkTransportReceiveFn receiver;
    void *receiver_context;
    bool open;
} LinkLinuxSerialTransport;

void link_linux_serial_init(LinkLinuxSerialTransport *transport);
bool link_linux_serial_open(LinkLinuxSerialTransport *transport,
                            const char *device,
                            unsigned int baud_rate);
void link_linux_serial_close(LinkLinuxSerialTransport *transport);
bool link_linux_serial_is_open(const LinkLinuxSerialTransport *transport);
LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport);
size_t link_linux_serial_discover(char paths[][256], size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
