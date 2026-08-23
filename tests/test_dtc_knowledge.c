// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dtc_knowledge.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

int main(void)
{
    LinkDtcKnowledge knowledge;
    char status[LINK_DTC_STATUS_TEXT_LENGTH];

    check(link_dtc_resolve("P0401", &knowledge), "resolve P0401");
    check(knowledge.definition_known, "P0401 definition known");
    check(strcmp(knowledge.title, "Exhaust Gas Recirculation Flow Insufficient Detected") == 0,
          "P0401 title");
    check(strcmp(knowledge.category, "EGR/emissions") == 0, "P0401 category");
    check(knowledge.system == LINK_DTC_SYSTEM_POWERTRAIN, "P0401 system");
    check(knowledge.origin == LINK_DTC_ORIGIN_STANDARD_GENERIC, "P0401 origin");

    check(link_dtc_resolve("p0304", &knowledge), "lowercase normalization");
    check(knowledge.definition_known && strcmp(knowledge.code, "P0304") == 0,
          "P0304 normalized");
    check(strcmp(knowledge.title, "Cylinder 4 Misfire Detected") == 0,
          "generated cylinder misfire");

    check(link_dtc_resolve("P0264", &knowledge), "resolve injector pattern");
    check(strcmp(knowledge.title, "Cylinder 2 Injector Circuit Low") == 0,
          "injector pattern mapping");

    check(link_dtc_resolve("P1450", &knowledge), "valid manufacturer-specific code");
    check(!knowledge.definition_known, "manufacturer code remains unmapped");
    check(knowledge.origin == LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC,
          "manufacturer code classification");
    check(knowledge.title[0] == '\0', "unknown title remains empty");

    check(link_dtc_resolve("U0100", &knowledge), "resolve network code");
    check(knowledge.definition_known && knowledge.system == LINK_DTC_SYSTEM_NETWORK,
          "network code metadata");

    check(!link_dtc_resolve("P04", &knowledge), "reject short code");
    check(!link_dtc_resolve("X0401", &knowledge), "reject invalid system");

    check(link_dtc_format_uds_status(0x0dU, status, sizeof(status)), "format UDS status");
    check(strstr(status, "Test failed") != NULL &&
          strstr(status, "Pending") != NULL &&
          strstr(status, "Confirmed") != NULL,
          "UDS status meanings");

    if (failures != 0) {
        fprintf(stderr, "%d DTC knowledge test(s) failed\n", failures);
        return 1;
    }
    return 0;
}
