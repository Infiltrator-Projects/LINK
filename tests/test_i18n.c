// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/i18n.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    InfiltratrI18nArgument argument = {"identity", "ELM327 v1.5"};
    char buffer[160];

    link_i18n_init();
    assert(link_i18n_supported_locale_count() == 6U);
    assert(strcmp(link_i18n_locale(), "en-AU") == 0);
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0);

    assert(link_i18n_set_locale("en_US"));
    assert(strcmp(link_i18n_tr("nav.live_data.summary"),
                  "Search, select and favorite live diagnostic parameters") == 0);
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0);

    assert(link_i18n_set_locale("de-AT.UTF-8"));
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Fahrzeug") == 0);
    (void)link_i18n_format(buffer, sizeof(buffer),
                           "connection.linked_starting", &argument, 1U);
    assert(strstr(buffer, "ELM327 v1.5") != NULL);

    assert(link_i18n_set_locale("fr-FR"));
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Véhicule") == 0);
    assert(link_i18n_set_locale("es-ES"));
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Vehículo") == 0);
    assert(link_i18n_set_locale("it-IT"));
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Veicolo") == 0);

    assert(!link_i18n_set_locale("zz-ZZ"));
    assert(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0);
    return 0;
}
