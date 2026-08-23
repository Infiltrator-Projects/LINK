// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dtc_knowledge.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define EXPECTED_OBDEX_DTC_COUNT 9533U
#define EXPECTED_OBDEX_SNAPSHOT "bc58b0eb7273226a1aabae98e956b70b8362bda1"

static int failures;

static void check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static void check_known(const char *code, LinkDtcSystem system, const char *message)
{
    LinkDtcKnowledge knowledge;
    check(link_dtc_resolve(code, &knowledge), message);
    check(knowledge.definition_known, message);
    check(knowledge.system == system, message);
    check(knowledge.origin == LINK_DTC_ORIGIN_STANDARD_GENERIC, message);
    check(knowledge.source == LINK_DTC_SOURCE_STANDARD_GENERIC, message);
    check(knowledge.title[0] != '\0', message);
    check(knowledge.category[0] != '\0', message);
}

int main(void)
{
    LinkDtcKnowledge knowledge;
    char status[LINK_DTC_STATUS_TEXT_LENGTH];

    check(link_dtc_catalogue_definition_count() == EXPECTED_OBDEX_DTC_COUNT,
          "complete OBDex generic DTC count");
    check(strcmp(link_dtc_catalogue_snapshot(), EXPECTED_OBDEX_SNAPSHOT) == 0,
          "pinned OBDex snapshot");

    /* One definition from every generic family owned by LINK. */
    check_known("P0420", LINK_DTC_SYSTEM_POWERTRAIN, "P0 generic definition");
    check_known("P2002", LINK_DTC_SYSTEM_POWERTRAIN, "P2 generic definition");
    check_known("P3400", LINK_DTC_SYSTEM_POWERTRAIN, "P3 standardized definition");
    check_known("B0001", LINK_DTC_SYSTEM_BODY, "B0 generic definition");
    check_known("C0031", LINK_DTC_SYSTEM_CHASSIS, "C0 generic definition");
    check_known("U0100", LINK_DTC_SYSTEM_NETWORK, "U0 generic definition");
    check_known("U3000", LINK_DTC_SYSTEM_NETWORK, "U3 generic definition");

    check(link_dtc_resolve("p0304", &knowledge), "lowercase normalization");
    check(knowledge.definition_known && strcmp(knowledge.code, "P0304") == 0,
          "P0304 normalized and resolved");
    check(knowledge.title[0] != '\0', "P0304 title present");

    check(link_dtc_resolve("P1450", &knowledge), "valid P1 manufacturer-specific code");
    check(!knowledge.definition_known, "P1 manufacturer code remains unmapped");
    check(knowledge.origin == LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC,
          "P1 manufacturer classification");
    check(knowledge.title[0] == '\0', "manufacturer title remains empty");

    check(link_dtc_resolve("P3000", &knowledge), "valid manufacturer portion of P3");
    check(!knowledge.definition_known, "P3000 not taken from generic catalogue");
    check(knowledge.origin == LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC,
          "P3000 manufacturer classification");

    check(link_dtc_resolve("P3400", &knowledge), "valid standardized portion of P3");
    check(knowledge.definition_known && knowledge.origin == LINK_DTC_ORIGIN_STANDARD_GENERIC,
          "P3400 standard generic classification");

    check(!link_dtc_resolve("P04", &knowledge), "reject short code");
    check(!link_dtc_resolve("X0401", &knowledge), "reject invalid system");
    check(!link_dtc_resolve("PZ401", &knowledge), "reject invalid second digit");

    check(strcmp(link_dtc_source_name(LINK_DTC_SOURCE_STANDARD_GENERIC),
                 "OBDex CC0 generic definition") == 0,
          "generic provenance label");

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
