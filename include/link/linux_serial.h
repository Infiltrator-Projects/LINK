// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_LINUX_SERIAL_H
#define LINK_LINUX_SERIAL_H

#include "link/transport.h"
#include "link/mercedes_me_adapter.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Linux ELM-compatible transport.
 *
 * The historical name is retained for source compatibility.  The provider
 * accepts ordinary tty/rfcomm paths, native BlueZ BLE / Bluetooth Classic
 * selections and native Tactrix OpenPort 2.0 USB selections returned by
 * link_linux_serial_discover(). BLE uses BlueZ D-Bus/GATT, Classic SPP uses a
 * native RFCOMM socket with SDP discovery, and OpenPort 2.0 uses LINK's own
 * libusb/J2534 provider. The historical "serial" API name is retained for
 * source compatibility; the object is now a general Linux diagnostic-adapter
 * transport.
 */
typedef struct LinkLinuxSerialTransport {
    int fd;
    char device[256];
    unsigned int baud_rate;
    LinkTransportReceiveFn receiver;
    void *receiver_context;
    void *provider_context;
    bool connected;
    bool bluetooth_le;
    bool bluetooth_classic;
    bool openport2;
    LinkAdapterKind adapter_kind;
    LinkMercedesMeAdapterFamily mercedes_me_family;
} LinkLinuxSerialTransport;

void link_linux_serial_init(LinkLinuxSerialTransport *transport);
bool link_linux_serial_configure(LinkLinuxSerialTransport *transport,
                                 const char *device,
                                 unsigned int baud_rate);
void link_linux_serial_disconnect(LinkLinuxSerialTransport *transport);
bool link_linux_serial_is_connected(const LinkLinuxSerialTransport *transport);
/**
 * Verify and identify the selected diagnostic adapter.
 *
 * For compatibility the historical function name is retained. ELM adapters
 * are queried with ATI; LINK-native OpenPort 2.0 selections return their
 * Tactrix/J2534 firmware identity without pretending the USB device is an ELM.
 */
bool link_linux_serial_probe_elm327(LinkLinuxSerialTransport *transport,
                                    char *identity,
                                    size_t identity_capacity);
bool link_linux_serial_probe_adapter(LinkLinuxSerialTransport *transport,
                                     char *identity,
                                     size_t identity_capacity);
LinkAdapterKind link_linux_serial_adapter_kind(
    const LinkLinuxSerialTransport *transport);
bool link_linux_serial_native_protocol_mode(
    const LinkLinuxSerialTransport *transport);
void link_linux_serial_pump(LinkLinuxSerialTransport *transport);
LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport);
size_t link_linux_serial_discover(char paths[][256], size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
