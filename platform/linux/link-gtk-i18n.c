// SPDX-License-Identifier: GPL-3.0-or-later
#define LINK_GTK_I18N_IMPLEMENTATION 1
#include "link-gtk-i18n.h"

#include "link/i18n.h"

#include <stdio.h>
#include <string.h>

static int link_gtk_i18n_initialised;

typedef struct LinkGtkLiteralTranslation {
    const char *english;
    const char *german;
    const char *polish;
} LinkGtkLiteralTranslation;

static const LinkGtkLiteralTranslation literal_translations[] = {
    {"Language", "Sprache", "Język"},
    {"English", "Englisch", "Angielski"},
    {"German", "Deutsch", "Niemiecki"},
    {"Polish", "Polnisch", "Polski"},
    {"Diagnostics", "Diagnose", "Diagnostyka"},
    {"LINKED · ELM327 VERIFIED", "VERBUNDEN · ELM327 BESTÄTIGT", "POŁĄCZONO · ELM327 ZWERYFIKOWANY"},
    {"NOT LINKED", "NICHT VERBUNDEN", "NIEPOŁĄCZONO"},
    {"LINK OFFLINE", "LINK GETRENNT", "LINK ROZŁĄCZONY"},
    {"STARTING DIAGNOSTICS", "DIAGNOSE WIRD GESTARTET", "URUCHAMIANIE DIAGNOSTYKI"},
    {"DIAGNOSTIC SESSION FAILED", "DIAGNOSESITZUNG FEHLGESCHLAGEN", "SESJA DIAGNOSTYCZNA NIE POWIODŁA SIĘ"},
    {"LIVE DIAGNOSTICS ACTIVE", "LIVE-DIAGNOSE AKTIV", "DIAGNOSTYKA NA ŻYWO AKTYWNA"},
    {"Estimated", "Geschätzt", "Szacowane"},
    {"Mixed measured sources", "Gemischte Messquellen", "Mieszane źródła pomiarowe"},
    {"Unavailable", "Nicht verfügbar", "Niedostępne"},
    {"No adapter", "Kein Adapter", "Brak adaptera"},
    {"LINK carries the Linux connection directly into ELM initialisation, supported-PID discovery, stored/pending/permanent OBD-II fault inventory and live polling.",
     "LINK führt die Linux-Verbindung direkt zur ELM-Initialisierung, Erkennung unterstützter PIDs, zum Lesen gespeicherter/anstehender/permanenter OBD-II-Fehler und zur Live-Abfrage.",
     "LINK prowadzi połączenie Linux bezpośrednio do inicjalizacji ELM, wykrywania obsługiwanych PID-ów, odczytu zapisanych/oczekujących/trwałych usterek OBD-II oraz odpytywania na żywo."},
    {"Jaguar-specific topology remains in JAGLINK while the standard transport, fault and live-data engine is shared through LINK.",
     "Die Jaguar-spezifische Topologie bleibt in JAGLINK, während Standardtransport, Fehler- und Live-Daten-Engine über LINK gemeinsam genutzt werden.",
     "Topologia specyficzna dla Jaguara pozostaje w JAGLINK, a standardowy transport, obsługa usterek i dane na żywo są współdzielone przez LINK."},
    {"Standards-based powertrain DTC decoding is provided by LINK and remains read-only.",
     "Die standardbasierte Antriebsstrang-DTC-Decodierung wird von LINK bereitgestellt und bleibt schreibgeschützt.",
     "Oparte na standardach dekodowanie kodów DTC układu napędowego zapewnia LINK i pozostaje tylko do odczytu."},
    {"Jaguar-specific module acquisition remains gated by verified X400 module addresses and safe read-only requests.",
     "Die Jaguar-spezifische Modulerfassung bleibt auf verifizierte X400-Moduladressen und sichere Nur-Lese-Anfragen beschränkt.",
     "Odczyt modułów specyficznych dla Jaguara pozostaje ograniczony do zweryfikowanych adresów modułów X400 i bezpiecznych żądań tylko do odczytu."},
    {"LINK prefers a verified Jaguar factory value when available, otherwise uses measured SAE PID 0x5E fuel rate with PID 0x0D vehicle speed. Estimates are never presented as measured data.",
     "LINK bevorzugt einen verifizierten Jaguar-Werkswert; andernfalls werden die gemessene SAE-Kraftstoffrate PID 0x5E und die Fahrzeuggeschwindigkeit PID 0x0D verwendet. Schätzwerte werden nie als Messwerte dargestellt.",
     "LINK preferuje zweryfikowaną wartość fabryczną Jaguara; w przeciwnym razie używa zmierzonego przepływu paliwa SAE PID 0x5E z prędkością pojazdu PID 0x0D. Wartości szacowane nigdy nie są przedstawiane jako pomiary."},
    {"Time-series traces receive real LINK telemetry samples from the active Linux diagnostic flow.",
     "Zeitreihen erhalten reale LINK-Telemetriemesswerte aus dem aktiven Linux-Diagnoseablauf.",
     "Wykresy czasowe otrzymują rzeczywiste próbki telemetrii LINK z aktywnego przebiegu diagnostyki Linux."},
    {"Raw requests, responses and telemetry use the shared evidence path; no synthetic vehicle data is displayed.",
     "Rohanforderungen, Antworten und Telemetrie verwenden den gemeinsamen Nachweispfad; synthetische Fahrzeugdaten werden nicht angezeigt.",
     "Surowe żądania, odpowiedzi i telemetria korzystają ze wspólnej ścieżki danych dowodowych; nie są wyświetlane żadne syntetyczne dane pojazdu."},
    {"Waiting", "Warten", "Oczekiwanie"},
    {"None reported", "Keine gemeldet", "Brak zgłoszonych"},
    {"Platform", "Plattform", "Platforma"},
    {"Engine", "Motor", "Silnik"},
    {"Family", "Familie", "Rodzina"},
    {"Profile", "Profil", "Profil"},
    {"Engine ECU", "Motorsteuergerät", "Sterownik silnika"},
    {"Definition", "Definition", "Definicja"},
    {"Physical CAN", "Physischer CAN", "Fizyczny CAN"},
    {"Model years", "Modelljahre", "Lata modelowe"},
    {"Network map", "Netzwerkübersicht", "Mapa sieci"},
    {"Adapter", "Adapter", "Adapter"},
    {"Select an adapter above and press LINK UP", "Oben einen Adapter auswählen und VERBINDEN drücken", "Wybierz adapter powyżej i naciśnij POŁĄCZ"},
    {"Diagnostic flow", "Diagnoseablauf", "Przebieg diagnostyki"},
    {"VEHICLE EVIDENCE", "FAHRZEUGNACHWEIS", "DANE POJAZDU"},
    {"VEHICLE PROFILE", "FAHRZEUGPROFIL", "PROFIL POJAZDU"},
    {"CONNECTION", "VERBINDUNG", "POŁĄCZENIE"},
    {"Linux diagnostic link", "Linux-Diagnoseverbindung", "Połączenie diagnostyczne Linux"},
    {"MERCEDES PROFILE", "MERCEDES-PROFIL", "PROFIL MERCEDES"},
    {"Known ECU endpoints", "Bekannte ECU-Endpunkte", "Znane punkty końcowe ECU"},
    {"NO ENDPOINT DEFINITIONS", "KEINE ENDPUNKTDEFINITIONEN", "BRAK DEFINICJI PUNKTÓW KOŃCOWYCH"},
    {"MERCEDES ENGINE", "MERCEDES-MOTOR", "SILNIK MERCEDES"},
    {"Manufacturer profile status", "Status des Herstellerprofils", "Stan profilu producenta"},
    {"STANDARD OBD-II", "STANDARD OBD-II", "STANDARD OBD-II"},
    {"Stored, pending and permanent faults", "Gespeicherte, anstehende und permanente Fehler", "Usterki zapisane, oczekujące i trwałe"},
    {"PROFILE READY", "PROFIL BEREIT", "PROFIL GOTOWY"},
    {"NOT SCANNED · LINK OFFLINE", "NICHT GESCANNT · LINK OFFLINE", "NIE SKANOWANO · LINK OFFLINE"},
    {"STARTING SCAN", "SCAN WIRD GESTARTET", "URUCHAMIANIE SKANOWANIA"},
    {"SCAN FAILED · RECONNECT TO RETRY", "SCAN FEHLGESCHLAGEN · ZUM WIEDERHOLEN NEU VERBINDEN", "SKANOWANIE NIEUDANE · POŁĄCZ PONOWNIE"},
    {"Stored", "Gespeichert", "Zapisane"},
    {"Pending", "Anstehend", "Oczekujące"},
    {"Permanent", "Permanent", "Trwałe"},
    {"PARAMETER TABLE", "PARAMETERTABELLE", "TABELA PARAMETRÓW"},
    {"LIVE DATA CATALOGUE", "LIVE-DATENKATALOG", "KATALOG DANYCH NA ŻYWO"},
    {"Real standard OBD-II samples", "Echte Standard-OBD-II-Messwerte", "Rzeczywiste próbki standardowego OBD-II"},
    {"Available shared diagnostic parameters", "Verfügbare gemeinsame Diagnoseparameter", "Dostępne wspólne parametry diagnostyczne"},
    {"Not supported by vehicle", "Vom Fahrzeug nicht unterstützt", "Nieobsługiwane przez pojazd"},
    {"Waiting for sample", "Warten auf Messwert", "Oczekiwanie na próbkę"},
    {"No live session", "Keine Live-Sitzung", "Brak sesji na żywo"},
    {"FUEL ECONOMY", "KRAFTSTOFFVERBRAUCH", "ZUŻYCIE PALIWA"},
    {"Fuel use and trip consumption", "Kraftstoff- und Fahrtverbrauch", "Zużycie paliwa i zużycie na trasie"},
    {"— · stationary / awaiting speed", "— · Stillstand / Warten auf Geschwindigkeit", "— · postój / oczekiwanie na prędkość"},
    {"Waiting for measured fuel data", "Warten auf gemessene Kraftstoffdaten", "Oczekiwanie na zmierzone dane paliwa"},
    {"Waiting for trip distance", "Warten auf Fahrstrecke", "Oczekiwanie na dystans podróży"},
    {"Not available", "Nicht verfügbar", "Niedostępne"},
    {"MEASURED FUEL DATA ACTIVE", "GEMESSENE KRAFTSTOFFDATEN AKTIV", "ZMIERZONE DANE PALIWA AKTYWNE"},
    {"WAITING FOR FUEL DATA", "WARTEN AUF KRAFTSTOFFDATEN", "OCZEKIWANIE NA DANE PALIWA"},
    {"Instantaneous", "Momentan", "Chwilowe"},
    {"Trip average", "Fahrtdurchschnitt", "Średnia z podróży"},
    {"Fuel rate", "Kraftstoffrate", "Przepływ paliwa"},
    {"Trip", "Fahrt", "Podróż"},
    {"Current source", "Aktuelle Quelle", "Bieżące źródło"},
    {"Mercedes factory direct", "Mercedes-Werkswert direkt", "Bezpośrednia wartość fabryczna Mercedes"},
    {"Mercedes factory counters", "Mercedes-Werkszähler", "Fabryczne liczniki Mercedes"},
    {"Mercedes factory fuel rate", "Mercedes-Werks-Kraftstoffrate", "Fabryczny przepływ paliwa Mercedes"},
    {"Mercedes factory source", "Mercedes-Werksquelle", "Fabryczne źródło Mercedes"},
    {"Jaguar factory direct", "Jaguar-Werkswert direkt", "Bezpośrednia wartość fabryczna Jaguar"},
    {"Jaguar factory counters", "Jaguar-Werkszähler", "Fabryczne liczniki Jaguar"},
    {"Jaguar factory fuel rate", "Jaguar-Werks-Kraftstoffrate", "Fabryczny przepływ paliwa Jaguar"},
    {"X400 factory signal", "X400-Werkssignal", "Sygnał fabryczny X400"},
    {"decoder verified", "Decoder bestätigt", "dekoder zweryfikowany"},
    {"decoder not yet vehicle-verified", "Decoder noch nicht am Fahrzeug bestätigt", "dekoder niezweryfikowany jeszcze w pojeździe"},
    {"AT-A-GLANCE", "AUF EINEN BLICK", "W SKRÓCIE"},
    {"Powertrain dashboard", "Antriebsstrang-Übersicht", "Panel układu napędowego"},
    {"Jaguar powertrain dashboard", "Jaguar-Antriebsstrang-Übersicht", "Panel układu napędowego Jaguar"},
    {"LIVE SAMPLES", "LIVE-MESSWERTE", "PRÓBKI NA ŻYWO"},
    {"INSTRUMENT TRACES", "INSTRUMENTENVERLÄUFE", "PRZEBIEGI WSKAŹNIKÓW"},
    {"Signal history", "Signalverlauf", "Historia sygnału"},
    {"SESSION RECORDER", "SITZUNGSAUFZEICHNUNG", "REJESTRATOR SESJI"},
    {"Diagnostic evidence", "Diagnosenachweis", "Dane diagnostyczne"},
    {"System identity", "Systemidentität", "Tożsamość systemu"},
    {"Version", "Version", "Wersja"},
    {"Product", "Produkt", "Produkt"},
    {"Portable core", "Portabler Kern", "Przenośny rdzeń"},
    {"Validated", "Validiert", "Zweryfikowany"},
    {"Invalid metadata", "Ungültige Metadaten", "Nieprawidłowe metadane"},
    {"Linux transport", "Linux-Transport", "Transport Linux"},
    {"Linux diagnostic flow", "Linux-Diagnoseablauf", "Przebieg diagnostyki Linux"},
    {"Fuel economy", "Kraftstoffverbrauch", "Zużycie paliwa"},
    {"Mercedes-Benz diagnostics", "Mercedes-Benz-Diagnose", "Diagnostyka Mercedes-Benz"},
    {"Jaguar X-Type X400 diagnostics", "Jaguar-X-Type-X400-Diagnose", "Diagnostyka Jaguar X-Type X400"},
    {"LINK serial ELM327 provider", "LINK serieller ELM327-Anbieter", "Dostawca szeregowy ELM327 LINK"},
    {"Automatic PID + DTC + live polling", "Automatische PID- + DTC- + Live-Abfrage", "Automatyczne PID + DTC + odpytywanie na żywo"},
    {"Factory-priority + SAE measured fallback", "Werkswert-Priorität + gemessener SAE-Fallback", "Priorytet fabryczny + mierzony fallback SAE"},
    {"X400 NETWORK TOPOLOGY", "X400-NETZWERKTOPOLOGIE", "TOPOLOGIA SIECI X400"},
    {"Diagnostic networks and module paths", "Diagnosenetzwerke und Modulpfade", "Sieci diagnostyczne i ścieżki modułów"},
    {"NO NETWORK DEFINITIONS", "KEINE NETZWERKDEFINITIONEN", "BRAK DEFINICJI SIECI"},
    {"JAGUAR MODULES", "JAGUAR-MODULE", "MODUŁY JAGUAR"},
    {"X400 PROFILE READY", "X400-PROFIL BEREIT", "PROFIL X400 GOTOWY"},
    {"Refresh", "Aktualisieren", "Odśwież"},
    {"Disconnected", "Getrennt", "Rozłączono"},
    {"About", "Info", "O programie"},
    {"MERCEDES-BENZ · C207 / OM651", "MERCEDES-BENZ · C207 / OM651", "MERCEDES-BENZ · C207 / OM651"},
    {"JAGUAR X-TYPE · X400", "JAGUAR X-TYPE · X400", "JAGUAR X-TYPE · X400"}
};

static void ensure_locale(void)
{
    if (link_gtk_i18n_initialised) return;
    link_i18n_init();
    (void)link_i18n_set_system_locale();
    link_gtk_i18n_initialised = 1;
}

static int selected_language(void)
{
    const char *locale = link_i18n_locale();
    if (locale != NULL && strncmp(locale, "de", 2U) == 0) return 1;
    if (locale != NULL && strncmp(locale, "pl", 2U) == 0) return 2;
    return 0;
}

static const char *translation_key(const char *text)
{
    static const struct {
        const char *text;
        const char *key;
    } mappings[] = {
        {"Refresh", "common.refresh"},
        {"Disconnected", "connection.disconnected"},
        {"LINK UP", "connection.link_up"},
        {"LINK DOWN", "connection.link_down"},
        {"Adapter", "common.adapter"},
        {"No adapter", "linux.no_adapter"},
        {"NOT LINKED", "linux.status.not_linked"},
        {"LINK OFFLINE", "linux.status.offline"},
        {"VEHICLE EVIDENCE", "linux.vehicle.evidence"},
        {"VEHICLE PROFILE", "linux.vehicle.profile"},
        {"CONNECTION", "linux.section.connection"},
        {"Linux diagnostic link", "linux.connection.title"},
        {"Platform", "linux.label.platform"},
        {"Engine", "linux.label.engine"},
        {"Family", "linux.label.family"},
        {"Profile", "linux.label.profile"},
        {"Engine ECU", "linux.label.engine_ecu"},
        {"Definition", "linux.label.definition"},
        {"Physical CAN", "linux.label.physical_can"},
        {"Model years", "linux.label.model_years"},
        {"Network map", "linux.label.network_map"},
        {"Select an adapter above and press LINK UP", "linux.connection.select_adapter"},
        {"Diagnostic flow", "linux.label.diagnostic_flow"},
        {"LINK carries the Linux connection directly into ELM initialisation, supported-PID discovery, stored/pending/permanent OBD-II fault inventory and live polling.", "linux.connection.description"},
        {"source-corroborated", "linux.value.source_corroborated"},
        {"Mercedes-Benz Diagnostics", "linux.title.mercedes"},
        {"Jaguar X400 Diagnostics", "linux.title.jaguar"},
        {"About", "common.about"},
        {"No ELM327 serial device detected", "connection.no_device"},
        {"Invalid adapter configuration", "connection.invalid_config"},
        {"Unable to open adapter · check dialout permissions", "connection.open_failed"},
        {"Device opened but ELM327 identity handshake failed", "connection.handshake_failed"},
        {"ELM327-compatible adapter", "connection.adapter_generic"},
        {"Diagnostic state unavailable", "diagnostics.unavailable"},
        {"Linked · diagnostic session idle", "diagnostics.idle"},
        {"Linked · initialising ELM327 adapter", "diagnostics.initialising"},
        {"Linked · discovering supported OBD-II PIDs", "diagnostics.discovering_pids"},
        {"Linked · manufacturer extension pending", "diagnostics.manufacturer_pending"},
        {"Linked · restoring standard OBD-II channel", "diagnostics.restoring"},
        {"Linked · scanning stored OBD-II faults", "diagnostics.stored_dtcs"},
        {"Linked · scanning pending OBD-II faults", "diagnostics.pending_dtcs"},
        {"Linked · scanning permanent OBD-II faults", "diagnostics.permanent_dtcs"},
        {"Linked · live OBD-II polling active", "diagnostics.live"},
        {"Linked · diagnostic session failed · LINK DOWN / LINK UP to retry", "diagnostics.failed"},
        {"Linked · diagnostics active", "diagnostics.active"},
        {"Linked · diagnostics ready", "diagnostics.ready"}
    };
    size_t index;
    if (text == NULL) return NULL;
    for (index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (strcmp(text, mappings[index].text) == 0) return mappings[index].key;
    }
    return NULL;
}

static const char *literal_translate(const char *text)
{
    const int language = selected_language();
    size_t index;
    if (text == NULL || language == 0) return text;
    for (index = 0U; index < sizeof(literal_translations) / sizeof(literal_translations[0]); ++index) {
        if (strcmp(text, literal_translations[index].english) == 0) {
            return language == 1 ? literal_translations[index].german
                                 : literal_translations[index].polish;
        }
    }
    return text;
}

const char *link_gtk_i18n_translate_text(const char *text)
{
    const char *key;
    const char *translated;
    static char dynamic[320];
    static const char prefix[] = "Linked · ";
    static const char suffix[] = " · starting diagnostics";
    size_t length;
    size_t prefix_length;
    size_t suffix_length;

    ensure_locale();
    if (text == NULL) return "";
    {
        const char *packed_translation = link_i18n_text(text);
        if (packed_translation != NULL && strcmp(packed_translation, text) != 0)
            return packed_translation;
    }
    key = translation_key(text);
    if (key != NULL) return link_i18n_text(key);


translated = literal_translate(text);
if (translated != text) return translated;

/* Compact parameter-table rows keep the technical PID while
   replacing the English short label with the selected-language full name. */
if (strncmp(text, "PID 0x", 6U) == 0 && strstr(text, " · ") != NULL) {
    unsigned int pid;
    if (sscanf(text, "PID 0x%2X", &pid) == 1 &&
        link_i18n_format_obd2_pid_label(dynamic, sizeof(dynamic), pid) != 0U)
        return dynamic;
}

length = strlen(text);
    prefix_length = sizeof(prefix) - 1U;
    suffix_length = sizeof(suffix) - 1U;
    if (length > prefix_length + suffix_length &&
        strncmp(text, prefix, prefix_length) == 0 &&
        strcmp(text + length - suffix_length, suffix) == 0) {
        char identity[160];
        size_t identity_length = length - prefix_length - suffix_length;
        InfiltratrI18nArgument argument;
        if (identity_length >= sizeof(identity)) identity_length = sizeof(identity) - 1U;
        memcpy(identity, text + prefix_length, identity_length);
        identity[identity_length] = '\0';
        argument.name = "identity";
        argument.value = identity;
        (void)link_i18n_format(dynamic, sizeof(dynamic),
                               "connection.linked_starting", &argument, 1U);
        return dynamic;
    }

    /* A few formatted screen messages are deliberately handled by shape so
       the diagnostic numbers/codes remain untouched while their prose changes. */
    {
        size_t stored_count, pending_count, permanent_count;
        if (sscanf(text, "COMPLETE · %zu stored · %zu pending · %zu permanent",
                   &stored_count, &pending_count, &permanent_count) == 3) {
            char stored_text[32];
            char pending_text[32];
            char permanent_text[32];
            InfiltratrI18nArgument arguments[3];
            (void)snprintf(stored_text, sizeof(stored_text), "%zu", stored_count);
            (void)snprintf(pending_text, sizeof(pending_text), "%zu", pending_count);
            (void)snprintf(permanent_text, sizeof(permanent_text), "%zu", permanent_count);
            arguments[0].name = "stored";
            arguments[0].value = stored_text;
            arguments[1].name = "pending";
            arguments[1].value = pending_text;
            arguments[2].name = "permanent";
            arguments[2].value = permanent_text;
            (void)link_i18n_format_text(dynamic, sizeof(dynamic),
                                        "linux.faults.complete", arguments, 3U);
            return dynamic;
        }
    }
    {
        size_t network_count;
        if (sscanf(text, "%zu defined networks", &network_count) == 1 &&
            strstr(text, "defined networks") != NULL) {
            char count_text[32];
            InfiltratrI18nArgument argument;
            (void)snprintf(count_text, sizeof(count_text), "%zu", network_count);
            argument.name = "count";
            argument.value = count_text;
            (void)link_i18n_format_text(dynamic, sizeof(dynamic),
                                        "linux.networks.defined", &argument, 1U);
            return dynamic;
        }
    }
    return text;
}

GtkWidget *link_gtk_i18n_label_new(const char *text)
{
    return gtk_label_new(link_gtk_i18n_translate_text(text));
}

void link_gtk_i18n_label_set_text(GtkLabel *label, const char *text)
{
    gtk_label_set_text(label, link_gtk_i18n_translate_text(text));
}

GtkWidget *link_gtk_i18n_button_new_with_label(const char *text)
{
    return gtk_button_new_with_label(link_gtk_i18n_translate_text(text));
}

void link_gtk_i18n_button_set_label(GtkButton *button, const char *text)
{
    gtk_button_set_label(button, link_gtk_i18n_translate_text(text));
}
