// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_data_ids.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", \
                #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int main(void)
{
    const LinkMercedesMeDataIdDefinition *item;
    size_t index;
    size_t other;

    CHECK(link_mercedes_me_data_id_count() == 194U);
    CHECK(link_mercedes_me_data_id_at(194U) == NULL);

    item = link_mercedes_me_data_id_find_symbol(
        "RELATIVE_ACCELERATOR_PEDAL_POSITION");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "relativeAcceleratorPedalPosition") == 0);

    item = link_mercedes_me_data_id_find_symbol("THROTTLE_POSITION");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "throttlePosition") == 0);

    item = link_mercedes_me_data_id_find_symbol("ENGINE_REFERENCE_THROTTLE");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "engineReferenceThrottle") == 0);

    item = link_mercedes_me_data_id_find_symbol("STORED_OBD_DTCS");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "storedObdDtcs") == 0);

    item = link_mercedes_me_data_id_find_symbol("PARTICLE_FILTER");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "particleFilter") == 0);

    item = link_mercedes_me_data_id_find_symbol("IRREGULAR_OBD_RESPONSE");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "irregularObdResponse") == 0);

    item = link_mercedes_me_data_id_find_symbol("SPEED_AND_FUEL_VALUES");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "speedAndFuelValues") == 0);

    item = link_mercedes_me_data_id_find_symbol("FUEL_VOLUME");
    CHECK(item != NULL);
    CHECK(strcmp(item->data_id, "fuelVolume") == 0);
    CHECK(strcmp(link_mercedes_me_data_id_find_symbol("FUEL_LEVEL_MIN")->data_id,
                 "fuelLevelMin") == 0);
    CHECK(strcmp(link_mercedes_me_data_id_find_symbol("ENGINE_FUEL_RATE")->data_id,
                 "engineFuelRate") == 0);
    CHECK(strcmp(link_mercedes_me_data_id_find_symbol("TANK_RANGE")->data_id,
                 "tankRange") == 0);

    CHECK(strcmp(link_mercedes_me_data_id_find_symbol(
                     "BT_RX_OVERFLOW_COUNT")->data_id,
                 "btRxOverflowCount") == 0);
    CHECK(strcmp(link_mercedes_me_data_id_find_symbol(
                     "CAN_TX_OVERFLOW_COUNT")->data_id,
                 "canTxOverflowCount") == 0);
    CHECK(strcmp(link_mercedes_me_data_id_find_symbol(
                     "BUS_ERROR_COUNT")->data_id,
                 "busErrorCount") == 0);

    CHECK(link_mercedes_me_data_id_literal_match_count("km") == 2U);
    CHECK(link_mercedes_me_data_id_literal_match("km", 0U) != NULL);
    CHECK(link_mercedes_me_data_id_literal_match("km", 1U) != NULL);
    CHECK(link_mercedes_me_data_id_literal_match("km", 2U) == NULL);
    CHECK(link_mercedes_me_data_id_literal_match_count("missing") == 0U);

    for (index = 0U; index < link_mercedes_me_data_id_count(); ++index) {
        const LinkMercedesMeDataIdDefinition *left =
            link_mercedes_me_data_id_at(index);
        CHECK(left != NULL);
        CHECK(left->symbol != NULL && left->symbol[0] != '\0');
        CHECK(left->data_id != NULL && left->data_id[0] != '\0');
        for (other = index + 1U;
             other < link_mercedes_me_data_id_count(); ++other) {
            const LinkMercedesMeDataIdDefinition *right =
                link_mercedes_me_data_id_at(other);
            CHECK(right != NULL);
            CHECK(strcmp(left->symbol, right->symbol) != 0);
        }
    }

    return 0;
}
