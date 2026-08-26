from pathlib import Path


def replace(path, old, new):
    p = Path(path)
    s = p.read_text()
    if old not in s:
        raise SystemExit(f"expected block not found in {path}: {old[:120]!r}")
    p.write_text(s.replace(old, new, 1))


Path("VERSION").write_text("0.14.3\n")

replace(
    "include/link/linux_serial.h",
    """ * accepts ordinary tty/rfcomm paths and native BlueZ BLE selections returned
 * by link_linux_serial_discover().  BLE is implemented through BlueZ D-Bus
 * and GATT; no rfcomm bridge, bluetoothctl subprocess or adapter-specific
 * UUID is required.
""",
    """ * accepts ordinary tty/rfcomm paths plus native BlueZ BLE and Bluetooth
 * Classic SPP selections returned by link_linux_serial_discover(). BLE uses
 * BlueZ D-Bus/GATT. Classic SPP uses a native RFCOMM socket with SDP service
 * discovery, so callers do not need to create /dev/rfcomm* manually.
""",
)
replace(
    "include/link/linux_serial.h",
    """    bool connected;
    bool bluetooth_le;
""",
    """    bool connected;
    bool bluetooth_le;
    bool bluetooth_classic;
""",
)

replace(
    "CMakeLists.txt",
    """    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux")
    target_link_libraries(${target} PRIVATE LINK::Core)
""",
    """    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/platform/linux")
    find_path(LINK_BLUEZ_INCLUDE_DIR bluetooth/bluetooth.h REQUIRED)
    find_library(LINK_BLUEZ_LIBRARY bluetooth REQUIRED)
    target_include_directories(${target} PRIVATE "${LINK_BLUEZ_INCLUDE_DIR}")
    target_link_libraries(${target} PRIVATE LINK::Core "${LINK_BLUEZ_LIBRARY}")
""",
)

p = Path("platform/linux/link-linux-serial.c")
s = p.read_text()
s = s.replace(
    "#include <gio/gio.h>\n#include <poll.h>\n",
    "#include <gio/gio.h>\n#include <bluetooth/bluetooth.h>\n#include <bluetooth/rfcomm.h>\n#include <bluetooth/sdp.h>\n#include <bluetooth/sdp_lib.h>\n#include <poll.h>\n",
    1,
)
s = s.replace(
    "#include <stdlib.h>\n#include <termios.h>\n",
    "#include <stdlib.h>\n#include <sys/socket.h>\n#include <termios.h>\n",
    1,
)
s = s.replace(
    "#define LINK_BLE_SCAN_MS 1000\n",
    "#define LINK_BLE_SCAN_MS 1000\n#define LINK_CLASSIC_SCAN_MS 1200\n#define LINK_CLASSIC_CONNECT_TIMEOUT_MS 8000\n",
    1,
)
s = s.replace(
    """static bool ble_extract_address(const char *device,
                                char address[18]);
static bool ble_name_likely_elm(const char *name);
""",
    """static bool ble_extract_address(const char *device,
                                char address[18]);
static bool classic_extract_address(const char *device,
                                    char address[18]);
static bool ble_name_likely_elm(const char *name);
static bool bluetooth_name_prefers_classic(const char *name);
static bool bluetooth_name_prefers_ble(const char *name);
""",
    1,
)
s = s.replace(
    """static size_t ble_discover_devices(char paths[][256], size_t capacity);
static bool ble_refresh_le_presence""",
    """static size_t ble_discover_devices(char paths[][256], size_t capacity);
static size_t classic_discover_devices(char paths[][256], size_t capacity);
static int classic_spp_channel(const char *address);
static LinkTransportStatus classic_connect(LinkLinuxSerialTransport *transport);
static bool ble_refresh_le_presence""",
    1,
)

marker = "static bool ble_name_likely_elm(const char *name)\n"
helper = r'''static bool classic_extract_address(const char *device,
                                    char address[18])
{
    size_t index;
    if (device == NULL || address == NULL || strncmp(device, "BT:", 3U) != 0)
        return false;
    for (index = 0U; index < 17U; ++index) {
        const char c = device[3U + index];
        if (c == '\0') return false;
        if ((index + 1U) % 3U == 0U) {
            if (c != ':') return false;
        } else if (!g_ascii_isxdigit(c)) {
            return false;
        }
        address[index] = c;
    }
    address[17] = '\0';
    return device[20] == '\0' || device[20] == ' ';
}

'''
if marker not in s:
    raise SystemExit("BLE name marker missing")
s = s.replace(marker, helper + marker, 1)

marker = "static int ble_discovered_compare(const void *left, const void *right)\n"
helper = r'''static bool bluetooth_name_contains(const char *name, const char *needle)
{
    gchar *lower_name;
    gchar *lower_needle;
    bool found;
    if (name == NULL || needle == NULL) return false;
    lower_name = g_ascii_strdown(name, -1);
    lower_needle = g_ascii_strdown(needle, -1);
    if (lower_name == NULL || lower_needle == NULL) {
        g_free(lower_name);
        g_free(lower_needle);
        return false;
    }
    found = strstr(lower_name, lower_needle) != NULL;
    g_free(lower_name);
    g_free(lower_needle);
    return found;
}

static bool bluetooth_name_prefers_classic(const char *name)
{
    return bluetooth_name_contains(name, "android-vlink") ||
           bluetooth_name_contains(name, "android vlink") ||
           bluetooth_name_contains(name, "android_vlink");
}

static bool bluetooth_name_prefers_ble(const char *name)
{
    return bluetooth_name_contains(name, "ios-vlink") ||
           bluetooth_name_contains(name, "ios vlink") ||
           bluetooth_name_contains(name, "ios_vlink");
}

'''
if marker not in s:
    raise SystemExit("compare marker missing")
s = s.replace(marker, helper + marker, 1)

old = """    qsort(devices, device_count, sizeof(devices[0]), ble_discovered_compare);
    if (device_count > capacity) device_count = capacity;
    for (index = 0U; index < device_count; ++index) {
        (void)snprintf(paths[index], 256U, "BLE:%s %s",
                       devices[index].address, devices[index].name);
    }
    return device_count;
}

static bool ble_refresh_le_presence"""
new = r'''    qsort(devices, device_count, sizeof(devices[0]), ble_discovered_compare);
    {
        size_t written_count = 0U;
        for (index = 0U; index < device_count && written_count < capacity; ++index) {
            if (!devices[index].likely_elm ||
                bluetooth_name_prefers_classic(devices[index].name)) continue;
            (void)snprintf(paths[written_count], 256U, "BLE:%s %s",
                           devices[index].address, devices[index].name);
            written_count++;
        }
        return written_count;
    }
}

static size_t classic_discover_devices(char paths[][256], size_t capacity)
{
    GError *error = NULL;
    GDBusConnection *bus;
    char adapter_path[256];
    GVariant *reply;
    bool discovery_started = false;
    gint64 deadline;
    GVariant *objects;
    GVariantIter iterator;
    const gchar *object_path;
    GVariant *interfaces;
    LinkLinuxBleDiscovered devices[LINK_BLE_MAX_DISCOVERED];
    size_t device_count = 0U;
    size_t index;
    size_t written_count = 0U;

    if (paths == NULL || capacity == 0U) return 0U;
    bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) g_error_free(error);
    if (bus == NULL) return 0U;
    if (!bluez_find_adapter(bus, adapter_path, sizeof(adapter_path))) {
        g_object_unref(bus);
        return 0U;
    }

    {
        GVariantBuilder filter;
        g_variant_builder_init(&filter, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&filter, "{sv}", "Transport",
                              g_variant_new_string("bredr"));
        reply = bluez_call(bus, adapter_path, LINK_BLUEZ_ADAPTER,
                           "SetDiscoveryFilter",
                           g_variant_new("(a{sv})", &filter),
                           G_VARIANT_TYPE("()"),
                           LINK_BLE_CALL_TIMEOUT_MS);
        if (reply != NULL) g_variant_unref(reply);
    }
    reply = bluez_call(bus, adapter_path, LINK_BLUEZ_ADAPTER,
                       "StartDiscovery", NULL, G_VARIANT_TYPE("()"),
                       LINK_BLE_CALL_TIMEOUT_MS);
    if (reply != NULL) {
        discovery_started = true;
        g_variant_unref(reply);
    }

    deadline = g_get_monotonic_time() + (gint64)LINK_CLASSIC_SCAN_MS * 1000;
    while (g_get_monotonic_time() < deadline) {
        while (g_main_context_pending(NULL))
            (void)g_main_context_iteration(NULL, FALSE);
        g_usleep(25000U);
    }

    objects = bluez_managed_objects(bus);
    if (objects != NULL) {
        g_variant_iter_init(&iterator, objects);
        while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                                   &object_path, &interfaces)) {
            GVariant *properties;
            const gchar *address = NULL;
            const gchar *name = NULL;
            gint16 rssi = 0;
            bool has_rssi = false;
            (void)object_path;
            properties = g_variant_lookup_value(
                interfaces, LINK_BLUEZ_DEVICE, G_VARIANT_TYPE("a{sv}"));
            if (properties != NULL && device_count < LINK_BLE_MAX_DISCOVERED &&
                g_variant_lookup(properties, "Address", "&s", &address) &&
                address != NULL && strlen(address) == 17U) {
                if (!g_variant_lookup(properties, "Alias", "&s", &name) ||
                    name == NULL || name[0] == '\0') {
                    (void)g_variant_lookup(properties, "Name", "&s", &name);
                }
                has_rssi = g_variant_lookup(properties, "RSSI", "n", &rssi) != FALSE;
                (void)snprintf(devices[device_count].address,
                               sizeof(devices[device_count].address), "%s", address);
                (void)snprintf(devices[device_count].name,
                               sizeof(devices[device_count].name), "%s",
                               name != NULL && name[0] != '\0'
                                   ? name : "Bluetooth Classic device");
                devices[device_count].likely_elm = ble_name_likely_elm(name);
                devices[device_count].has_rssi = has_rssi;
                devices[device_count].rssi = rssi;
                device_count++;
            }
            if (properties != NULL) g_variant_unref(properties);
            g_variant_unref(interfaces);
        }
        g_variant_unref(objects);
    }

    if (discovery_started) {
        reply = bluez_call(bus, adapter_path, LINK_BLUEZ_ADAPTER,
                           "StopDiscovery", NULL, G_VARIANT_TYPE("()"),
                           LINK_BLE_CALL_TIMEOUT_MS);
        if (reply != NULL) g_variant_unref(reply);
    }
    g_object_unref(bus);

    qsort(devices, device_count, sizeof(devices[0]), ble_discovered_compare);
    for (index = 0U; index < device_count && written_count < capacity; ++index) {
        if (!devices[index].likely_elm ||
            bluetooth_name_prefers_ble(devices[index].name)) continue;
        (void)snprintf(paths[written_count], 256U, "BT:%s %s",
                       devices[index].address, devices[index].name);
        written_count++;
    }
    return written_count;
}

static int classic_spp_channel(const char *address)
{
    bdaddr_t target;
    uuid_t service_uuid;
    uint32_t attribute_range = UINT32_C(0x0000ffff);
    sdp_session_t *session;
    sdp_list_t *search_list = NULL;
    sdp_list_t *attribute_list = NULL;
    sdp_list_t *records = NULL;
    sdp_list_t *item;
    int channel = -1;

    if (address == NULL || str2ba(address, &target) != 0) return -1;
    session = sdp_connect(BDADDR_ANY, &target, SDP_RETRY_IF_BUSY);
    if (session == NULL) return -1;
    sdp_uuid16_create(&service_uuid, SERIAL_PORT_SVCLASS_ID);
    search_list = sdp_list_append(NULL, &service_uuid);
    attribute_list = sdp_list_append(NULL, &attribute_range);
    if (search_list != NULL && attribute_list != NULL &&
        sdp_service_search_attr_req(session, search_list, SDP_ATTR_REQ_RANGE,
                                    attribute_list, &records) == 0) {
        for (item = records; item != NULL; item = item->next) {
            const sdp_record_t *record = item->data;
            const int port = record != NULL
                ? sdp_get_proto_port(record, RFCOMM_UUID) : -1;
            if (port > 0 && port <= 30) {
                channel = port;
                break;
            }
        }
    }
    for (item = records; item != NULL; item = item->next) {
        if (item->data != NULL) sdp_record_free(item->data);
    }
    sdp_list_free(records, NULL);
    sdp_list_free(attribute_list, NULL);
    sdp_list_free(search_list, NULL);
    sdp_close(session);
    return channel;
}

static LinkTransportStatus classic_connect(LinkLinuxSerialTransport *transport)
{
    char address[18];
    bdaddr_t target;
    struct sockaddr_rc remote;
    struct pollfd descriptor;
    int channel;
    int fd;
    int flags;
    int socket_error = 0;
    socklen_t socket_error_size = sizeof(socket_error);

    if (transport == NULL || !transport->bluetooth_classic)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (transport->connected) return LINK_TRANSPORT_OK;
    if (!classic_extract_address(transport->device, address) ||
        str2ba(address, &target) != 0) return LINK_TRANSPORT_INVALID_ARGUMENT;

    channel = classic_spp_channel(address);
    if (channel <= 0) channel = 1;
    fd = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
    if (fd < 0) return LINK_TRANSPORT_IO_ERROR;
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    (void)fcntl(fd, F_SETFD, FD_CLOEXEC);

    memset(&remote, 0, sizeof(remote));
    remote.rc_family = AF_BLUETOOTH;
    remote.rc_bdaddr = target;
    remote.rc_channel = (uint8_t)channel;
    if (connect(fd, (struct sockaddr *)&remote, sizeof(remote)) != 0) {
        if (errno != EINPROGRESS) {
            close(fd);
            return LINK_TRANSPORT_NOT_CONNECTED;
        }
        descriptor.fd = fd;
        descriptor.events = POLLOUT;
        descriptor.revents = 0;
        if (poll(&descriptor, 1, LINK_CLASSIC_CONNECT_TIMEOUT_MS) <= 0 ||
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &socket_error,
                       &socket_error_size) != 0 || socket_error != 0) {
            close(fd);
            return LINK_TRANSPORT_NOT_CONNECTED;
        }
    }
    transport->fd = fd;
    transport->connected = true;
    return LINK_TRANSPORT_OK;
}

static bool ble_refresh_le_presence'''
if old not in s:
    raise SystemExit("BLE discovery tail missing")
s = s.replace(old, new, 1)

s = s.replace(
    """    if (transport->bluetooth_le) return ble_connect(transport);
    if (transport->connected) return LINK_TRANSPORT_OK;
""",
    """    if (transport->bluetooth_le) return ble_connect(transport);
    if (transport->bluetooth_classic) return classic_connect(transport);
    if (transport->connected) return LINK_TRANSPORT_OK;
""",
    1,
)

old = """    char address[18];
    if (transport == NULL || device == NULL || device[0] == '\0') return false;
    if (transport->connected) link_linux_serial_disconnect(transport);
    if (transport->provider_context != NULL) ble_state_destroy(transport);
    transport->bluetooth_le = ble_extract_address(device, address);
    if (strncmp(device, "BLE:", 4U) == 0 && !transport->bluetooth_le) return false;
"""
new = """    char address[18];
    char classic_address[18];
    if (transport == NULL || device == NULL || device[0] == '\0') return false;
    if (transport->connected) link_linux_serial_disconnect(transport);
    if (transport->provider_context != NULL) ble_state_destroy(transport);
    transport->bluetooth_le = ble_extract_address(device, address);
    transport->bluetooth_classic = classic_extract_address(device, classic_address);
    if (strncmp(device, "BLE:", 4U) == 0 && !transport->bluetooth_le) return false;
    if (strncmp(device, "BT:", 3U) == 0 && !transport->bluetooth_classic) return false;
"""
if old not in s:
    raise SystemExit("configure block missing")
s = s.replace(old, new, 1)

s = s.replace(
    """    if (transport->bluetooth_le)
        return ble_probe_elm327(transport, identity, identity_capacity);
    (void)tcflush(transport->fd, TCIFLUSH);
""",
    """    if (transport->bluetooth_le)
        return ble_probe_elm327(transport, identity, identity_capacity);
    if (!transport->bluetooth_classic) (void)tcflush(transport->fd, TCIFLUSH);
""",
    1,
)

s = s.replace(
    """    if (count < capacity)
        count += ble_discover_devices(paths + count, capacity - count);
    return count;
""",
    """    if (count < capacity)
        count += classic_discover_devices(paths + count, capacity - count);
    if (count < capacity)
        count += ble_discover_devices(paths + count, capacity - count);
    return count;
""",
    1,
)
p.write_text(s)

Path("docs/LINUX-BLUETOOTH.md").write_text(
    """<!-- SPDX-License-Identifier: GPL-3.0-or-later -->
# Linux Bluetooth transport

LINK 0.14.3 provides native BlueZ Bluetooth Low Energy (GATT) and Bluetooth Classic Serial Port Profile (RFCOMM) transports alongside ordinary Linux tty devices.

The GTK shell discovers `/dev/ttyUSB*`, `/dev/ttyACM*`, existing `/dev/rfcomm*`, likely ELM/OBD BLE devices and likely ELM/OBD Bluetooth Classic devices. BLE selections are represented as `BLE:<address> <name>` and Classic selections as `BT:<address> <name>`. Unrelated phones, mice and generic BLE beacons are no longer emitted into the diagnostic-adapter chooser.

Selecting a BLE adapter performs an LE refresh, connects through `org.bluez.Device1`, waits for GATT service resolution, chooses a writable/notifiable characteristic pair, enables notifications and verifies the adapter with `ATI`. No Vgate-specific GATT UUID is required.

Selecting a Bluetooth Classic adapter performs native SDP discovery for the Serial Port Profile and opens an RFCOMM socket directly. If a device does not expose SDP before connection, LINK falls back to RFCOMM channel 1, which is common for ELM327-compatible adapters. No manual `rfcomm bind` or `/dev/rfcomm0` setup is required. Existing `/dev/rfcomm*` devices remain supported for compatibility.

Vgate dual-mode adapters can therefore expose both sides independently: a name such as `IOS-Vlink` is normally reached over BLE/GATT, while `ANDROID-Vlink` is normally reached over Bluetooth Classic/SPP. The product UI uses the selected transport rather than assuming that either advertising name is tied to the host operating system.

The ELM layer above both providers owns command framing, parser state, timeouts and diagnostic semantics. Mercedes- and Jaguar-specific diagnostic knowledge remains outside LINK.
"""
)
