// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/elm327.h"
#include "link/obd2.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void check(bool condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static size_t pid_count(const LinkObd2PidSet *set)
{
    size_t count = 0U;
    unsigned int pid;
    if (set == NULL) return 0U;
    for (pid = 1U; pid <= 0xffU; ++pid) {
        if (link_obd2_pid_set_contains(set, (uint8_t)pid)) count++;
    }
    return count;
}

static LinkElm327Response parse_response(const char *command, const char *wire)
{
    LinkElm327Parser parser;
    LinkElm327Response response;
    size_t consumed = 0U;
    memset(&response, 0, sizeof(response));
    check(link_elm327_parser_begin(&parser, command) == LINK_ELM327_RESULT_OK,
          "parser begin");
    check(link_elm327_parser_feed(&parser, (const uint8_t *)wire,
                                  strlen(wire), &consumed) == LINK_ELM327_RESULT_OK,
          "parser feed");
    check(link_elm327_parser_finish(&parser, &response) == LINK_ELM327_RESULT_OK,
          "parser finish");
    return response;
}

static void test_catalogue(void)
{
    const LinkObd2PidDefinition *definition;
    LinkObd2DecodedPid decoded;
    static const uint8_t odometer_payload[] = {0x00U, 0x01U, 0x86U, 0xa0U};
    static const uint8_t o2_payload[] = {0x40U, 0x00U, 0x20U, 0x00U};
    static const uint8_t vin_payload[] = {
        'S','A','J','A','D','5','6','L','6','4','W','D','7','8','4','3','5'
    };
    static const uint8_t fuel_status[] = {0x02U, 0x00U};
    static const uint8_t throttle_g[] = {0x80U};
    static const uint8_t reflash_distance[] = {0x01U, 0xf4U};
    static const uint8_t egr_payload[] = {
        0x3fU, 0x80U, 0x40U, 0x80U, 0xffU, 0x00U, 0x40U
    };
    static const uint8_t intake_payload[] = {
        0x0fU, 0x00U, 0x80U, 0xffU, 0x40U
    };
    static const uint8_t rail_payload[] = {
        0x3fU, 0x00U, 0x64U, 0x01U, 0x00U, 0x50U,
        0x00U, 0x32U, 0x00U, 0x10U, 0x28U
    };
    static const uint8_t injection_payload[] = {
        0x0fU, 0x00U, 0x64U, 0x00U, 0xc8U,
        0x01U, 0x2cU, 0x01U, 0x90U
    };
    static const uint8_t inlet_pressure_payload[] = {
        0x0cU, 0x0aU, 0x14U
    };
    static const uint8_t boost_payload[] = {
        0x1bU, 0x04U, 0x00U, 0x08U, 0x00U,
        0x10U, 0x00U, 0x20U, 0x00U, 0x00U
    };
    static const uint8_t vgt_payload[] = {
        0x3fU, 0x40U, 0x80U, 0xc0U, 0xffU, 0x09U
    };
    static const uint8_t exhaust_pressure_payload[] = {
        0x03U, 0x00U, 0x64U, 0x00U, 0xc8U
    };
    static const uint8_t turbo_rpm_payload[] = {
        0x03U, 0x00U, 0x64U, 0x00U, 0xc8U
    };
    static const uint8_t turbo_temp_payload[] = {
        0x0fU, 0x28U, 0x50U, 0x03U, 0x20U, 0x04U, 0xb0U
    };
    static const uint8_t charge_temp_payload[] = {
        0x0fU, 0x28U, 0x32U, 0x3cU, 0x46U
    };
    static const uint8_t dpf_pressure_payload[] = {
        0x07U, 0xffU, 0x9cU, 0x00U, 0x64U, 0x00U, 0xc8U
    };
    static const uint8_t dpf_temp_payload[] = {
        0x0fU, 0x03U, 0x20U, 0x04U, 0xb0U,
        0x06U, 0x40U, 0x07U, 0xd0U
    };
    static const uint8_t runtime_payload[] = {
        0x07U,
        0x00U, 0x00U, 0x00U, 0x64U,
        0x00U, 0x00U, 0x00U, 0x32U,
        0x00U, 0x00U, 0x00U, 0x19U
    };
    static const uint8_t nox_payload[] = {
        0x0fU, 0x00U, 0x64U, 0x00U, 0xc8U,
        0x01U, 0x2cU, 0x01U, 0x90U
    };
    static const uint8_t nox_reagent_payload[] = {
        0x0fU, 0x00U, 0xc8U, 0x01U, 0x90U, 0x80U,
        0x00U, 0x00U, 0x00U, 0x64U
    };
    static const uint8_t pm_payload[] = {
        0x03U, 0x00U, 0x50U, 0x00U, 0xa0U
    };
    static const uint8_t map_payload[] = {
        0x03U, 0x00U, 0x20U, 0x00U, 0x40U
    };
    static const uint8_t aftertreatment_payload[] = {
        0x7fU, 0x0fU, 0x80U, 0x00U, 0x0aU, 0x00U, 0x14U
    };
    static const uint8_t wide_o2_payload[] = {
        0xffU,
        0x00U,0x64U, 0x00U,0xc8U, 0x01U,0x2cU, 0x01U,0x90U,
        0x10U,0x00U, 0x20U,0x00U, 0x30U,0x00U, 0x40U,0x00U
    };
    static const uint8_t pm_output_payload[] = {
        0x0fU, 0x03U, 0x00U, 0x64U, 0x01U, 0x00U, 0xc8U
    };
    static const uint8_t egt_wide_payload[] = {
        0x0fU, 0x03U,0x20U, 0x04U,0xb0U,
        0x06U,0x40U, 0x07U,0xd0U
    };
    static const uint8_t hev_payload[] = {
        0x60U, 0x00U, 0x19U, 0x00U, 0xf8U, 0x30U
    };
    static const uint8_t fuel_use_payload[] = {
        0xffU, 0x00U, 0x20U, 0x40U, 0x60U,
        0x80U, 0xa0U, 0xc0U, 0xffU
    };
    static const uint8_t certified_payload[] = {0xc0U, 0xffU, 0x80U};
    static const uint8_t battery_soh[] = {0x80U};
    static const uint8_t exhaust_flow[] = {0x00U, 0x64U};
    static const uint8_t raw_motor[] = {0x01U, 0x02U, 0x03U};

    check(link_obd2_service_definition_count() == 10U,
          "service catalogue contains modes 01 through 0A");
    check(link_obd2_parameter_identifier_namespace_count(0x01U) == 256U &&
          link_obd2_parameter_identifier_namespace_count(0x05U) == 256U &&
          link_obd2_parameter_identifier_namespace_count(0x06U) == 256U &&
          link_obd2_parameter_identifier_namespace_count(0x09U) == 256U,
          "parameterized standard read namespaces are fully addressable");
    check(strcmp(link_obd2_obdonuds_revision(), "J1979-2_202604") == 0,
          "OBDonUDS profile revision is current");
    check(link_obd2_service_definition(0x01U) != NULL &&
              link_obd2_service_definition(0x01U)->read_only,
          "Mode 01 catalogued read-only");
    check(link_obd2_service_definition(0x04U) != NULL &&
              !link_obd2_service_definition(0x04U)->read_only,
          "Mode 04 remains write/control");
    check(link_obd2_service_definition(0x08U) != NULL &&
              !link_obd2_service_definition(0x08U)->read_only,
          "Mode 08 remains write/control");

    check(link_obd2_pid_definition_count() == 235U,
          "shared standards catalogue contains 234 Mode 01/09 definitions");
    check(strcmp(link_obd2_pid_catalogue_snapshot(),
                 "bc58b0eb7273226a1aabae98e956b70b8362bda1+link-standard-supplement-v3+link-corrections-v2") == 0,
          "catalogue provenance includes base snapshot and supplement");

    definition = link_obd2_pid_definition(0x01U, 0x69U);
    check(definition != NULL && definition->bytes == 7U,
          "modern EGR standard PID is catalogued");
    definition = link_obd2_pid_definition(0x01U, 0x7aU);
    check(definition != NULL && definition->bytes == 7U &&
              definition->value_kind == LINK_OBD2_VALUE_MULTI_SCALAR,
          "DPF bank 1 pressure standard PID is decoded");
    definition = link_obd2_pid_definition(0x01U, 0x7cU);
    check(definition != NULL && definition->bytes == 9U &&
              definition->value_kind == LINK_OBD2_VALUE_MULTI_SCALAR,
          "DPF temperature pinned metadata is corrected and decoded");
    definition = link_obd2_pid_definition(0x01U, 0x83U);
    check(definition != NULL && definition->bytes == 9U &&
              definition->value_kind == LINK_OBD2_VALUE_MULTI_SCALAR,
          "NOx sensor pinned metadata is corrected and decoded");
    definition = link_obd2_pid_definition(0x01U, 0xc8U);
    check(definition != NULL && definition->bytes == 1U,
          "late standard warning-lamp PID is catalogued");
    definition = link_obd2_pid_definition(0x09U, 0x09U);
    check(definition != NULL && definition->bytes == 1U,
          "Mode 09 ECU-name message count is catalogued");

    definition = link_obd2_pid_definition(0x01U, 0x95U);
    check(definition != NULL && definition->value_kind == LINK_OBD2_VALUE_RAW,
          "SCR/NH3 assignment is catalogued without invented layout");
    definition = link_obd2_pid_definition(0x01U, 0xb2U);
    check(definition != NULL && definition->bytes == 1U &&
              definition->value_kind == LINK_OBD2_VALUE_SCALAR,
          "traction battery state-of-health PID is decoded");
    definition = link_obd2_pid_definition(0x01U, 0xd3U);
    check(definition != NULL && definition->bytes == 4U,
          "engine odometer late PID is decoded");
    definition = link_obd2_pid_definition(0x01U, 0xdaU);
    check(definition != NULL && definition->value_kind == LINK_OBD2_VALUE_RAW,
          "classic assigned live-data catalogue reaches PID DA");
    definition = link_obd2_pid_definition(0x01U, 0xe0U);
    check(definition != NULL && definition->bytes == 4U &&
              definition->value_kind == LINK_OBD2_VALUE_BITMAP,
          "final classic support page E0 is catalogued");
    definition = link_obd2_pid_definition(0x01U, 0x9eU);
    check(definition != NULL && strcmp(definition->unit, "kg/h") == 0,
          "current exhaust-flow unit corrects pinned-source metadata");

    {
        size_t left;
        for (left = 0U; left < link_obd2_pid_definition_count(); ++left) {
            const LinkObd2PidDefinition *left_definition =
                link_obd2_pid_definition_at(left);
            size_t right;
            check(left_definition != NULL, "catalogue enumeration is complete");
            if (left_definition == NULL) continue;
            for (right = left + 1U;
                 right < link_obd2_pid_definition_count(); ++right) {
                const LinkObd2PidDefinition *right_definition =
                    link_obd2_pid_definition_at(right);
                check(right_definition == NULL ||
                          left_definition->mode != right_definition->mode ||
                          left_definition->pid != right_definition->pid,
                      "catalogue contains no duplicate mode/PID assignments");
            }
        }
    }

    /*
     * Full-catalogue contract: every assigned definition in the shared SAE
     * table must be consumable by the generic decoder. A definition whose
     * public layout is not independently corroborated still succeeds and
     * preserves its complete raw payload rather than becoming "unsupported".
     */
    {
        uint8_t payload[64] = {0};
        size_t definition_index;
        for (definition_index = 0U;
             definition_index < link_obd2_pid_definition_count();
             ++definition_index) {
            const LinkObd2PidDefinition *catalogue_definition =
                link_obd2_pid_definition_at(definition_index);
            LinkObd2DecodedPid catalogue_decoded;
            check(catalogue_definition != NULL,
                  "every SAE catalogue slot resolves");
            if (catalogue_definition == NULL) continue;
            check(catalogue_definition->bytes <= sizeof(payload),
                  "SAE payload fits generic decoder raw buffer");
            if (catalogue_definition->bytes > sizeof(payload)) continue;
            check(link_obd2_decode_pid_payload(
                      catalogue_definition->mode,
                      catalogue_definition->pid,
                      payload,
                      catalogue_definition->bytes,
                      &catalogue_decoded) == LINK_OBD2_RESULT_OK,
                  "every assigned SAE PID is generically decodable/raw-preserving");
            check(catalogue_decoded.definition != NULL,
                  "every assigned SAE PID retains definition metadata");
            check(catalogue_decoded.raw_length == catalogue_definition->bytes,
                  "every assigned SAE PID preserves its complete payload");
        }
    }

    check(link_obd2_decode_pid_payload(
              0x01U, 0xa6U, odometer_payload,
              sizeof(odometer_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 10000.0,
          "decode catalogue-backed odometer");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x24U, o2_payload,
              sizeof(o2_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 0.5 &&
              decoded.signals[1].value == 1.0,
          "decode both structured oxygen-sensor values");
    check(link_obd2_decode_pid_payload(
              0x09U, 0x02U, vin_payload,
              sizeof(vin_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.text_available &&
              strcmp(decoded.text, "SAJAD56L64WD78435") == 0,
          "decode catalogue-backed Mode 09 VIN");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x03U, fuel_status,
              sizeof(fuel_status), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.raw_length == 2U && decoded.signal_count == 0U,
          "preserve encoded standard PID raw payload");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x8dU, throttle_g,
              sizeof(throttle_g), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value > 50.19 &&
              decoded.signals[0].value < 50.20,
          "decode supplemented standard throttle G");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xc7U, reflash_distance,
              sizeof(reflash_distance), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 500.0,
          "decode supplemented reflash distance");

    check(link_obd2_decode_pid_payload(
              0x01U, 0x69U, egr_payload,
              sizeof(egr_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 6U &&
              decoded.signals[2].value == 0.0 &&
              decoded.signals[3].value == 100.0 &&
              decoded.signals[5].value == -50.0,
          "decode all six corroborated EGR fields");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x6aU, intake_payload,
              sizeof(intake_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[2].value == 100.0,
          "decode diesel intake-air control fields");

    check(link_obd2_decode_pid_payload(
              0x01U, 0x6dU, rail_payload,
              sizeof(rail_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 6U &&
              decoded.signals[0].value == 1000.0 &&
              decoded.signals[1].value == 2560.0 &&
              decoded.signals[2].value == 40.0 &&
              decoded.signals[5].value == 0.0,
          "decode modern dual fuel-pressure control payload");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x70U, boost_payload,
              sizeof(boost_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 32.0 &&
              decoded.signals[3].value == 256.0,
          "decode boost values beyond legacy 255 kPa MAP range");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x6eU, injection_payload,
              sizeof(injection_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 1000.0 &&
              decoded.signals[3].value == 4000.0,
          "decode injection pressure control");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x6fU, inlet_pressure_payload,
              sizeof(inlet_pressure_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 80.0 &&
              decoded.signals[1].value == 160.0,
          "decode wide-range turbo compressor inlet pressure");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x71U, vgt_payload,
              sizeof(vgt_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 6U &&
              decoded.signals[2].value == 1.0 &&
              decoded.signals[5].value == 2.0,
          "decode variable-geometry turbo command, actual and status");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x73U, exhaust_pressure_payload,
              sizeof(exhaust_pressure_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 1.0 &&
              decoded.signals[1].value == 2.0,
          "decode dual exhaust pressure");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x74U, turbo_rpm_payload,
              sizeof(turbo_rpm_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 1000.0 &&
              decoded.signals[1].value == 2000.0,
          "decode dual turbocharger speed");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x75U, turbo_temp_payload,
              sizeof(turbo_temp_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 0.0 &&
              decoded.signals[3].value == 80.0,
          "decode turbocharger temperatures");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x77U, charge_temp_payload,
              sizeof(charge_temp_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[3].value == 30.0,
          "decode charge-air cooler temperatures");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x7aU, dpf_pressure_payload,
              sizeof(dpf_pressure_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 3U &&
              decoded.signals[0].value == -1.0 &&
              decoded.signals[2].value == 2.0,
          "decode signed DPF differential and absolute pressures");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x7cU, dpf_temp_payload,
              sizeof(dpf_temp_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 40.0 &&
              decoded.signals[3].value == 160.0,
          "decode corrected DPF temperature payload");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x7fU, runtime_payload,
              sizeof(runtime_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 3U &&
              decoded.signals[0].value == 100.0 &&
              decoded.signals[2].value == 25.0,
          "decode engine, idle and PTO runtime with low support bits");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x83U, nox_payload,
              sizeof(nox_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 100.0 &&
              decoded.signals[3].value == 400.0,
          "decode four NOx concentration channels");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x85U, nox_reagent_payload,
              sizeof(nox_reagent_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 1.0 &&
              decoded.signals[1].value == 2.0 &&
              decoded.signals[3].value == 100.0,
          "decode NOx reagent consumption, level and warning runtime");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x86U, pm_payload,
              sizeof(pm_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 1.0 &&
              decoded.signals[1].value == 2.0,
          "decode particulate-matter concentration");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x87U, map_payload,
              sizeof(map_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 1.0 &&
              decoded.signals[1].value == 2.0,
          "decode dual intake manifold absolute pressure");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x8bU, aftertreatment_payload,
              sizeof(aftertreatment_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 7U &&
              decoded.signals[4].value > 50.19 &&
              decoded.signals[4].value < 50.20 &&
              decoded.signals[6].value == 20.0,
          "decode diesel aftertreatment status and regeneration metrics");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x8cU, wide_o2_payload,
              sizeof(wide_o2_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 8U &&
              decoded.signals[0].value > 0.1525 &&
              decoded.signals[4].value > 0.499 &&
              decoded.signals[4].value < 0.501,
          "decode all eight wide-range oxygen-sensor channels");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x8fU, pm_output_payload,
              sizeof(pm_output_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[1].value == 1.0 &&
              decoded.signals[3].value == 2.0,
          "decode particulate-matter sensor status and output");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x98U, egt_wide_payload,
              sizeof(egt_wide_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 4U &&
              decoded.signals[0].value == 40.0 &&
              decoded.signals[3].value == 160.0,
          "decode wide exhaust-gas temperature channels");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x9aU, hev_payload,
              sizeof(hev_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 100.0 &&
              decoded.signals[1].value == -200.0,
          "decode hybrid battery voltage and signed current");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x9fU, fuel_use_payload,
              sizeof(fuel_use_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 8U &&
              decoded.signals[0].value == 0.0 &&
              decoded.signals[7].value == 100.0,
          "decode all eight fuel-system-use fields without truncation");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xb2U, battery_soh,
              sizeof(battery_soh), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value > 50.19 &&
              decoded.signals[0].value < 50.20,
          "decode traction battery state of health");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xd2U, certified_payload,
              sizeof(certified_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 2U &&
              decoded.signals[0].value == 100.0 &&
              decoded.signals[1].value > 50.19 &&
              decoded.signals[1].value < 50.20,
          "decode certified energy and range");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xd3U, odometer_payload,
              sizeof(odometer_payload), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 10000.0,
          "decode late engine odometer");
    check(link_obd2_decode_pid_payload(
              0x01U, 0x9eU, exhaust_flow,
              sizeof(exhaust_flow), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 1U &&
              decoded.signals[0].value == 20.0 &&
              strcmp(decoded.signals[0].unit, "kg/h") == 0,
          "decode corrected engine exhaust flow");
    check(link_obd2_decode_pid_payload(
              0x01U, 0xccU, raw_motor,
              sizeof(raw_motor), &decoded) == LINK_OBD2_RESULT_OK &&
              decoded.signal_count == 0U &&
              decoded.raw_length == sizeof(raw_motor),
          "preserve assigned electric-motor PID raw when layout is unverified");
}

static void test_complete_mode01_namespace(void)
{
    size_t assigned = 0U, reserved = 0U;
    unsigned int raw;
    check(link_obd2_mode01_identifier_count() == 256U, "all Mode 01 slots exposed");
    check(strcmp(link_obd2_j1979_audit_revision(), "J1979DA_201110+verified-public-updates") == 0,
          "J1979 audit revision");
    for (raw = 0U; raw <= 0xffU; ++raw) {
        const uint8_t pid = (uint8_t)raw;
        const LinkObd2PidDefinition *d = link_obd2_pid_definition(0x01U, pid);
        if (link_obd2_mode01_identifier_status(pid) == LINK_OBD2_IDENTIFIER_ASSIGNED) {
            ++assigned;
            check(d != NULL && d->pid == pid, "assigned slot has definition");
        } else {
            ++reserved;
            check(d == NULL, "reserved slot has no fake definition");
        }
    }
    check(assigned == 220U && reserved == 36U, "220 assigned plus 36 reserved");
    check(assigned == link_obd2_mode01_assigned_count(), "assigned count matches");
}

static void test_requests(void)
{
    char command[16];
    LinkObd2ClearAuthorization authorization = LINK_OBD2_CLEAR_AUTHORIZATION_INIT;

    check(link_obd2_build_standard_read_request(
              0x01U, 0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "generic Mode 01 request");
    check(link_obd2_build_standard_read_request(
              0x05U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_INVALID_ARGUMENT && command[0] == '\0',
          "Mode 05 rejects invalid two-byte shortcut");
    check(link_obd2_build_standard_read_request(
              0x06U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0600") == 0,
          "generic Mode 06 request");
    {
        LinkElm327Response raw_response =
            parse_response("0601", "0601\r460112345678\r>");
        LinkObd2DecodedPid raw_decoded;
        check(link_obd2_decode_standard_identifier_payload(
                  &raw_response, 0x06U, 0x01U, &raw_decoded) ==
                  LINK_OBD2_RESULT_OK &&
              raw_decoded.definition == NULL &&
              raw_decoded.raw_length == 4U &&
              raw_decoded.raw[0] == 0x12U &&
              raw_decoded.raw[3] == 0x78U,
              "Mode 06 unverified monitor payload is preserved losslessly");
    }

    check(link_obd2_build_standard_read_request(
              0x09U, 0x02U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "0902") == 0,
          "generic Mode 09 request");
    check(link_obd2_build_standard_read_request(
              0x04U, 0x00U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 04");
    check(link_obd2_build_standard_read_request(
              0x08U, 0x01U, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "generic read path denies Mode 08");

    check(link_obd2_build_live_pid_request(0x0cU, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "010C") == 0,
          "legacy RPM request remains compatible");

    {
        uint16_t did = 0U;
        uint16_t logical_pid = 0U;
        check(link_obd2_obdonuds_pid_to_did(
                  0x005eU, &did) == LINK_OBD2_RESULT_OK &&
                  did == 0xf45eU,
              "map classic logical PID into J1979-2 DID page");
        check(link_obd2_obdonuds_pid_to_did(
                  0x0100U, &did) == LINK_OBD2_RESULT_OK &&
                  did == 0xf500U,
              "map extended logical PID 100 into J1979-2 DID page");
        check(link_obd2_obdonuds_did_to_pid(
                  0xf5ffU, &logical_pid) == LINK_OBD2_RESULT_OK &&
                  logical_pid == 0x01ffU,
              "reverse-map final supported J1979-2 DID");
        check(link_obd2_build_obdonuds_pid_request(
                  0x0100U, command, sizeof(command)) ==
                  LINK_OBD2_RESULT_OK &&
                  strcmp(command, "22F500") == 0,
              "build OBDonUDS read for extended logical PID");
        check(link_obd2_build_obdonuds_pid_request(
                  0x0200U, command, sizeof(command)) ==
                  LINK_OBD2_RESULT_INVALID_ARGUMENT &&
                  command[0] == '\0',
              "reject unqualified J1979-2 logical PID beyond mapped pages");
    }

    check(link_obd2_build_clear_dtc_request(
              &authorization, command, sizeof(command)) ==
              LINK_OBD2_RESULT_NOT_AUTHORIZED && command[0] == '\0',
          "clear DTC deny by default");
    authorization.confirmed = true;
    authorization.acknowledge_readiness_reset = true;
    check(link_obd2_build_clear_dtc_request(
              &authorization, command, sizeof(command)) ==
              LINK_OBD2_RESULT_OK && strcmp(command, "04") == 0,
          "clear DTC explicit authorization");
}

static void test_live_and_capabilities(void)
{
    LinkElm327Response response;
    LinkObd2Sample sample;
    LinkObd2ResponderSampleList responders;
    LinkObd2PidSet union_set = {{0}};
    LinkObd2ResponderPidSetList responder_sets = {0};
    const LinkObd2PidSet *engine_set;
    const LinkObd2PidSet *secondary_set;
    bool has_more = false;

    response = parse_response("010C", "410C1AF8\r>");
    check(link_obd2_decode_live_pid(&response, 0x0cU, &sample) ==
              LINK_OBD2_RESULT_OK && sample.unit == LINK_OBD2_UNIT_RPM &&
              sample.value == 1726.0,
          "decode RPM");

    {
        LinkObd2DecodedPid decoded;
        response = parse_response(
            "0169",
            "009\r"
            "0: 41 69 3F 80 40 80\r"
            "1: FF 00 40\r>");
        check(link_obd2_decode_live_pid_payload(
                  &response, 0x69U, &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.signal_count == 6U &&
                  decoded.raw_length == 7U &&
                  decoded.signals[3].value == 100.0,
              "assemble and decode indexed multi-line modern live PID");

        response = parse_response("01CC", "41CC010203\r>");
        check(link_obd2_decode_live_pid_payload(
                  &response, 0xccU, &decoded) == LINK_OBD2_RESULT_OK &&
                  decoded.signal_count == 0U &&
                  decoded.raw_length == 3U,
              "read raw-preserved modern assigned live PID");
    }

    response = parse_response(
        "010C", "7E804410C0CF9\r7E904410C0CFC\r>");
    check(link_obd2_decode_live_pid_responders(
              &response, 0x0cU, &responders) == LINK_OBD2_RESULT_OK &&
              responders.count == 2U && !responders.truncated,
          "decode both C207 RPM responders");
    check(responders.samples[0].responder_id_available &&
              responders.samples[0].responder_id == 0x7e8U,
          "retain engine responder identity");
    check(responders.samples[1].responder_id_available &&
              responders.samples[1].responder_id == 0x7e9U,
          "retain secondary responder identity");

    /* Real Jaguar X-Type/X400 ISO 9141-2 capture with ATH1/ATS0. */
    response = parse_response(
        "010C", "486B11410C0BD7F3\r>");
    check(link_obd2_decode_live_pid_responders(
              &response, 0x0cU, &responders) == LINK_OBD2_RESULT_OK &&
              responders.count == 1U && !responders.truncated,
          "decode X400 ISO-9141 RPM response with ATH1 framing");
    check(responders.samples[0].responder_id_available &&
              responders.samples[0].responder_id == 0x11U &&
              !responders.samples[0].extended_id &&
              responders.samples[0].sample.value == 757.75,
          "retain X400 ISO-9141 source and RPM value");

    response = parse_response(
        "010C", "486B11410C0BD7F2\r>");
    check(link_obd2_decode_live_pid_responders(
              &response, 0x0cU, &responders) ==
              LINK_OBD2_RESULT_UNEXPECTED_RESPONSE,
          "do not strip ISO-9141-looking data with an invalid checksum");

    response = parse_response(
        "0100",
        "7E8 06 41 00 98 3B A0 13\r"
        "7E9 06 41 00 98 18 00 01\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x00U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && has_more,
          "decode C207 0x00 capability block by responder");
    response = parse_response(
        "0120",
        "7E8 06 41 20 B0 03 A0 05\r"
        "7E9 06 41 20 80 01 80 01\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x20U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && has_more,
          "decode C207 0x20 capability block by responder");
    response = parse_response(
        "0140",
        "7E8 06 41 40 4C D8 00 00\r"
        "7E9 06 41 40 40 80 00 00\r>");
    check(link_obd2_accept_supported_pid_responders(
              &response, 0x40U, &union_set, &responder_sets, &has_more) ==
              LINK_OBD2_RESULT_OK && !has_more,
          "decode C207 0x40 capability block by responder");

    engine_set = link_obd2_responder_pid_set_find(
        &responder_sets, 0x7e8U, false);
    secondary_set = link_obd2_responder_pid_set_find(
        &responder_sets, 0x7e9U, false);
    check(engine_set != NULL && pid_count(engine_set) == 29U,
          "retain all C207 engine capability bits");
    check(secondary_set != NULL && pid_count(secondary_set) == 12U,
          "retain all C207 secondary capability bits");
    check(link_obd2_pid_set_contains(engine_set, 0x2fU) &&
              link_obd2_pid_set_contains(engine_set, 0x46U) &&
              link_obd2_pid_set_contains(engine_set, 0x4dU),
          "engine set preserves later capability pages");
    check(!link_obd2_pid_set_contains(secondary_set, 0x2fU) &&
              link_obd2_pid_set_contains(secondary_set, 0x42U),
          "secondary capability set remains independent");
}

static void test_vin_response_variants(void)
{
    LinkElm327Response response;
    char vin[LINK_OBD2_VIN_LENGTH + 1U];

    /* Existing byte-aligned Mode 09 response form remains valid. */
    response = parse_response(
        "0902",
        "49020153414A414435\r"
        "490202364C36345744\r"
        "4902033738343335\r>");
    check(link_obd2_decode_vin(&response, vin) == LINK_OBD2_RESULT_OK &&
              strcmp(vin, "SAJAD56L64WD78435") == 0,
          "decode existing non-padded Mode 09 VIN response");

    /* Exact JAGLINK 0.2.70 X-Type/X400 capture: first frame is zero padded. */
    response = parse_response(
        "0902",
        "49020100000053\r"
        "490202414A4144\r"
        "49020335334638\r"
        "49020432584335\r"
        "49020537363233\r>");
    check(link_obd2_decode_vin(&response, vin) == LINK_OBD2_RESULT_OK &&
              strcmp(vin, "SAJAD53F82XC57623") == 0,
          "decode zero-padded ISO-era Mode 09 VIN response");

    /* Zero bytes are padding only before the VIN; never accept them inside it. */
    response = parse_response(
        "0902",
        "4902015341004A4144\r"
        "4902023533463832\r"
        "4902035843353736\r"
        "4902043233\r>");
    check(link_obd2_decode_vin(&response, vin) ==
              LINK_OBD2_RESULT_MALFORMED_RESPONSE,
          "reject zero byte embedded inside VIN data");
}

static void test_negative_response(void)
{
    LinkElm327Response response;
    uint8_t nrc = 0U;

    response = parse_response("0A", "7F0A22\r>");
    check(link_obd2_is_negative_response(
              &response, UINT8_C(0x0a), &nrc) && nrc == UINT8_C(0x22),
          "recognize unavailable permanent-DTC response");
    check(!link_obd2_is_negative_response(
              &response, UINT8_C(0x07), &nrc),
          "do not attribute negative response to another service");
}

int main(void)
{
    test_catalogue();
    test_complete_mode01_namespace();
    test_requests();
    test_live_and_capabilities();
    test_vin_response_variants();
    test_negative_response();

    if (failures != 0) {
        fprintf(stderr, "%d OBD-II test(s) failed\n", failures);
        return 1;
    }
    puts("OBD-II standards tests passed");
    return 0;
}
