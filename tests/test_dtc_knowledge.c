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

static void check_origin(
    const char *code,
    LinkDtcOrigin origin,
    bool definition_known,
    const char *message)
{
    LinkDtcKnowledge knowledge;
    check(link_dtc_resolve(code, &knowledge), message);
    check(knowledge.origin == origin, message);
    check(knowledge.definition_known == definition_known, message);
    if (!definition_known) {
        check(knowledge.source == LINK_DTC_SOURCE_UNKNOWN, message);
        check(knowledge.title[0] == '\0', message);
        check(knowledge.category[0] == '\0', message);
    }
}

static void test_complete_catalogue(void)
{
    size_t counts_p0 = 0U, counts_p2 = 0U, counts_p3 = 0U;
    size_t counts_b0 = 0U, counts_c0 = 0U, counts_u0 = 0U, counts_u3 = 0U;
    size_t other = 0U;
    char previous[LINK_DTC_CODE_LENGTH] = "";
    size_t index;

    for (index = 0U; index < link_dtc_catalogue_definition_count(); ++index) {
        LinkDtcKnowledge entry;
        LinkDtcKnowledge resolved;
        check(link_dtc_catalogue_definition_at(index, &entry),
              "enumerate every compiled generic DTC");
        if (!link_dtc_catalogue_definition_at(index, &entry)) continue;

        check(entry.definition_known, "enumerated definition is known");
        check(entry.origin == LINK_DTC_ORIGIN_STANDARD_GENERIC,
              "enumerated definition is standard generic");
        check(entry.source == LINK_DTC_SOURCE_STANDARD_GENERIC,
              "enumerated definition retains CC0 source");
        check(entry.title[0] != '\0', "every generic DTC has a title");
        check(entry.category[0] != '\0', "every generic DTC has a category");
        if (index != 0U)
            check(strcmp(previous, entry.code) < 0,
                  "compiled DTC catalogue is strictly sorted and unique");
        (void)snprintf(previous, sizeof(previous), "%s", entry.code);

        check(link_dtc_resolve(entry.code, &resolved),
              "every enumerated definition resolves");
        check(resolved.definition_known &&
                  resolved.origin == LINK_DTC_ORIGIN_STANDARD_GENERIC &&
                  resolved.source == LINK_DTC_SOURCE_STANDARD_GENERIC &&
                  strcmp(resolved.title, entry.title) == 0 &&
                  strcmp(resolved.category, entry.category) == 0,
              "catalogue enumeration and binary-search resolver agree");

        if (entry.code[0] == 'P' && entry.code[1] == '0') ++counts_p0;
        else if (entry.code[0] == 'P' && entry.code[1] == '2') ++counts_p2;
        else if (entry.code[0] == 'P' && entry.code[1] == '3') ++counts_p3;
        else if (entry.code[0] == 'B' && entry.code[1] == '0') ++counts_b0;
        else if (entry.code[0] == 'C' && entry.code[1] == '0') ++counts_c0;
        else if (entry.code[0] == 'U' && entry.code[1] == '0') ++counts_u0;
        else if (entry.code[0] == 'U' && entry.code[1] == '3') ++counts_u3;
        else ++other;
    }

    check(counts_p0 == 3705U, "complete P0 generic family count");
    check(counts_p2 == 3495U, "complete P2 generic family count");
    check(counts_p3 == 155U, "complete standardized P3 family count");
    check(counts_b0 == 323U, "complete B0 generic family count");
    check(counts_c0 == 626U, "complete C0 generic family count");
    check(counts_u0 == 1055U, "complete U0 generic family count");
    check(counts_u3 == 174U, "complete U3 generic family count");
    check(other == 0U, "generic catalogue contains no manufacturer/reserved family");
    {
        LinkDtcKnowledge unused;
        check(!link_dtc_catalogue_definition_at(
                  link_dtc_catalogue_definition_count(), &unused),
              "catalogue enumeration rejects past-end index");
        check(!link_dtc_catalogue_definition_at(0U, NULL),
              "catalogue enumeration rejects NULL output");
    }
}

int main(void)
{
    LinkDtcKnowledge knowledge;
    char status[LINK_DTC_STATUS_TEXT_LENGTH];

    check(link_dtc_catalogue_definition_count() == EXPECTED_OBDEX_DTC_COUNT,
          "complete OBDex generic DTC count");
    check(strcmp(link_dtc_catalogue_snapshot(), EXPECTED_OBDEX_SNAPSHOT) == 0,
          "pinned OBDex snapshot");
    check(strcmp(link_dtc_range_model_revision(), "J2012_202509") == 0,
          "range model audited against current SAE J2012 revision");
    check(strcmp(link_dtc_catalogue_audit_revision(), "J2012DA_202607") == 0,
          "catalogue audit records current J2012 Digital Annex revision");
    test_complete_catalogue();

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

    /*
     * Exhaustively qualify the current J2012/ISO 15031-6 ownership boundaries.
     * "Standard-controlled" means LINK must not hand the number to an OEM, but
     * absence from the open catalogue is not falsely presented as an assigned
     * generic definition.
     */
    check_origin("B0000", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "B0 controlled unassigned boundary");
    check_origin("B1000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "B1 manufacturer boundary");
    check_origin("B2000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "B2 manufacturer boundary");
    check_origin("B3000", LINK_DTC_ORIGIN_DOCUMENT_RESERVED, false,
                 "B3 document-reserved boundary");

    check_origin("C0000", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "C0 controlled unassigned boundary");
    check_origin("C1000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "C1 manufacturer boundary");
    check_origin("C2000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "C2 manufacturer boundary");
    check_origin("C3000", LINK_DTC_ORIGIN_DOCUMENT_RESERVED, false,
                 "C3 document-reserved boundary");

    check_origin("P0000", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "P0 controlled unassigned boundary");
    check_origin("P1000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "P1 manufacturer boundary");
    check_known("P2000", LINK_DTC_SYSTEM_POWERTRAIN,
                "P2 standard-controlled assigned boundary");
    check_origin("P3000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "P3000 manufacturer portion");
    check_origin("P33FF", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "P33FF manufacturer upper boundary");
    check_known("P3400", LINK_DTC_SYSTEM_POWERTRAIN,
                "P3400 standardized P3 lower boundary");
    check_origin("P3FFF", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "P3FFF standard-controlled unassigned boundary");

    check_origin("U0000", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "U0 controlled unassigned boundary");
    check_origin("U1000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "U1 manufacturer boundary");
    check_origin("U2000", LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC, false,
                 "U2 manufacturer boundary");
    check_known("U3000", LINK_DTC_SYSTEM_NETWORK,
                "U3 standard-controlled assigned boundary");
    check_origin("U3FFF", LINK_DTC_ORIGIN_STANDARD_CONTROLLED, false,
                 "U3 standard-controlled unassigned boundary");

    check(strcmp(link_dtc_origin_name(LINK_DTC_ORIGIN_STANDARD_CONTROLLED),
                 "Standard-controlled, definition unavailable") == 0,
          "controlled-unmapped origin label");
    check(strcmp(link_dtc_origin_name(LINK_DTC_ORIGIN_DOCUMENT_RESERVED),
                 "Reserved by standard") == 0,
          "document-reserved origin label");

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
