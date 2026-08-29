// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_adapter.h"
#include <stdio.h>
#include <string.h>
#define CHECK(e) do { if (!(e)) { fprintf(stderr, "check failed: %s at %s:%d\n", #e, __FILE__, __LINE__); return 1; } } while (0)
int main(void)
{
    CHECK(link_mercedes_me_adapter_family_from_name("MB-123456") == LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-812345") == LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-912345") == LINK_MERCEDES_ME_ADAPTER_BLE);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-212345") == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-312345") == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-412345") == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-612345") == LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-512345") == LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-712345") == LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION);
    CHECK(link_mercedes_me_adapter_family_from_name("VAN-1234") == LINK_MERCEDES_ME_ADAPTER_OTHER_APPS);
    CHECK(link_mercedes_me_adapter_family_from_name("MB-012345") == LINK_MERCEDES_ME_ADAPTER_UNKNOWN);
    CHECK(link_mercedes_me_adapter_prefers_ble(LINK_MERCEDES_ME_ADAPTER_BLE));
    CHECK(link_mercedes_me_adapter_prefers_classic_spp(LINK_MERCEDES_ME_ADAPTER_FIRST_GENERATION));
    CHECK(link_mercedes_me_adapter_prefers_classic_spp(LINK_MERCEDES_ME_ADAPTER_SECOND_GENERATION));
    CHECK(strcmp(LINK_MERCEDES_ME_NUS_RX_UUID, "6E400002-B5A3-F393-E0A9-E50E24DCCA9E") == 0);
    CHECK(strcmp(LINK_MERCEDES_ME_NUS_TX_UUID, "6E400003-B5A3-F393-E0A9-E50E24DCCA9E") == 0);
    CHECK(LINK_MERCEDES_ME_REFERENCE_CLASSIC_CONNECT_TIMEOUT_MS == 44000U);
    CHECK(LINK_MERCEDES_ME_REFERENCE_MIN_CONNECTION_DURATION_MS == 6000U);
    CHECK(LINK_MERCEDES_ME_REFERENCE_BLE_MTU == 512U);
    return 0;
}
