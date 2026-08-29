// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_MERCEDES_ME_ADAPTER_H
#define LINK_MERCEDES_ME_ADAPTER_H
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
#define LINK_MERCEDES_ME_SPP_UUID "00001101-0000-1000-8000-00805F9B34FB"
#define LINK_MERCEDES_ME_NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_NUS_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_NUS_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define LINK_MERCEDES_ME_CCCD_UUID "00002902-0000-1000-8000-00805F9B34FB"
#define LINK_MERCEDES_ME_TOSHIBA_SERVICE_UUID "e079c6a0-aa8b-11e3-a903-0002a5d5c51b"
#define LINK_MERCEDES_ME_TOSHIBA_CHARACTERISTIC_UUID "b38312c0-aa89-11e3-9cef-0002a5d5c51b"
#define LINK_MERCEDES_ME_REFERENCE_CLASSIC_CONNECT_TIMEOUT_MS 44000U
#define LINK_MERCEDES_ME_REFERENCE_MIN_CONNECTION_DURATION_MS 6000U
#define LINK_MERCEDES_ME_REFERENCE_BLE_MTU 512U
typedef enum LinkMercedesMeAdapterFamily {
    LINK_MERCEDES_ME_ADAPTER_UNKNOWN = 0,
    LINK_MERCEDES_ME_ADAPTER_BLE,
    LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION,
    LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION,
    LINK_MERCEDES_ME_ADAPTER_OTHER_APPS
} LinkMercedesMeAdapterFamily;
LinkMercedesMeAdapterFamily link_mercedes_me_adapter_family_from_name(const char *name);
const char *link_mercedes_me_adapter_family_name(LinkMercedesMeAdapterFamily family);
bool link_mercedes_me_adapter_prefers_ble(LinkMercedesMeAdapterFamily family);
bool link_mercedes_me_adapter_prefers_classic_spp(LinkMercedesMeAdapterFamily family);
#ifdef __cplusplus
}
#endif
#endif
