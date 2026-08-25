// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/i18n.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool check(bool condition, const char *message)
{
    if (!condition) fprintf(stderr, "link-i18n-test: %s\n", message);
    return condition;
}

int main(void)
{
    InfiltratrI18nArgument argument = {"identity", "ELM327 v1.5"};
    char buffer[160];
    bool passed = true;

    link_i18n_init();
    passed &= check(link_i18n_supported_locale_count() == 15U,
                    "supported locale count mismatch");
    passed &= check(strcmp(link_i18n_locale(), "en-AU") == 0,
                    "canonical locale mismatch");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0,
                    "English vehicle translation mismatch");

    passed &= check(link_i18n_set_locale("en_US"), "en-US selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.live_data.summary"),
                           "Search, select and favorite live diagnostic parameters") == 0,
                    "en-US override mismatch");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0,
                    "en-US fallback mismatch");

    passed &= check(link_i18n_set_locale("de-AT.UTF-8"),
                    "German language fallback selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Fahrzeug") == 0,
                    "German vehicle translation mismatch");
    passed &= check(strcmp(link_i18n_tr("fuel.instantaneous"), "Momentanverbrauch") == 0,
                    "German fuel-economy translation mismatch");
    (void)link_i18n_format(buffer, sizeof(buffer),
                           "connection.linked_starting", &argument, 1U);
    passed &= check(strstr(buffer, "ELM327 v1.5") != NULL,
                    "German placeholder interpolation failed");

    passed &= check(link_i18n_set_locale("pl_PL.UTF-8"),
                    "Polish locale selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Pojazd") == 0,
                    "Polish vehicle translation mismatch");
    passed &= check(strcmp(link_i18n_tr("connection.link_up"), "POŁĄCZ") == 0,
                    "Polish LINK UP translation mismatch");
    passed &= check(strcmp(link_i18n_tr("fuel.average"), "Średnie zużycie paliwa") == 0,
                    "Polish fuel-economy translation mismatch");
    passed &= check(strcmp(link_i18n_supported_locale(7U), "pl-PL") == 0,
                    "Polish locale registry mismatch");
    passed &= check(strcmp(link_i18n_supported_locale_name(7U), "Polski") == 0,
                    "Polish display name mismatch");

    passed &= check(link_i18n_set_locale("fr-FR"), "French locale selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Véhicule") == 0,
                    "French translation mismatch");
    passed &= check(link_i18n_set_locale("es-ES"), "Spanish locale selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Vehículo") == 0,
                    "Spanish translation mismatch");
    passed &= check(link_i18n_set_locale("it-IT"), "Italian locale selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Veicolo") == 0,
                    "Italian translation mismatch");


    passed &= check(link_i18n_set_locale("en-GB"), "en-GB selection failed");
    passed &= check(strcmp(link_i18n_locale(), "en-GB") == 0,
          "en-GB locale identity mismatch");
    passed &= check(link_i18n_set_locale("es-419"), "Latin-American Spanish selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Vehículo") == 0,
          "Latin-American Spanish fallback mismatch");
    passed &= check(link_i18n_set_locale("pt-BR"), "Brazilian Portuguese selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Veículo") == 0,
          "Brazilian Portuguese translation mismatch");
    passed &= check(link_i18n_set_locale("zh-CN"), "Simplified Chinese selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "车辆") == 0,
          "Simplified Chinese translation mismatch");
    passed &= check(link_i18n_set_locale("hi-IN"), "Hindi selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "वाहन") == 0,
          "Hindi translation mismatch");
    passed &= check(link_i18n_set_locale("ar"), "Arabic selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "المركبة") == 0,
          "Arabic translation mismatch");
    passed &= check(link_i18n_set_locale("ja-JP"), "Japanese selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "車両") == 0,
          "Japanese translation mismatch");
    passed &= check(link_i18n_set_locale("ko-KR"), "Korean selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "차량") == 0,
          "Korean translation mismatch");
    passed &= check(link_i18n_set_locale("id-ID"), "Indonesian selection failed");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Kendaraan") == 0,
          "Indonesian translation mismatch");
    passed &= check(strcmp(link_i18n_supported_locale(14U), "id-ID") == 0,
          "15-language registry ordering mismatch");

    {
        const char *pack_path = "test-custom-language.lang";
        FILE *pack_file = fopen(pack_path, "wb");
        passed &= check(pack_file != NULL, "custom language pack file create failed");
        if (pack_file != NULL) {
            const char contents[] =
                "locale=sv-SE\n"
                "name=Svenska\n"
                "direction=ltr\n"
                "version=1\n"
                "nav.vehicle=Fordon\n"
                "Custom literal=Egen text\n";
            passed &= check(fwrite(contents, 1U, sizeof(contents) - 1U, pack_file) == sizeof(contents) - 1U,
                            "custom language pack write failed");
            fclose(pack_file);
            passed &= check(link_i18n_load_language_pack(pack_path),
                            "custom language pack load failed");
            passed &= check(link_i18n_installed_locale_count() == 16U,
                            "custom language pack did not extend installed registry");
            passed &= check(link_i18n_select_locale("sv-SE"),
                            "custom language selection failed");
            passed &= check(strcmp(link_i18n_selected_locale(), "sv-SE") == 0,
                            "custom language identity mismatch");
            passed &= check(strcmp(link_i18n_text("nav.vehicle"), "Fordon") == 0,
                            "custom language translation mismatch");
            passed &= check(strcmp(link_i18n_text("nav.faults"), "Faults") == 0,
                            "custom language en-AU fallback mismatch");
            passed &= check(strcmp(link_i18n_text("Custom literal"), "Egen text") == 0,
                            "custom literal translation mismatch");
            (void)remove(pack_path);
            link_i18n_clear_language_packs();
            passed &= check(link_i18n_installed_locale_count() == 15U,
                            "language pack clear did not restore built-in registry");
        }
    }

    passed &= check(link_i18n_set_locale("pt-BR"), "pt-BR Linux shell selection failed");
    passed &= check(strcmp(link_i18n_tr("linux.vehicle.profile"), "PERFIL DO VEÍCULO") == 0,
                    "pt-BR Linux vehicle-profile translation mismatch");
    passed &= check(strcmp(link_i18n_tr("linux.connection.select_adapter"),
                           "Selecione um adaptador acima e pressione CONECTAR") == 0,
                    "pt-BR Linux connection guidance translation mismatch");
    passed &= check(link_i18n_set_locale("zh-CN"), "zh-CN Linux shell selection failed");
    passed &= check(strcmp(link_i18n_tr("linux.vehicle.evidence"), "车辆证据") == 0,
                    "zh-CN Linux vehicle-evidence translation mismatch");
    {
        InfiltratrI18nArgument count_argument = {"count", "4"};
        (void)link_i18n_format(buffer, sizeof(buffer), "linux.networks.defined",
                               &count_argument, 1U);
        passed &= check(strcmp(buffer, "已定义 4 个网络") == 0,
                        "zh-CN network-count interpolation mismatch");
    }

    passed &= check(!link_i18n_set_locale("zz-ZZ"),
                    "unknown locale should not be accepted");
    passed &= check(strcmp(link_i18n_tr("nav.vehicle"), "Vehicle") == 0,
                    "unknown locale should fall back to en-AU");

    return passed ? EXIT_SUCCESS : EXIT_FAILURE;
}
