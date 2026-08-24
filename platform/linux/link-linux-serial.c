// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/linux_serial.h"

#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

static speed_t serial_speed(unsigned int baud)
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

static LinkTransportStatus serial_connect(void *context)
{
    LinkLinuxSerialTransport *transport = context;
    struct termios tty;
    speed_t speed;
    if (transport == NULL || transport->device[0] == '\0')
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (transport->connected) return LINK_TRANSPORT_OK;
    transport->fd = open(transport->device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (transport->fd < 0) return LINK_TRANSPORT_IO_ERROR;
    if (tcgetattr(transport->fd, &tty) != 0) {
        close(transport->fd);
        transport->fd = -1;
        return LINK_TRANSPORT_IO_ERROR;
    }
    speed = serial_speed(transport->baud_rate);
    (void)cfsetispeed(&tty, speed);
    (void)cfsetospeed(&tty, speed);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_oflag &= ~OPOST;
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 1;
    if (tcsetattr(transport->fd, TCSANOW, &tty) != 0) {
        close(transport->fd);
        transport->fd = -1;
        return LINK_TRANSPORT_IO_ERROR;
    }
    (void)tcflush(transport->fd, TCIOFLUSH);
    transport->connected = true;
    return LINK_TRANSPORT_OK;
}

static void serial_disconnect(void *context)
{
    link_linux_serial_disconnect((LinkLinuxSerialTransport *)context);
}

static bool serial_is_connected(void *context)
{
    return link_linux_serial_is_connected((const LinkLinuxSerialTransport *)context);
}

static LinkTransportStatus serial_write(void *context,
                                        const uint8_t *bytes,
                                        size_t size)
{
    LinkLinuxSerialTransport *transport = context;
    size_t offset = 0U;
    if (transport == NULL || bytes == NULL || size == 0U)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (!transport->connected || transport->fd < 0)
        return LINK_TRANSPORT_NOT_CONNECTED;
    while (offset < size) {
        ssize_t written = write(transport->fd, bytes + offset, size - offset);
        if (written < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                struct pollfd descriptor = { transport->fd, POLLOUT, 0 };
                if (poll(&descriptor, 1, 500) > 0) continue;
            }
            return LINK_TRANSPORT_IO_ERROR;
        }
        offset += (size_t)written;
    }
    return LINK_TRANSPORT_OK;
}

static void serial_set_receiver(void *context,
                                LinkTransportReceiveFn receiver,
                                void *receiver_context)
{
    LinkLinuxSerialTransport *transport = context;
    if (transport == NULL) return;
    transport->receiver = receiver;
    transport->receiver_context = receiver_context;
}

void link_linux_serial_init(LinkLinuxSerialTransport *transport)
{
    if (transport == NULL) return;
    memset(transport, 0, sizeof(*transport));
    transport->fd = -1;
    transport->baud_rate = 38400U;
}

bool link_linux_serial_configure(LinkLinuxSerialTransport *transport,
                                 const char *device,
                                 unsigned int baud_rate)
{
    if (transport == NULL || device == NULL || device[0] == '\0') return false;
    if (transport->connected) link_linux_serial_disconnect(transport);
    (void)snprintf(transport->device, sizeof(transport->device), "%s", device);
    transport->baud_rate = baud_rate == 0U ? 38400U : baud_rate;
    return true;
}

void link_linux_serial_disconnect(LinkLinuxSerialTransport *transport)
{
    if (transport == NULL) return;
    if (transport->fd >= 0) close(transport->fd);
    transport->fd = -1;
    transport->connected = false;
}

bool link_linux_serial_is_connected(const LinkLinuxSerialTransport *transport)
{
    return transport != NULL && transport->connected && transport->fd >= 0;
}

bool link_linux_serial_probe_elm327(LinkLinuxSerialTransport *transport,
                                    char *identity,
                                    size_t identity_capacity)
{
    static const uint8_t command[] = { 'A', 'T', 'I', '\r' };
    char response[512];
    size_t used = 0U;
    int elapsed = 0;
    if (identity != NULL && identity_capacity != 0U) identity[0] = '\0';
    if (!link_linux_serial_is_connected(transport)) return false;
    (void)tcflush(transport->fd, TCIFLUSH);
    if (serial_write(transport, command, sizeof(command)) != LINK_TRANSPORT_OK) return false;
    while (elapsed < 1800 && used + 1U < sizeof(response)) {
        struct pollfd descriptor = { transport->fd, POLLIN, 0 };
        int result = poll(&descriptor, 1, 100);
        elapsed += 100;
        if (result < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (result == 0) continue;
        if ((descriptor.revents & POLLIN) != 0) {
            ssize_t count = read(transport->fd, response + used, sizeof(response) - used - 1U);
            if (count > 0) {
                used += (size_t)count;
                response[used] = '\0';
                if (strchr(response, '>') != NULL) break;
            }
        }
    }
    response[used] = '\0';
    if (used == 0U || strchr(response, '>') == NULL) return false;
    if (identity != NULL && identity_capacity != 0U) {
        size_t source = 0U;
        size_t target = 0U;
        while (source < used && target + 1U < identity_capacity) {
            char c = response[source++];
            if (c == '>' || c == '\r' || c == '\n') {
                if (target != 0U) break;
                continue;
            }
            if (c >= 32 && c < 127) identity[target++] = c;
        }
        identity[target] = '\0';
    }
    return true;
}

void link_linux_serial_pump(LinkLinuxSerialTransport *transport)
{
    uint8_t buffer[1024];
    if (!link_linux_serial_is_connected(transport) || transport->receiver == NULL) return;
    for (;;) {
        ssize_t count = read(transport->fd, buffer, sizeof(buffer));
        if (count > 0) {
            transport->receiver(transport->receiver_context, buffer, (size_t)count);
            continue;
        }
        if (count < 0 && errno == EINTR) continue;
        break;
    }
}

LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport)
{
    LinkTransport result = LINK_TRANSPORT_INIT;
    result.context = transport;
    result.connect = serial_connect;
    result.disconnect = serial_disconnect;
    result.is_connected = serial_is_connected;
    result.write = serial_write;
    result.set_receiver = serial_set_receiver;
    return result;
}

size_t link_linux_serial_discover(char paths[][256], size_t capacity)
{
    static const char *prefixes[] = { "ttyUSB", "ttyACM", "rfcomm" };
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
            size_t length = strlen(prefixes[prefix_index]);
            if (strncmp(entry->d_name, prefixes[prefix_index], length) == 0) {
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
void link_linux_serial_init(LinkLinuxSerialTransport *transport) { if (transport != NULL) { memset(transport, 0, sizeof(*transport)); transport->fd = -1; } }
bool link_linux_serial_configure(LinkLinuxSerialTransport *transport, const char *device, unsigned int baud_rate) { (void)transport; (void)device; (void)baud_rate; return false; }
void link_linux_serial_disconnect(LinkLinuxSerialTransport *transport) { (void)transport; }
bool link_linux_serial_is_connected(const LinkLinuxSerialTransport *transport) { (void)transport; return false; }
bool link_linux_serial_probe_elm327(LinkLinuxSerialTransport *transport, char *identity, size_t identity_capacity) { (void)transport; if (identity != NULL && identity_capacity != 0U) identity[0] = '\0'; return false; }
void link_linux_serial_pump(LinkLinuxSerialTransport *transport) { (void)transport; }
LinkTransport link_linux_serial_as_transport(LinkLinuxSerialTransport *transport) { LinkTransport result = LINK_TRANSPORT_INIT; (void)transport; return result; }
size_t link_linux_serial_discover(char paths[][256], size_t capacity) { (void)paths; (void)capacity; return 0U; }
#endif
