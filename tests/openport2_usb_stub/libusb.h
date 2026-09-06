/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Test-only libusb surface. Never included in a product target. */
#ifndef LINK_TEST_LIBUSB_H
#define LINK_TEST_LIBUSB_H
#include <stdint.h>
#include <sys/types.h>
typedef struct libusb_device { int unused; } libusb_device;
struct libusb_context { int unused; };
struct libusb_device_handle { int unused; };
struct libusb_device_descriptor { uint16_t idVendor, idProduct; };
struct libusb_endpoint_descriptor { uint8_t bmAttributes, bEndpointAddress; };
struct libusb_interface_descriptor {
    uint8_t bNumEndpoints, bInterfaceNumber;
    const struct libusb_endpoint_descriptor *endpoint;
};
struct libusb_interface {
    int num_altsetting;
    const struct libusb_interface_descriptor *altsetting;
};
struct libusb_config_descriptor {
    uint8_t bNumInterfaces;
    const struct libusb_interface *interface;
};
enum {
    LIBUSB_SUCCESS = 0, LIBUSB_ERROR_IO = -1,
    LIBUSB_ERROR_INVALID_PARAM = -2, LIBUSB_ERROR_ACCESS = -3,
    LIBUSB_ERROR_NO_DEVICE = -4, LIBUSB_ERROR_NOT_FOUND = -5,
    LIBUSB_ERROR_BUSY = -6, LIBUSB_ERROR_TIMEOUT = -7,
    LIBUSB_ERROR_OVERFLOW = -8, LIBUSB_ERROR_PIPE = -9,
    LIBUSB_ERROR_INTERRUPTED = -10, LIBUSB_ERROR_NO_MEM = -11,
    LIBUSB_ERROR_NOT_SUPPORTED = -12, LIBUSB_ERROR_OTHER = -99,
    LIBUSB_TRANSFER_TYPE_MASK = 3, LIBUSB_TRANSFER_TYPE_BULK = 2,
    LIBUSB_ENDPOINT_IN = 0x80, LIBUSB_ENDPOINT_OUT = 0
};
int libusb_init(struct libusb_context **);
void libusb_exit(struct libusb_context *);
ssize_t libusb_get_device_list(struct libusb_context *, libusb_device ***);
void libusb_free_device_list(libusb_device **, int);
int libusb_get_device_descriptor(libusb_device *, struct libusb_device_descriptor *);
int libusb_open(libusb_device *, struct libusb_device_handle **);
void libusb_close(struct libusb_device_handle *);
uint8_t libusb_get_device_address(libusb_device *);
int libusb_get_config_descriptor(libusb_device *, uint8_t, struct libusb_config_descriptor **);
void libusb_free_config_descriptor(struct libusb_config_descriptor *);
int libusb_kernel_driver_active(struct libusb_device_handle *, int);
int libusb_detach_kernel_driver(struct libusb_device_handle *, int);
int libusb_claim_interface(struct libusb_device_handle *, int);
int libusb_release_interface(struct libusb_device_handle *, int);
int libusb_bulk_transfer(struct libusb_device_handle *, unsigned char, unsigned char *, int, int *, unsigned int);
const char *libusb_error_name(int);
#endif
