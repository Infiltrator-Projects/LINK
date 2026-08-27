// SPDX-License-Identifier: GPL-3.0-or-later
#include "../platform/linux/link-linux-openport2.h"
#include <stdio.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "check failed: %s at %s:%d\n", #x, __FILE__, __LINE__); return 1; } } while (0)
int main(void)
{
    CHECK(link_linux_openport2_rx_status_is_complete_vehicle(0x00UL));
    CHECK(link_linux_openport2_rx_status_is_complete_vehicle(0x100UL));
    CHECK(!link_linux_openport2_rx_status_is_complete_vehicle(LINK_LINUX_OPENPORT2_RX_TX_LOOPBACK));
    CHECK(!link_linux_openport2_rx_status_is_complete_vehicle(LINK_LINUX_OPENPORT2_RX_START_OF_MESSAGE));
    CHECK(!link_linux_openport2_rx_status_is_complete_vehicle(LINK_LINUX_OPENPORT2_RX_TX_DONE));
    CHECK(!link_linux_openport2_rx_status_is_complete_vehicle(0x03UL));
    CHECK(!link_linux_openport2_rx_status_is_complete_vehicle(0x0aUL));
    puts("OpenPort RxStatus policy tests passed");
    return 0;
}
