// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/linux_serial.h"
#include "link-linux-openport2.h"

#include <string.h>

#if defined(__linux__)
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gio/gio.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#define LINK_BLUEZ_BUS "org.bluez"
#define LINK_BLUEZ_OBJECT_MANAGER "org.freedesktop.DBus.ObjectManager"
#define LINK_BLUEZ_PROPERTIES "org.freedesktop.DBus.Properties"
#define LINK_BLUEZ_ADAPTER "org.bluez.Adapter1"
#define LINK_BLUEZ_DEVICE "org.bluez.Device1"
#define LINK_BLUEZ_CHARACTERISTIC "org.bluez.GattCharacteristic1"
#define LINK_BLE_SCAN_MS 1000
#define LINK_CLASSIC_SCAN_MS 1200
#define LINK_CLASSIC_CONNECT_TIMEOUT_MS 8000
#define LINK_BLE_CONNECT_SCAN_MS 450
#define LINK_BLE_SERVICES_TIMEOUT_MS 8000
#define LINK_BLE_CALL_TIMEOUT_MS 5000
#define LINK_BLE_WRITE_CHUNK 20U
#define LINK_BLE_MAX_CHARACTERISTICS 64U
#define LINK_BLE_MAX_DISCOVERED 64U
#define LINK_BLE_PROBE_CAPACITY 512U

typedef struct LinkLinuxBleState {
    GDBusConnection *bus;
    char address[18];
    char device_path[256];
    char write_path[256];
    char notify_path[256];
    guint notify_subscription_id;
    bool notifications_started;
    bool write_without_response;
    bool connected;
    bool probing;
    uint8_t probe_buffer[LINK_BLE_PROBE_CAPACITY];
    size_t probe_used;
} LinkLinuxBleState;

typedef struct LinkLinuxBleCharacteristic {
    char path[256];
    char service[256];
    char uuid[64];
    bool writable;
    bool write_without_response;
    bool notifiable;
} LinkLinuxBleCharacteristic;

typedef struct LinkLinuxBleDiscovered {
    char address[18];
    char name[128];
    bool likely_elm;
    bool has_rssi;
    gint16 rssi;
} LinkLinuxBleDiscovered;

static speed_t serial_speed(unsigned int baud);
static LinkTransportStatus serial_connect(void *context);
static void serial_disconnect(void *context);
static bool serial_is_connected(void *context);
static LinkTransportStatus serial_write(void *context,
                                        const uint8_t *bytes,
                                        size_t size);
static void serial_set_receiver(void *context,
                                LinkTransportReceiveFn receiver,
                                void *receiver_context);
static LinkLinuxBleState *ble_state(LinkLinuxSerialTransport *transport);
static void ble_state_destroy(LinkLinuxSerialTransport *transport);
static GVariant *bluez_call(GDBusConnection *bus,
                            const char *object_path,
                            const char *interface_name,
                            const char *method_name,
                            GVariant *parameters,
                            const GVariantType *reply_type,
                            int timeout_ms);
static GVariant *bluez_managed_objects(GDBusConnection *bus);
static bool bluez_find_adapter(GDBusConnection *bus,
                               char *path,
                               size_t capacity);
static bool bluez_find_device(GDBusConnection *bus,
                              const char *address,
                              char *path,
                              size_t capacity);
static bool bluez_get_boolean(GDBusConnection *bus,
                              const char *path,
                              const char *interface_name,
                              const char *property_name,
                              bool *value);
static bool ble_extract_address(const char *device,
                                char address[18]);
static bool classic_extract_address(const char *device,
                                    char address[18]);
static bool ble_name_likely_elm(const char *name);
static bool bluetooth_name_prefers_classic(const char *name);
static bool bluetooth_name_prefers_ble(const char *name);
static int ble_discovered_compare(const void *left, const void *right);
static size_t ble_discover_devices(char paths[][256], size_t capacity);
static size_t classic_discover_devices(char paths[][256], size_t capacity);
static int classic_spp_channel(const char *address);
static LinkTransportStatus classic_connect(LinkLinuxSerialTransport *transport);
static bool ble_refresh_le_presence(GDBusConnection *bus,
                                    const char *address,
                                    char *device_path,
                                    size_t device_capacity,
                                    int scan_ms);
static bool ble_wait_services(LinkLinuxBleState *state);
static bool ble_find_characteristics(LinkLinuxBleState *state);
static void ble_properties_changed(GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data);
static bool ble_start_notifications(LinkLinuxSerialTransport *transport);
static void ble_stop_notifications(LinkLinuxSerialTransport *transport);
static LinkTransportStatus ble_connect(LinkLinuxSerialTransport *transport);
static void ble_disconnect(LinkLinuxSerialTransport *transport);
static LinkTransportStatus ble_write(LinkLinuxSerialTransport *transport,
                                     const uint8_t *bytes,
                                     size_t size);
static bool response_has_prompt(const uint8_t *bytes, size_t size);
static void copy_elm_identity(const uint8_t *bytes,
                              size_t size,
                              char *identity,
                              size_t identity_capacity);
static bool ble_probe_elm327(LinkLinuxSerialTransport *transport,
                             char *identity,
                             size_t identity_capacity);

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

static LinkLinuxBleState *ble_state(LinkLinuxSerialTransport *transport)
{
    return transport != NULL
        ? (LinkLinuxBleState *)transport->provider_context : NULL;
}

static void ble_state_destroy(LinkLinuxSerialTransport *transport)
{
    LinkLinuxBleState *state;
    if (transport == NULL) return;
    state = ble_state(transport);
    if (state == NULL) return;
    if (state->bus != NULL) g_object_unref(state->bus);
    g_free(state);
    transport->provider_context = NULL;
}

static GVariant *bluez_call(GDBusConnection *bus,
                            const char *object_path,
                            const char *interface_name,
                            const char *method_name,
                            GVariant *parameters,
                            const GVariantType *reply_type,
                            int timeout_ms)
{
    GError *error = NULL;
    GVariant *reply;
    if (bus == NULL || object_path == NULL || interface_name == NULL ||
        method_name == NULL) return NULL;
    reply = g_dbus_connection_call_sync(bus,
                                        LINK_BLUEZ_BUS,
                                        object_path,
                                        interface_name,
                                        method_name,
                                        parameters,
                                        reply_type,
                                        G_DBUS_CALL_FLAGS_NONE,
                                        timeout_ms,
                                        NULL,
                                        &error);
    if (error != NULL) g_error_free(error);
    return reply;
}

static GVariant *bluez_managed_objects(GDBusConnection *bus)
{
    GVariant *reply;
    GVariant *objects;
    reply = bluez_call(bus, "/", LINK_BLUEZ_OBJECT_MANAGER,
                       "GetManagedObjects", NULL,
                       G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
                       LINK_BLE_CALL_TIMEOUT_MS);
    if (reply == NULL) return NULL;
    objects = g_variant_get_child_value(reply, 0U);
    g_variant_unref(reply);
    return objects;
}

static bool bluez_find_adapter(GDBusConnection *bus,
                               char *path,
                               size_t capacity)
{
    GVariant *objects;
    GVariantIter iterator;
    const gchar *object_path;
    GVariant *interfaces;
    bool found = false;
    if (path == NULL || capacity == 0U) return false;
    path[0] = '\0';
    objects = bluez_managed_objects(bus);
    if (objects == NULL) return false;
    g_variant_iter_init(&iterator, objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &object_path, &interfaces)) {
        GVariant *properties = g_variant_lookup_value(
            interfaces, LINK_BLUEZ_ADAPTER, G_VARIANT_TYPE("a{sv}"));
        if (properties != NULL) {
            (void)snprintf(path, capacity, "%s", object_path);
            g_variant_unref(properties);
            g_variant_unref(interfaces);
            found = true;
            break;
        }
        g_variant_unref(interfaces);
    }
    g_variant_unref(objects);
    return found;
}

static bool bluez_find_device(GDBusConnection *bus,
                              const char *address,
                              char *path,
                              size_t capacity)
{
    GVariant *objects;
    GVariantIter iterator;
    const gchar *object_path;
    GVariant *interfaces;
    bool found = false;
    if (address == NULL || path == NULL || capacity == 0U) return false;
    path[0] = '\0';
    objects = bluez_managed_objects(bus);
    if (objects == NULL) return false;
    g_variant_iter_init(&iterator, objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &object_path, &interfaces)) {
        GVariant *properties = g_variant_lookup_value(
            interfaces, LINK_BLUEZ_DEVICE, G_VARIANT_TYPE("a{sv}"));
        if (properties != NULL) {
            const gchar *candidate = NULL;
            if (g_variant_lookup(properties, "Address", "&s", &candidate) &&
                candidate != NULL && g_ascii_strcasecmp(candidate, address) == 0) {
                (void)snprintf(path, capacity, "%s", object_path);
                found = true;
            }
            g_variant_unref(properties);
        }
        g_variant_unref(interfaces);
        if (found) break;
    }
    g_variant_unref(objects);
    return found;
}

static bool bluez_get_boolean(GDBusConnection *bus,
                              const char *path,
                              const char *interface_name,
                              const char *property_name,
                              bool *value)
{
    GVariant *reply;
    GVariant *boxed;
    GVariant *inner;
    bool result = false;
    if (value == NULL) return false;
    *value = false;
    reply = bluez_call(bus, path, LINK_BLUEZ_PROPERTIES, "Get",
                       g_variant_new("(ss)", interface_name, property_name),
                       G_VARIANT_TYPE("(v)"),
                       LINK_BLE_CALL_TIMEOUT_MS);
    if (reply == NULL) return false;
    boxed = g_variant_get_child_value(reply, 0U);
    inner = g_variant_get_variant(boxed);
    if (g_variant_is_of_type(inner, G_VARIANT_TYPE_BOOLEAN)) {
        *value = g_variant_get_boolean(inner) != FALSE;
        result = true;
    }
    g_variant_unref(inner);
    g_variant_unref(boxed);
    g_variant_unref(reply);
    return result;
}

static bool ble_extract_address(const char *device,
                                char address[18])
{
    size_t index;
    if (device == NULL || address == NULL || strncmp(device, "BLE:", 4U) != 0)
        return false;
    for (index = 0U; index < 17U; ++index) {
        const char c = device[4U + index];
        if (c == '\0') return false;
        if ((index + 1U) % 3U == 0U) {
            if (c != ':') return false;
        } else if (!g_ascii_isxdigit(c)) {
            return false;
        }
        address[index] = c;
    }
    address[17] = '\0';
    return device[21] == '\0' || device[21] == ' ';
}

static bool classic_extract_address(const char *device,
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

static bool ble_name_likely_elm(const char *name)
{
    gchar *lower;
    bool likely;
    if (name == NULL || name[0] == '\0') return false;
    lower = g_ascii_strdown(name, -1);
    if (lower == NULL) return false;
    likely = strstr(lower, "vgate") != NULL ||
             strstr(lower, "v-link") != NULL ||
             strstr(lower, "icar") != NULL ||
             strstr(lower, "obd") != NULL ||
             strstr(lower, "elm") != NULL ||
             strstr(lower, "car pro") != NULL;
    g_free(lower);
    return likely;
}

static bool bluetooth_name_contains(const char *name, const char *needle)
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

static int ble_discovered_compare(const void *left, const void *right)
{
    const LinkLinuxBleDiscovered *a = left;
    const LinkLinuxBleDiscovered *b = right;
    if (a->likely_elm != b->likely_elm) return a->likely_elm ? -1 : 1;
    if (a->has_rssi != b->has_rssi) return a->has_rssi ? -1 : 1;
    if (a->has_rssi && b->has_rssi && a->rssi != b->rssi)
        return a->rssi > b->rssi ? -1 : 1;
    return g_ascii_strcasecmp(a->name, b->name);
}

static size_t ble_discover_devices(char paths[][256], size_t capacity)
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
                              g_variant_new_string("le"));
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

    deadline = g_get_monotonic_time() + (gint64)LINK_BLE_SCAN_MS * 1000;
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
                                   ? name : "Bluetooth LE device");
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
    {
        size_t written_count = 0U;
        for (index = 0U; index < device_count && written_count < capacity; ++index) {
            if (!devices[index].likely_elm ||
                bluetooth_name_prefers_classic(devices[index].name)) continue;
            (void)snprintf(paths[written_count], 256U, "BLE:%.17s %.127s",
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
        (void)snprintf(paths[written_count], 256U, "BT:%.17s %.127s",
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
            sdp_list_t *protocols = NULL;
            int port = -1;
            if (record != NULL &&
                sdp_get_access_protos(record, &protocols) == 0 &&
                protocols != NULL) {
                port = sdp_get_proto_port(protocols, RFCOMM_UUID);
            }
            if (protocols != NULL) {
                sdp_list_t *group;
                for (group = protocols; group != NULL; group = group->next)
                    sdp_list_free((sdp_list_t *)group->data, NULL);
                sdp_list_free(protocols, NULL);
            }
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

static bool ble_refresh_le_presence(GDBusConnection *bus,
                                    const char *address,
                                    char *device_path,
                                    size_t device_capacity,
                                    int scan_ms)
{
    char adapter_path[256];
    GVariant *reply;
    bool discovery_started = false;
    gint64 deadline;
    bool found = false;
    if (bus == NULL || address == NULL || device_path == NULL ||
        device_capacity == 0U) return false;
    if (!bluez_find_adapter(bus, adapter_path, sizeof(adapter_path))) return false;
    {
        GVariantBuilder filter;
        g_variant_builder_init(&filter, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&filter, "{sv}", "Transport",
                              g_variant_new_string("le"));
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
    deadline = g_get_monotonic_time() + (gint64)scan_ms * 1000;
    do {
        while (g_main_context_pending(NULL))
            (void)g_main_context_iteration(NULL, FALSE);
        if (bluez_find_device(bus, address, device_path, device_capacity)) {
            found = true;
        }
        if (g_get_monotonic_time() >= deadline) break;
        g_usleep(25000U);
    } while (true);
    if (discovery_started) {
        reply = bluez_call(bus, adapter_path, LINK_BLUEZ_ADAPTER,
                           "StopDiscovery", NULL, G_VARIANT_TYPE("()"),
                           LINK_BLE_CALL_TIMEOUT_MS);
        if (reply != NULL) g_variant_unref(reply);
    }
    return found;
}

static bool ble_wait_services(LinkLinuxBleState *state)
{
    gint64 deadline;
    if (state == NULL || state->bus == NULL || state->device_path[0] == '\0')
        return false;
    deadline = g_get_monotonic_time() +
               (gint64)LINK_BLE_SERVICES_TIMEOUT_MS * 1000;
    while (g_get_monotonic_time() < deadline) {
        bool resolved = false;
        while (g_main_context_pending(NULL))
            (void)g_main_context_iteration(NULL, FALSE);
        if (bluez_get_boolean(state->bus, state->device_path,
                              LINK_BLUEZ_DEVICE, "ServicesResolved",
                              &resolved) && resolved) {
            return true;
        }
        g_usleep(50000U);
    }
    return false;
}

static bool ble_find_characteristics(LinkLinuxBleState *state)
{
    GVariant *objects;
    GVariantIter iterator;
    const gchar *object_path;
    GVariant *interfaces;
    LinkLinuxBleCharacteristic characteristics[LINK_BLE_MAX_CHARACTERISTICS];
    size_t count = 0U;
    size_t write_index = 0U;
    size_t notify_index = 0U;
    int best_score = -1;
    size_t i;
    size_t j;

    if (state == NULL || state->bus == NULL || state->device_path[0] == '\0')
        return false;
    objects = bluez_managed_objects(state->bus);
    if (objects == NULL) return false;
    memset(characteristics, 0, sizeof(characteristics));
    g_variant_iter_init(&iterator, objects);
    while (g_variant_iter_next(&iterator, "{&o@a{sa{sv}}}",
                               &object_path, &interfaces)) {
        GVariant *properties;
        if (!g_str_has_prefix(object_path, state->device_path)) {
            g_variant_unref(interfaces);
            continue;
        }
        properties = g_variant_lookup_value(
            interfaces, LINK_BLUEZ_CHARACTERISTIC, G_VARIANT_TYPE("a{sv}"));
        if (properties != NULL && count < LINK_BLE_MAX_CHARACTERISTICS) {
            LinkLinuxBleCharacteristic *characteristic = &characteristics[count];
            GVariant *flags;
            GVariant *service;
            GVariant *uuid;
            (void)snprintf(characteristic->path,
                           sizeof(characteristic->path), "%s", object_path);
            service = g_variant_lookup_value(properties, "Service",
                                             G_VARIANT_TYPE_OBJECT_PATH);
            if (service != NULL) {
                (void)snprintf(characteristic->service,
                               sizeof(characteristic->service), "%s",
                               g_variant_get_string(service, NULL));
                g_variant_unref(service);
            }
            uuid = g_variant_lookup_value(properties, "UUID",
                                          G_VARIANT_TYPE_STRING);
            if (uuid != NULL) {
                (void)snprintf(characteristic->uuid,
                               sizeof(characteristic->uuid), "%s",
                               g_variant_get_string(uuid, NULL));
                g_variant_unref(uuid);
            }
            flags = g_variant_lookup_value(properties, "Flags",
                                           G_VARIANT_TYPE("as"));
            if (flags != NULL) {
                GVariantIter flags_iterator;
                const gchar *flag;
                g_variant_iter_init(&flags_iterator, flags);
                while (g_variant_iter_next(&flags_iterator, "&s", &flag)) {
                    if (strcmp(flag, "write") == 0) characteristic->writable = true;
                    if (strcmp(flag, "write-without-response") == 0) {
                        characteristic->writable = true;
                        characteristic->write_without_response = true;
                    }
                    if (strcmp(flag, "notify") == 0 ||
                        strcmp(flag, "indicate") == 0) {
                        characteristic->notifiable = true;
                    }
                }
                g_variant_unref(flags);
            }
            count++;
            g_variant_unref(properties);
        }
        g_variant_unref(interfaces);
    }
    g_variant_unref(objects);

    for (i = 0U; i < count; ++i) {
        if (!characteristics[i].writable) continue;
        for (j = 0U; j < count; ++j) {
            int score = 0;
            if (!characteristics[j].notifiable) continue;
            if (strcmp(characteristics[i].service,
                       characteristics[j].service) == 0 &&
                characteristics[i].service[0] != '\0') score += 60;
            if (strcmp(characteristics[i].path,
                       characteristics[j].path) == 0) score += 80;
            if (characteristics[i].write_without_response) score += 10;
            if (strstr(characteristics[i].uuid, "fff2") != NULL ||
                strstr(characteristics[i].uuid, "ffe1") != NULL) score += 3;
            if (strstr(characteristics[j].uuid, "fff1") != NULL ||
                strstr(characteristics[j].uuid, "ffe1") != NULL) score += 3;
            if (score > best_score) {
                best_score = score;
                write_index = i;
                notify_index = j;
            }
        }
    }
    if (best_score < 0) return false;
    (void)snprintf(state->write_path, sizeof(state->write_path), "%s",
                   characteristics[write_index].path);
    (void)snprintf(state->notify_path, sizeof(state->notify_path), "%s",
                   characteristics[notify_index].path);
    state->write_without_response =
        characteristics[write_index].write_without_response;
    return true;
}

static void ble_properties_changed(GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data)
{
    LinkLinuxSerialTransport *transport = user_data;
    LinkLinuxBleState *state = ble_state(transport);
    const gchar *changed_interface;
    GVariant *changed;
    GVariant *invalidated;
    GVariant *value;
    gsize size = 0U;
    const guint8 *bytes;
    (void)connection;
    (void)sender_name;
    (void)object_path;
    (void)interface_name;
    (void)signal_name;
    if (state == NULL || parameters == NULL) return;
    g_variant_get(parameters, "(&s@a{sv}@as)",
                  &changed_interface, &changed, &invalidated);
    if (strcmp(changed_interface, LINK_BLUEZ_CHARACTERISTIC) != 0) {
        g_variant_unref(changed);
        g_variant_unref(invalidated);
        return;
    }
    value = g_variant_lookup_value(changed, "Value", G_VARIANT_TYPE("ay"));
    if (value != NULL) {
        bytes = g_variant_get_fixed_array(value, &size, sizeof(guint8));
        if (bytes != NULL && size != 0U) {
            if (state->probing) {
                size_t available = sizeof(state->probe_buffer) - state->probe_used;
                size_t copied = size < available ? size : available;
                if (copied != 0U) {
                    memcpy(state->probe_buffer + state->probe_used, bytes, copied);
                    state->probe_used += copied;
                }
            } else if (transport != NULL && transport->receiver != NULL) {
                transport->receiver(transport->receiver_context,
                                    (const uint8_t *)bytes, (size_t)size);
            }
        }
        g_variant_unref(value);
    }
    g_variant_unref(changed);
    g_variant_unref(invalidated);
}

static bool ble_start_notifications(LinkLinuxSerialTransport *transport)
{
    LinkLinuxBleState *state = ble_state(transport);
    GVariant *reply;
    if (state == NULL || state->bus == NULL || state->notify_path[0] == '\0')
        return false;
    state->notify_subscription_id = g_dbus_connection_signal_subscribe(
        state->bus,
        LINK_BLUEZ_BUS,
        LINK_BLUEZ_PROPERTIES,
        "PropertiesChanged",
        state->notify_path,
        LINK_BLUEZ_CHARACTERISTIC,
        G_DBUS_SIGNAL_FLAGS_NONE,
        ble_properties_changed,
        transport,
        NULL);
    reply = bluez_call(state->bus, state->notify_path,
                       LINK_BLUEZ_CHARACTERISTIC, "StartNotify",
                       NULL, G_VARIANT_TYPE("()"), LINK_BLE_CALL_TIMEOUT_MS);
    if (reply == NULL) {
        if (state->notify_subscription_id != 0U) {
            g_dbus_connection_signal_unsubscribe(state->bus,
                                                 state->notify_subscription_id);
            state->notify_subscription_id = 0U;
        }
        return false;
    }
    g_variant_unref(reply);
    state->notifications_started = true;
    return true;
}

static void ble_stop_notifications(LinkLinuxSerialTransport *transport)
{
    LinkLinuxBleState *state = ble_state(transport);
    GVariant *reply;
    if (state == NULL || state->bus == NULL) return;
    if (state->notify_subscription_id != 0U) {
        g_dbus_connection_signal_unsubscribe(state->bus,
                                             state->notify_subscription_id);
        state->notify_subscription_id = 0U;
    }
    if (state->notifications_started && state->notify_path[0] != '\0') {
        reply = bluez_call(state->bus, state->notify_path,
                           LINK_BLUEZ_CHARACTERISTIC, "StopNotify",
                           NULL, G_VARIANT_TYPE("()"), LINK_BLE_CALL_TIMEOUT_MS);
        if (reply != NULL) g_variant_unref(reply);
    }
    state->notifications_started = false;
}

static LinkTransportStatus ble_connect(LinkLinuxSerialTransport *transport)
{
    LinkLinuxBleState *state;
    GError *error = NULL;
    GVariant *reply;
    bool already_connected = false;
    if (transport == NULL || !transport->bluetooth_le)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (transport->connected) return LINK_TRANSPORT_OK;
    state = ble_state(transport);
    if (state == NULL) {
        state = g_new0(LinkLinuxBleState, 1U);
        transport->provider_context = state;
    }
    if (!ble_extract_address(transport->device, state->address))
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    state->bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if (error != NULL) g_error_free(error);
    if (state->bus == NULL) {
        ble_state_destroy(transport);
        return LINK_TRANSPORT_IO_ERROR;
    }
    if (!ble_refresh_le_presence(state->bus, state->address,
                                 state->device_path,
                                 sizeof(state->device_path),
                                 LINK_BLE_CONNECT_SCAN_MS)) {
        ble_state_destroy(transport);
        return LINK_TRANSPORT_NOT_CONNECTED;
    }
    (void)bluez_get_boolean(state->bus, state->device_path,
                            LINK_BLUEZ_DEVICE, "Connected",
                            &already_connected);
    if (!already_connected) {
        reply = bluez_call(state->bus, state->device_path,
                           LINK_BLUEZ_DEVICE, "Connect", NULL,
                           G_VARIANT_TYPE("()"), 10000);
        if (reply == NULL) {
            ble_state_destroy(transport);
            return LINK_TRANSPORT_NOT_CONNECTED;
        }
        g_variant_unref(reply);
    }
    if (!ble_wait_services(state) || !ble_find_characteristics(state) ||
        !ble_start_notifications(transport)) {
        ble_disconnect(transport);
        return LINK_TRANSPORT_UNSUPPORTED;
    }
    state->connected = true;
    transport->connected = true;
    return LINK_TRANSPORT_OK;
}

static void ble_disconnect(LinkLinuxSerialTransport *transport)
{
    LinkLinuxBleState *state = ble_state(transport);
    GVariant *reply;
    if (transport == NULL) return;
    if (state != NULL && state->bus != NULL) {
        ble_stop_notifications(transport);
        if (state->device_path[0] != '\0') {
            reply = bluez_call(state->bus, state->device_path,
                               LINK_BLUEZ_DEVICE, "Disconnect", NULL,
                               G_VARIANT_TYPE("()"), LINK_BLE_CALL_TIMEOUT_MS);
            if (reply != NULL) g_variant_unref(reply);
        }
        state->connected = false;
    }
    transport->connected = false;
    ble_state_destroy(transport);
}

static LinkTransportStatus ble_write(LinkLinuxSerialTransport *transport,
                                     const uint8_t *bytes,
                                     size_t size)
{
    LinkLinuxBleState *state = ble_state(transport);
    size_t offset = 0U;
    if (transport == NULL || bytes == NULL || size == 0U)
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (state == NULL || !state->connected || state->bus == NULL ||
        state->write_path[0] == '\0') return LINK_TRANSPORT_NOT_CONNECTED;
    while (offset < size) {
        const size_t remaining = size - offset;
        const size_t chunk = remaining < LINK_BLE_WRITE_CHUNK
            ? remaining : LINK_BLE_WRITE_CHUNK;
        GVariantBuilder options;
        GVariant *value;
        GVariant *reply;
        g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&options, "{sv}", "type",
                              g_variant_new_string(
                                  state->write_without_response
                                      ? "command" : "request"));
        value = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,
                                          bytes + offset, chunk,
                                          sizeof(uint8_t));
        reply = bluez_call(state->bus, state->write_path,
                           LINK_BLUEZ_CHARACTERISTIC, "WriteValue",
                           g_variant_new("(@aya{sv})", value, &options),
                           G_VARIANT_TYPE("()"), LINK_BLE_CALL_TIMEOUT_MS);
        if (reply == NULL) return LINK_TRANSPORT_IO_ERROR;
        g_variant_unref(reply);
        offset += chunk;
        if (offset < size && state->write_without_response) g_usleep(5000U);
    }
    return LINK_TRANSPORT_OK;
}

static bool response_has_prompt(const uint8_t *bytes, size_t size)
{
    size_t index;
    if (bytes == NULL) return false;
    for (index = 0U; index < size; ++index) {
        if (bytes[index] == (uint8_t)'>') return true;
    }
    return false;
}

static void copy_elm_identity(const uint8_t *bytes,
                              size_t size,
                              char *identity,
                              size_t identity_capacity)
{
    size_t source = 0U;
    if (identity == NULL || identity_capacity == 0U) return;
    identity[0] = '\0';
    while (source < size) {
        char line[160];
        size_t used = 0U;
        size_t start;
        size_t end;
        while (source < size &&
               (bytes[source] == '\r' || bytes[source] == '\n' ||
                bytes[source] == '>' || bytes[source] == ' ' ||
                bytes[source] == '\t')) source++;
        while (source < size && bytes[source] != '\r' &&
               bytes[source] != '\n' && bytes[source] != '>') {
            if (used + 1U < sizeof(line) && bytes[source] >= 32U &&
                bytes[source] < 127U) line[used++] = (char)bytes[source];
            source++;
        }
        line[used] = '\0';
        start = 0U;
        while (start < used && (line[start] == ' ' || line[start] == '\t')) start++;
        end = used;
        while (end > start &&
               (line[end - 1U] == ' ' || line[end - 1U] == '\t')) end--;
        line[end] = '\0';
        if (line[start] == '\0' || strcmp(line + start, "ATI") == 0 ||
            strcmp(line + start, "OK") == 0) continue;
        (void)snprintf(identity, identity_capacity, "%s", line + start);
        return;
    }
}

static bool ble_probe_elm327(LinkLinuxSerialTransport *transport,
                             char *identity,
                             size_t identity_capacity)
{
    static const uint8_t command[] = { 'A', 'T', 'I', '\r' };
    LinkLinuxBleState *state = ble_state(transport);
    gint64 deadline;
    bool complete = false;
    if (identity != NULL && identity_capacity != 0U) identity[0] = '\0';
    if (state == NULL || !state->connected) return false;
    state->probe_used = 0U;
    state->probing = true;
    if (ble_write(transport, command, sizeof(command)) != LINK_TRANSPORT_OK) {
        state->probing = false;
        return false;
    }
    deadline = g_get_monotonic_time() + 1800000;
    while (g_get_monotonic_time() < deadline) {
        while (g_main_context_pending(NULL))
            (void)g_main_context_iteration(NULL, FALSE);
        if (response_has_prompt(state->probe_buffer, state->probe_used)) {
            complete = true;
            break;
        }
        g_usleep(10000U);
    }
    state->probing = false;
    if (!complete) return false;
    copy_elm_identity(state->probe_buffer, state->probe_used,
                      identity, identity_capacity);
    return true;
}

static LinkTransportStatus serial_connect(void *context)
{
    LinkLinuxSerialTransport *transport = context;
    struct termios tty;
    speed_t speed;
    if (transport == NULL || transport->device[0] == '\0')
        return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (transport->openport2)
        return link_linux_openport2_connect(transport);
    if (transport->bluetooth_le) return ble_connect(transport);
    if (transport->bluetooth_classic) return classic_connect(transport);
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
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB);
#ifdef CRTSCTS
    tty.c_cflag &= ~CRTSCTS;
#endif
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
    if (transport->openport2)
        return link_linux_openport2_write(transport, bytes, size);
    if (transport->bluetooth_le) return ble_write(transport, bytes, size);
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
    char address[18];
    char classic_address[18];
    bool was_openport2;
    if (transport == NULL || device == NULL || device[0] == '\0') return false;
    was_openport2 = transport->openport2;
    if (transport->connected) link_linux_serial_disconnect(transport);
    if (transport->provider_context != NULL) {
        if (was_openport2)
            link_linux_openport2_destroy(transport);
        else
            ble_state_destroy(transport);
    }

    transport->bluetooth_le = ble_extract_address(device, address);
    transport->bluetooth_classic =
        classic_extract_address(device, classic_address);
    transport->openport2 = link_linux_openport2_is_selection(device);

    if (strncmp(device, "BLE:", 4U) == 0 && !transport->bluetooth_le)
        return false;
    if (strncmp(device, "BT:", 3U) == 0 && !transport->bluetooth_classic)
        return false;
    if (strncmp(device, "OP2:", 4U) == 0 && !transport->openport2)
        return false;

    (void)snprintf(transport->device, sizeof(transport->device), "%s", device);
    transport->baud_rate = baud_rate == 0U ? 38400U : baud_rate;
    transport->fd = -1;
    return true;
}

void link_linux_serial_disconnect(LinkLinuxSerialTransport *transport)
{
    if (transport == NULL) return;
    if (transport->openport2) {
        link_linux_openport2_disconnect(transport);
        return;
    }
    if (transport->bluetooth_le) {
        ble_disconnect(transport);
        return;
    }
    if (transport->fd >= 0) close(transport->fd);
    transport->fd = -1;
    transport->connected = false;
}

bool link_linux_serial_is_connected(const LinkLinuxSerialTransport *transport)
{
    if (transport == NULL || !transport->connected) return false;
    if (transport->openport2)
        return link_linux_openport2_is_connected(transport);
    if (transport->bluetooth_le) {
        const LinkLinuxBleState *state =
            (const LinkLinuxBleState *)transport->provider_context;
        return state != NULL && state->connected && state->bus != NULL;
    }
    return transport->fd >= 0;
}

bool link_linux_serial_probe_elm327(LinkLinuxSerialTransport *transport,
                                    char *identity,
                                    size_t identity_capacity)
{
    static const uint8_t command[] = { 'A', 'T', 'I', '\r' };
    uint8_t response[512];
    size_t used = 0U;
    int elapsed = 0;
    if (identity != NULL && identity_capacity != 0U) identity[0] = '\0';
    if (!link_linux_serial_is_connected(transport)) return false;
    if (transport->openport2)
        return link_linux_openport2_probe(
            transport, identity, identity_capacity);
    if (transport->bluetooth_le)
        return ble_probe_elm327(transport, identity, identity_capacity);
    if (!transport->bluetooth_classic) (void)tcflush(transport->fd, TCIFLUSH);
    if (serial_write(transport, command, sizeof(command)) != LINK_TRANSPORT_OK) return false;
    while (elapsed < 1800 && used < sizeof(response)) {
        struct pollfd descriptor = { transport->fd, POLLIN, 0 };
        int result = poll(&descriptor, 1, 100);
        elapsed += 100;
        if (result < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (result == 0) continue;
        if ((descriptor.revents & POLLIN) != 0) {
            ssize_t count = read(transport->fd, response + used,
                                 sizeof(response) - used);
            if (count > 0) {
                used += (size_t)count;
                if (response_has_prompt(response, used)) break;
            }
        }
    }
    if (used == 0U || !response_has_prompt(response, used)) return false;
    copy_elm_identity(response, used, identity, identity_capacity);
    return true;
}

void link_linux_serial_pump(LinkLinuxSerialTransport *transport)
{
    uint8_t buffer[1024];
    if (!link_linux_serial_is_connected(transport) || transport->receiver == NULL) return;
    if (transport->openport2) {
        link_linux_openport2_pump(transport);
        return;
    }
    if (transport->bluetooth_le) return;
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
    if (directory != NULL) {
        while ((entry = readdir(directory)) != NULL && count < capacity) {
            bool match = false;
            for (prefix_index = 0U;
                 prefix_index < sizeof(prefixes) / sizeof(prefixes[0]);
                 ++prefix_index) {
                size_t length = strlen(prefixes[prefix_index]);
                if (strncmp(entry->d_name, prefixes[prefix_index], length) == 0) {
                    match = true;
                    break;
                }
            }
            if (match) {
                (void)snprintf(paths[count], 256U, "/dev/%.249s", entry->d_name);
                count++;
            }
        }
        closedir(directory);
    }
    if (count < capacity)
        count += link_linux_openport2_discover(
            paths + count, capacity - count);
    if (count < capacity)
        count += classic_discover_devices(paths + count, capacity - count);
    if (count < capacity)
        count += ble_discover_devices(paths + count, capacity - count);
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
