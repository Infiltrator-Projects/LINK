// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/linux_serial.h"

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

static speed_t link_linux_serial_speed(unsigned int baud)
{
    switch (baud) {
    case 9600U: return B9600;
    case 19200U: return B19200;
    case 38400U: return B38400;
    case 57600U: return B57600;
    case 115200U: return B115200;
    default: return B38400;
    }
}

static LinkTransportStatus serial_send(void *context,
                                       const uint8_t *bytes,
                                       size_t size)
{
    LinkLinuxSerialTransport *transport = context;
    size_t offset = 0U;
    if (transport == NULL || !transport->open || bytes == NULL || size == 0U)
        return LINK_TRANSPORT_STATUS_INVALID_ARGUMENT;
    while (offset < size) {
        ssize_t written = write(transport->fd, bytes + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            return LINK_TRANSPORT_STATUS_IO_ERROR;
        }
        offset += (size_t)written;
    }
    return LINK_TRANSPORT_STATUS_OK;
}

static LinkTransportStatus serial_set_receiver(void *context,
                                                LinkTransportReceiveFn receiver,
                                                void *receiver_context)
{
    LinkLinuxSerialTransport *transport = context;
    if (transport == NULL) return LINK_TRANSPORT_STATUS_INVALID_ARGUMENT;
    transport->receiver = receiver;
    transport->receiver_context = receiver_context;
    return LINK_TRANSPORT_STATUS_OK;
}

static void serial_close_adapter(void *context)
{
    link_linux_serial_close((LinkLinuxSerialTransport *)context);
}

void link_linux_serial_init(LinkLinuxSerialTransport *transport)
{
    if (transport == NULL) return;
    memset(transport, 0, sizeof(*transport));
    transport->fd = -1;
}

bool link_linux_serial_open(LinkLinuxSerialTransport *transport,
                            const char *device,
                            unsigned int baud_rate)
{
    struct termios tty;
    speed_t speed;
    if (transport == NULL || device == NULL || device[0] == '\0') return false;
    link_linux_serial_close(transport);
    transport->fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (transport->fd < 0) return false;
    if (tcgetattr(transport->fd, &tty) != 0) {
        link_linux_serial_close(transport);
        return false;
    }
    speed = link_linux_serial_speed(baud_rate);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    if (tcsetattr(transport->fd, TCSANOW, &tty) != 0) {
        link_linux_serial_close(transport);
        return false;
    }
    (void)snprintf(transport->device, sizeof(transport->device), "%s", device);
    transport->open = true;
    return true;
}

void link_linux_serial_close(LinkLinuxSerialTransport *transport)
{
    if (transport == NULL) return;
    if (transport->fd >= 0) close(transport->fd);
    transport->fd = -1;
    transport->open = false;
    transport->receiver = NULL;
    transport->receiver_context = NULL;
    transport->device[0] = '\0';
}

bool link_linux_serial_is_open(const LinkLinuxSerialTransport *transport)
{
    return transport != NULL && transport->open;
}

LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport)
{
    LinkTransport result = {0};
    result.context = transport;
    result.send = serial_send;
    result.set_receiver = serial_set_receiver;
    result.close = serial_close_adapter;
    return result;
}

size_t link_linux_serial_discover(char paths[][256], size_t capacity)
{
    static const char *prefixes[] = {"ttyUSB", "ttyACM", "rfcomm"};
    DIR *directory;
    struct dirent *entry;
    size_t count = 0U;
    size_t prefix_index;
    if (paths == NULL || capacity == 0U) return 0U;
    directory = opendir("/dev");
    if (directory == NULL) return 0U;
    while ((entry = readdir(directory)) != NULL && count < capacity) {
        bool match = false;
        for (prefix_index = 0U; prefix_index < sizeof(prefixes) / sizeof(prefixes[0]); ++prefix_index) {
            if (strncmp(entry->d_name, prefixes[prefix_index], strlen(prefixes[prefix_index])) == 0) {
                match = true;
                break;
            }
        }
        if (match) {
            (void)snprintf(paths[count], 256U, "/dev/%s", entry->d_name);
            count++;
        }
    }
    closedir(directory);
    return count;
}
#else
void link_linux_serial_init(LinkLinuxSerialTransport *transport) { if (transport) transport->fd = -1; }
bool link_linux_serial_open(LinkLinuxSerialTransport *transport, const char *device, unsigned int baud_rate) { (void)transport; (void)device; (void)baud_rate; return false; }
void link_linux_serial_close(LinkLinuxSerialTransport *transport) { (void)transport; }
bool link_linux_serial_is_open(const LinkLinuxSerialTransport *transport) { (void)transport; return false; }
LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport) { LinkTransport result = {0}; (void)transport; return result; }
size_t link_linux_serial_discover(char paths[][256], size_t capacity) { (void)paths; (void)capacity; return 0U; }
#endif
