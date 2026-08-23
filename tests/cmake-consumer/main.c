// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/isotp.h"
#include "link/uds_services.h"

#include <stdio.h>

int main(void)
{
    if (link_uds_standard_service_count() != LINK_UDS_STANDARD_SERVICE_COUNT) {
        fputs("unexpected UDS service catalogue size\n", stderr);
        return 1;
    }
    if (!link_isotp_can_data_length_is_valid(true,
                                              LINK_ISOTP_CAN_FD_MAX_DATA_LENGTH)) {
        fputs("installed LINK package lacks CAN-FD 64-byte ISO-TP support\n",
              stderr);
        return 1;
    }

    puts("LINK installed package consumer passed");
    return 0;
}
