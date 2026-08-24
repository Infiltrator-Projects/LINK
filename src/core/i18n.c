// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/i18n.h"

#include <string.h>

#define ENTRY(key_, value_) { (key_), (value_) }
#define COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static const InfiltratrI18nEntry en_au[] = {
    ENTRY("nav.vehicle", "Vehicle"),
    ENTRY("nav.vehicle.summary", "Vehicle identity, adapter and connection information"),
    ENTRY("nav.modules", "Modules"),
    ENTRY("nav.modules.summary", "Discovered control modules and ECU identification"),
    ENTRY("nav.faults", "Faults"),
    ENTRY("nav.faults.summary", "Diagnostic trouble codes by control module"),
    ENTRY("nav.live_data", "Live Data"),
    ENTRY("nav.live_data.summary", "Search, select and favourite live diagnostic parameters"),
    ENTRY("nav.table", "Table"),
    ENTRY("nav.table.summary", "Dense live values for selected diagnostic parameters"),
    ENTRY("nav.dashboard", "Dashboard"),
    ENTRY("nav.dashboard.summary", "At-a-glance live diagnostic measurements"),
    ENTRY("nav.graphs", "Graphs"),
    ENTRY("nav.graphs.summary", "Time-series views for selected diagnostic parameters"),
    ENTRY("nav.log", "Log"),
    ENTRY("nav.log.summary", "Diagnostic session history and exported telemetry"),
    ENTRY("nav.settings", "Settings"),
    ENTRY("nav.settings.summary", "Display, adapter, units, logging and application preferences"),
    ENTRY("common.about", "About"),
    ENTRY("common.adapter", "Adapter"),
    ENTRY("common.refresh", "Refresh"),
    ENTRY("language.label", "Language"),
    ENTRY("language.system", "System Default"),
    ENTRY("connection.disconnected", "Disconnected"),
    ENTRY("connection.link_up", "LINK UP"),
    ENTRY("connection.link_down", "LINK DOWN"),
    ENTRY("connection.no_device", "No ELM327 serial device detected"),
    ENTRY("connection.invalid_config", "Invalid adapter configuration"),
    ENTRY("connection.open_failed", "Unable to open adapter · check dialout permissions"),
    ENTRY("connection.handshake_failed", "Device opened but ELM327 identity handshake failed"),
    ENTRY("connection.adapter_generic", "ELM327-compatible adapter"),
    ENTRY("connection.linked_starting", "Linked · {identity} · starting diagnostics"),
    ENTRY("diagnostics.unavailable", "Diagnostic state unavailable"),
    ENTRY("diagnostics.idle", "Linked · diagnostic session idle"),
    ENTRY("diagnostics.initialising", "Linked · initialising ELM327 adapter"),
    ENTRY("diagnostics.discovering_pids", "Linked · discovering supported OBD-II PIDs"),
    ENTRY("diagnostics.manufacturer_pending", "Linked · manufacturer extension pending"),
    ENTRY("diagnostics.restoring", "Linked · restoring standard OBD-II channel"),
    ENTRY("diagnostics.stored_dtcs", "Linked · scanning stored OBD-II faults"),
    ENTRY("diagnostics.pending_dtcs", "Linked · scanning pending OBD-II faults"),
    ENTRY("diagnostics.permanent_dtcs", "Linked · scanning permanent OBD-II faults"),
    ENTRY("diagnostics.live", "Linked · live OBD-II polling active"),
    ENTRY("diagnostics.failed", "Linked · diagnostic session failed · LINK DOWN / LINK UP to retry"),
    ENTRY("diagnostics.active", "Linked · diagnostics active"),
    ENTRY("diagnostics.ready", "Linked · diagnostics ready"),
    ENTRY("discover.file", "File"),
    ENTRY("discover.export_evidence", "Export evidence..."),
    ENTRY("discover.exit", "Exit"),
    ENTRY("discover.help", "Help"),
    ENTRY("discover.about", "About {product}..."),
    ENTRY("discover.vehicle_interface", "Vehicle interface"),
    ENTRY("discover.j2534_dll", "J2534 FunctionLibrary DLL"),
    ENTRY("discover.browse", "Browse..."),
    ENTRY("discover.connect_passive", "Connect passive 500 kbit/s"),
    ENTRY("discover.inventory", "Read-only OBD inventory"),
    ENTRY("discover.stop", "Stop"),
    ENTRY("discover.status_disconnected", "DISCONNECTED - deny-by-default safety policy active"),
    ENTRY("discover.session_log", "Diagnostic session log"),
    ENTRY("discover.annotation", "Evidence annotation"),
    ENTRY("discover.add_annotation", "Add annotation"),
    ENTRY("discover.no_dll", "No J2534 FunctionLibrary DLL is selected."),
    ENTRY("discover.evidence_failed", "Cannot create the evidence JSONL file."),
    ENTRY("discover.connect_first", "Connect to the OpenPort/J2534 device first."),
    ENTRY("discover.no_evidence", "No evidence has been recorded yet."),
    ENTRY("discover.about_line", "OpenPort 2.0 / SAE J2534 read-only discovery and evidence capture."),
    ENTRY("discover.about_safety", "Unsafe and unknown diagnostic services are denied before transmission.")
};

static const InfiltratrI18nEntry en_us[] = {
    ENTRY("nav.live_data.summary", "Search, select and favorite live diagnostic parameters")
};

static const InfiltratrI18nEntry de_de[] = {
    ENTRY("nav.vehicle", "Fahrzeug"),
    ENTRY("nav.vehicle.summary", "Fahrzeugidentität, Adapter- und Verbindungsinformationen"),
    ENTRY("nav.modules", "Steuergeräte"),
    ENTRY("nav.modules.summary", "Erkannte Steuergeräte und ECU-Identifikation"),
    ENTRY("nav.faults", "Fehler"),
    ENTRY("nav.faults.summary", "Diagnosefehlercodes nach Steuergerät"),
    ENTRY("nav.live_data", "Live-Daten"),
    ENTRY("nav.live_data.summary", "Live-Diagnoseparameter suchen, auswählen und favorisieren"),
    ENTRY("nav.table", "Tabelle"),
    ENTRY("nav.table.summary", "Kompakte Live-Werte ausgewählter Diagnoseparameter"),
    ENTRY("nav.dashboard", "Übersicht"),
    ENTRY("nav.dashboard.summary", "Live-Diagnosemesswerte auf einen Blick"),
    ENTRY("nav.graphs", "Diagramme"),
    ENTRY("nav.graphs.summary", "Zeitreihen ausgewählter Diagnoseparameter"),
    ENTRY("nav.log", "Protokoll"),
    ENTRY("nav.log.summary", "Diagnosesitzungsverlauf und exportierte Telemetrie"),
    ENTRY("nav.settings", "Einstellungen"),
    ENTRY("nav.settings.summary", "Anzeige, Adapter, Einheiten, Protokollierung und Anwendungseinstellungen"),
    ENTRY("common.about", "Info"),
    ENTRY("common.adapter", "Adapter"),
    ENTRY("common.refresh", "Aktualisieren"),
    ENTRY("language.label", "Sprache"),
    ENTRY("language.system", "Systemstandard"),
    ENTRY("connection.disconnected", "Getrennt"),
    ENTRY("connection.link_up", "VERBINDEN"),
    ENTRY("connection.link_down", "TRENNEN"),
    ENTRY("connection.no_device", "Kein serielles ELM327-Gerät erkannt"),
    ENTRY("connection.invalid_config", "Ungültige Adapterkonfiguration"),
    ENTRY("connection.open_failed", "Adapter kann nicht geöffnet werden · dialout-Berechtigungen prüfen"),
    ENTRY("connection.handshake_failed", "Gerät geöffnet, aber ELM327-Identitätsprüfung fehlgeschlagen"),
    ENTRY("connection.adapter_generic", "ELM327-kompatibler Adapter"),
    ENTRY("connection.linked_starting", "Verbunden · {identity} · Diagnose wird gestartet"),
    ENTRY("diagnostics.unavailable", "Diagnosestatus nicht verfügbar"),
    ENTRY("diagnostics.idle", "Verbunden · Diagnosesitzung inaktiv"),
    ENTRY("diagnostics.initialising", "Verbunden · ELM327-Adapter wird initialisiert"),
    ENTRY("diagnostics.discovering_pids", "Verbunden · unterstützte OBD-II-PIDs werden ermittelt"),
    ENTRY("diagnostics.manufacturer_pending", "Verbunden · Herstellerspezifische Erweiterung ausstehend"),
    ENTRY("diagnostics.restoring", "Verbunden · Standard-OBD-II-Kanal wird wiederhergestellt"),
    ENTRY("diagnostics.stored_dtcs", "Verbunden · gespeicherte OBD-II-Fehler werden gelesen"),
    ENTRY("diagnostics.pending_dtcs", "Verbunden · anstehende OBD-II-Fehler werden gelesen"),
    ENTRY("diagnostics.permanent_dtcs", "Verbunden · permanente OBD-II-Fehler werden gelesen"),
    ENTRY("diagnostics.live", "Verbunden · OBD-II-Live-Abfrage aktiv"),
    ENTRY("diagnostics.failed", "Verbunden · Diagnosesitzung fehlgeschlagen · trennen und erneut verbinden"),
    ENTRY("diagnostics.active", "Verbunden · Diagnose aktiv"),
    ENTRY("diagnostics.ready", "Verbunden · Diagnose bereit"),
    ENTRY("discover.file", "Datei"),
    ENTRY("discover.export_evidence", "Nachweise exportieren..."),
    ENTRY("discover.exit", "Beenden"),
    ENTRY("discover.help", "Hilfe"),
    ENTRY("discover.about", "Info zu {product}..."),
    ENTRY("discover.vehicle_interface", "Fahrzeugschnittstelle"),
    ENTRY("discover.j2534_dll", "J2534 FunctionLibrary-DLL"),
    ENTRY("discover.browse", "Durchsuchen..."),
    ENTRY("discover.connect_passive", "Passiv mit 500 kbit/s verbinden"),
    ENTRY("discover.inventory", "Schreibgeschützte OBD-Inventur"),
    ENTRY("discover.stop", "Stopp"),
    ENTRY("discover.status_disconnected", "GETRENNT - Sicherheitsrichtlinie verweigert standardmäßig"),
    ENTRY("discover.session_log", "Diagnosesitzungsprotokoll"),
    ENTRY("discover.annotation", "Nachweis-Anmerkung"),
    ENTRY("discover.add_annotation", "Anmerkung hinzufügen"),
    ENTRY("discover.no_dll", "Keine J2534 FunctionLibrary-DLL ausgewählt."),
    ENTRY("discover.evidence_failed", "Die JSONL-Nachweisdatei kann nicht erstellt werden."),
    ENTRY("discover.connect_first", "Zuerst mit dem OpenPort/J2534-Gerät verbinden."),
    ENTRY("discover.no_evidence", "Es wurden noch keine Nachweise aufgezeichnet."),
    ENTRY("discover.about_line", "Schreibgeschützte OpenPort-2.0-/SAE-J2534-Erkennung und Nachweiserfassung."),
    ENTRY("discover.about_safety", "Unsichere und unbekannte Diagnosedienste werden vor der Übertragung verweigert.")
};

static const InfiltratrI18nEntry fr_fr[] = {
    ENTRY("nav.vehicle", "Véhicule"), ENTRY("nav.vehicle.summary", "Identité du véhicule, adaptateur et connexion"),
    ENTRY("nav.modules", "Calculateurs"), ENTRY("nav.modules.summary", "Calculateurs détectés et identification ECU"),
    ENTRY("nav.faults", "Défauts"), ENTRY("nav.faults.summary", "Codes de défaut par calculateur"),
    ENTRY("nav.live_data", "Données en direct"), ENTRY("nav.live_data.summary", "Rechercher, sélectionner et mettre en favoris les paramètres de diagnostic"),
    ENTRY("nav.table", "Tableau"), ENTRY("nav.table.summary", "Valeurs en direct des paramètres sélectionnés"),
    ENTRY("nav.dashboard", "Tableau de bord"), ENTRY("nav.dashboard.summary", "Mesures de diagnostic en direct en un coup d'œil"),
    ENTRY("nav.graphs", "Graphiques"), ENTRY("nav.graphs.summary", "Courbes temporelles des paramètres sélectionnés"),
    ENTRY("nav.log", "Journal"), ENTRY("nav.log.summary", "Historique de session et télémétrie exportée"),
    ENTRY("nav.settings", "Réglages"), ENTRY("nav.settings.summary", "Affichage, adaptateur, unités, journalisation et préférences"),
    ENTRY("common.about", "À propos"), ENTRY("common.adapter", "Adaptateur"), ENTRY("common.refresh", "Actualiser"),
    ENTRY("language.label", "Langue"), ENTRY("language.system", "Langue du système"),
    ENTRY("connection.disconnected", "Déconnecté"), ENTRY("connection.link_up", "CONNECTER"), ENTRY("connection.link_down", "DÉCONNECTER"),
    ENTRY("connection.no_device", "Aucun périphérique série ELM327 détecté"), ENTRY("connection.invalid_config", "Configuration d'adaptateur invalide"),
    ENTRY("connection.open_failed", "Impossible d'ouvrir l'adaptateur · vérifiez les permissions dialout"),
    ENTRY("connection.handshake_failed", "Périphérique ouvert mais identification ELM327 échouée"),
    ENTRY("connection.adapter_generic", "Adaptateur compatible ELM327"), ENTRY("connection.linked_starting", "Connecté · {identity} · démarrage du diagnostic"),
    ENTRY("diagnostics.unavailable", "État du diagnostic indisponible"), ENTRY("diagnostics.idle", "Connecté · session de diagnostic inactive"),
    ENTRY("diagnostics.initialising", "Connecté · initialisation de l'adaptateur ELM327"), ENTRY("diagnostics.discovering_pids", "Connecté · détection des PID OBD-II pris en charge"),
    ENTRY("diagnostics.manufacturer_pending", "Connecté · extension constructeur en attente"), ENTRY("diagnostics.restoring", "Connecté · restauration du canal OBD-II standard"),
    ENTRY("diagnostics.stored_dtcs", "Connecté · lecture des défauts OBD-II mémorisés"), ENTRY("diagnostics.pending_dtcs", "Connecté · lecture des défauts OBD-II en attente"),
    ENTRY("diagnostics.permanent_dtcs", "Connecté · lecture des défauts OBD-II permanents"), ENTRY("diagnostics.live", "Connecté · interrogation OBD-II en direct active"),
    ENTRY("diagnostics.failed", "Connecté · session de diagnostic échouée · déconnectez puis reconnectez"), ENTRY("diagnostics.active", "Connecté · diagnostic actif"),
    ENTRY("diagnostics.ready", "Connecté · diagnostic prêt"),
    ENTRY("discover.file", "Fichier"), ENTRY("discover.export_evidence", "Exporter les preuves..."), ENTRY("discover.exit", "Quitter"), ENTRY("discover.help", "Aide"),
    ENTRY("discover.about", "À propos de {product}..."), ENTRY("discover.vehicle_interface", "Interface véhicule"), ENTRY("discover.j2534_dll", "DLL FunctionLibrary J2534"),
    ENTRY("discover.browse", "Parcourir..."), ENTRY("discover.connect_passive", "Connexion passive 500 kbit/s"), ENTRY("discover.inventory", "Inventaire OBD en lecture seule"),
    ENTRY("discover.stop", "Arrêter"), ENTRY("discover.status_disconnected", "DÉCONNECTÉ - politique de sécurité restrictive active"),
    ENTRY("discover.session_log", "Journal de session de diagnostic"), ENTRY("discover.annotation", "Annotation de preuve"), ENTRY("discover.add_annotation", "Ajouter une annotation"),
    ENTRY("discover.no_dll", "Aucune DLL FunctionLibrary J2534 n'est sélectionnée."), ENTRY("discover.evidence_failed", "Impossible de créer le fichier de preuves JSONL."),
    ENTRY("discover.connect_first", "Connectez d'abord le périphérique OpenPort/J2534."), ENTRY("discover.no_evidence", "Aucune preuve n'a encore été enregistrée."),
    ENTRY("discover.about_line", "Découverte OpenPort 2.0 / SAE J2534 en lecture seule et collecte de preuves."),
    ENTRY("discover.about_safety", "Les services de diagnostic dangereux ou inconnus sont refusés avant transmission.")
};

static const InfiltratrI18nEntry es_es[] = {
    ENTRY("nav.vehicle", "Vehículo"), ENTRY("nav.vehicle.summary", "Identidad del vehículo, adaptador e información de conexión"),
    ENTRY("nav.modules", "Módulos"), ENTRY("nav.modules.summary", "Módulos de control detectados e identificación de ECU"),
    ENTRY("nav.faults", "Fallos"), ENTRY("nav.faults.summary", "Códigos de avería por módulo de control"),
    ENTRY("nav.live_data", "Datos en vivo"), ENTRY("nav.live_data.summary", "Buscar, seleccionar y marcar parámetros de diagnóstico favoritos"),
    ENTRY("nav.table", "Tabla"), ENTRY("nav.table.summary", "Valores en vivo de los parámetros seleccionados"),
    ENTRY("nav.dashboard", "Panel"), ENTRY("nav.dashboard.summary", "Mediciones de diagnóstico en vivo de un vistazo"),
    ENTRY("nav.graphs", "Gráficos"), ENTRY("nav.graphs.summary", "Series temporales de los parámetros seleccionados"),
    ENTRY("nav.log", "Registro"), ENTRY("nav.log.summary", "Historial de sesión y telemetría exportada"),
    ENTRY("nav.settings", "Ajustes"), ENTRY("nav.settings.summary", "Pantalla, adaptador, unidades, registro y preferencias"),
    ENTRY("common.about", "Acerca de"), ENTRY("common.adapter", "Adaptador"), ENTRY("common.refresh", "Actualizar"),
    ENTRY("language.label", "Idioma"), ENTRY("language.system", "Predeterminado del sistema"),
    ENTRY("connection.disconnected", "Desconectado"), ENTRY("connection.link_up", "CONECTAR"), ENTRY("connection.link_down", "DESCONECTAR"),
    ENTRY("connection.no_device", "No se detectó ningún dispositivo serie ELM327"), ENTRY("connection.invalid_config", "Configuración de adaptador no válida"),
    ENTRY("connection.open_failed", "No se puede abrir el adaptador · compruebe los permisos de dialout"),
    ENTRY("connection.handshake_failed", "El dispositivo se abrió, pero falló la identificación ELM327"),
    ENTRY("connection.adapter_generic", "Adaptador compatible con ELM327"), ENTRY("connection.linked_starting", "Conectado · {identity} · iniciando diagnóstico"),
    ENTRY("diagnostics.unavailable", "Estado de diagnóstico no disponible"), ENTRY("diagnostics.idle", "Conectado · sesión de diagnóstico inactiva"),
    ENTRY("diagnostics.initialising", "Conectado · inicializando adaptador ELM327"), ENTRY("diagnostics.discovering_pids", "Conectado · detectando PID OBD-II compatibles"),
    ENTRY("diagnostics.manufacturer_pending", "Conectado · extensión del fabricante pendiente"), ENTRY("diagnostics.restoring", "Conectado · restaurando canal OBD-II estándar"),
    ENTRY("diagnostics.stored_dtcs", "Conectado · leyendo fallos OBD-II almacenados"), ENTRY("diagnostics.pending_dtcs", "Conectado · leyendo fallos OBD-II pendientes"),
    ENTRY("diagnostics.permanent_dtcs", "Conectado · leyendo fallos OBD-II permanentes"), ENTRY("diagnostics.live", "Conectado · lectura OBD-II en vivo activa"),
    ENTRY("diagnostics.failed", "Conectado · la sesión de diagnóstico falló · desconecte y vuelva a conectar"), ENTRY("diagnostics.active", "Conectado · diagnóstico activo"),
    ENTRY("diagnostics.ready", "Conectado · diagnóstico listo"),
    ENTRY("discover.file", "Archivo"), ENTRY("discover.export_evidence", "Exportar evidencia..."), ENTRY("discover.exit", "Salir"), ENTRY("discover.help", "Ayuda"),
    ENTRY("discover.about", "Acerca de {product}..."), ENTRY("discover.vehicle_interface", "Interfaz del vehículo"), ENTRY("discover.j2534_dll", "DLL FunctionLibrary J2534"),
    ENTRY("discover.browse", "Examinar..."), ENTRY("discover.connect_passive", "Conexión pasiva a 500 kbit/s"), ENTRY("discover.inventory", "Inventario OBD de solo lectura"),
    ENTRY("discover.stop", "Detener"), ENTRY("discover.status_disconnected", "DESCONECTADO - política de seguridad restrictiva activa"),
    ENTRY("discover.session_log", "Registro de sesión de diagnóstico"), ENTRY("discover.annotation", "Anotación de evidencia"), ENTRY("discover.add_annotation", "Añadir anotación"),
    ENTRY("discover.no_dll", "No se ha seleccionado ninguna DLL FunctionLibrary J2534."), ENTRY("discover.evidence_failed", "No se puede crear el archivo de evidencia JSONL."),
    ENTRY("discover.connect_first", "Conecte primero el dispositivo OpenPort/J2534."), ENTRY("discover.no_evidence", "Todavía no se ha registrado evidencia."),
    ENTRY("discover.about_line", "Descubrimiento OpenPort 2.0 / SAE J2534 de solo lectura y captura de evidencia."),
    ENTRY("discover.about_safety", "Los servicios de diagnóstico inseguros o desconocidos se bloquean antes de transmitirlos.")
};

static const InfiltratrI18nEntry it_it[] = {
    ENTRY("nav.vehicle", "Veicolo"), ENTRY("nav.vehicle.summary", "Identità del veicolo, adattatore e informazioni di connessione"),
    ENTRY("nav.modules", "Centraline"), ENTRY("nav.modules.summary", "Centraline rilevate e identificazione ECU"),
    ENTRY("nav.faults", "Guasti"), ENTRY("nav.faults.summary", "Codici guasto per centralina"),
    ENTRY("nav.live_data", "Dati in tempo reale"), ENTRY("nav.live_data.summary", "Cerca, seleziona e aggiungi ai preferiti i parametri diagnostici"),
    ENTRY("nav.table", "Tabella"), ENTRY("nav.table.summary", "Valori in tempo reale dei parametri selezionati"),
    ENTRY("nav.dashboard", "Cruscotto"), ENTRY("nav.dashboard.summary", "Misure diagnostiche in tempo reale a colpo d'occhio"),
    ENTRY("nav.graphs", "Grafici"), ENTRY("nav.graphs.summary", "Serie temporali dei parametri selezionati"),
    ENTRY("nav.log", "Registro"), ENTRY("nav.log.summary", "Cronologia sessione e telemetria esportata"),
    ENTRY("nav.settings", "Impostazioni"), ENTRY("nav.settings.summary", "Schermo, adattatore, unità, registrazione e preferenze"),
    ENTRY("common.about", "Informazioni"), ENTRY("common.adapter", "Adattatore"), ENTRY("common.refresh", "Aggiorna"),
    ENTRY("language.label", "Lingua"), ENTRY("language.system", "Predefinita di sistema"),
    ENTRY("connection.disconnected", "Disconnesso"), ENTRY("connection.link_up", "CONNETTI"), ENTRY("connection.link_down", "DISCONNETTI"),
    ENTRY("connection.no_device", "Nessun dispositivo seriale ELM327 rilevato"), ENTRY("connection.invalid_config", "Configurazione adattatore non valida"),
    ENTRY("connection.open_failed", "Impossibile aprire l'adattatore · verificare i permessi dialout"),
    ENTRY("connection.handshake_failed", "Dispositivo aperto ma identificazione ELM327 non riuscita"),
    ENTRY("connection.adapter_generic", "Adattatore compatibile ELM327"), ENTRY("connection.linked_starting", "Connesso · {identity} · avvio diagnostica"),
    ENTRY("diagnostics.unavailable", "Stato diagnostico non disponibile"), ENTRY("diagnostics.idle", "Connesso · sessione diagnostica inattiva"),
    ENTRY("diagnostics.initialising", "Connesso · inizializzazione adattatore ELM327"), ENTRY("diagnostics.discovering_pids", "Connesso · ricerca PID OBD-II supportati"),
    ENTRY("diagnostics.manufacturer_pending", "Connesso · estensione costruttore in attesa"), ENTRY("diagnostics.restoring", "Connesso · ripristino canale OBD-II standard"),
    ENTRY("diagnostics.stored_dtcs", "Connesso · lettura guasti OBD-II memorizzati"), ENTRY("diagnostics.pending_dtcs", "Connesso · lettura guasti OBD-II in attesa"),
    ENTRY("diagnostics.permanent_dtcs", "Connesso · lettura guasti OBD-II permanenti"), ENTRY("diagnostics.live", "Connesso · lettura OBD-II in tempo reale attiva"),
    ENTRY("diagnostics.failed", "Connesso · sessione diagnostica non riuscita · disconnettere e riconnettere"), ENTRY("diagnostics.active", "Connesso · diagnostica attiva"),
    ENTRY("diagnostics.ready", "Connesso · diagnostica pronta"),
    ENTRY("discover.file", "File"), ENTRY("discover.export_evidence", "Esporta prove..."), ENTRY("discover.exit", "Esci"), ENTRY("discover.help", "Aiuto"),
    ENTRY("discover.about", "Informazioni su {product}..."), ENTRY("discover.vehicle_interface", "Interfaccia veicolo"), ENTRY("discover.j2534_dll", "DLL FunctionLibrary J2534"),
    ENTRY("discover.browse", "Sfoglia..."), ENTRY("discover.connect_passive", "Connessione passiva 500 kbit/s"), ENTRY("discover.inventory", "Inventario OBD in sola lettura"),
    ENTRY("discover.stop", "Arresta"), ENTRY("discover.status_disconnected", "DISCONNESSO - politica di sicurezza restrittiva attiva"),
    ENTRY("discover.session_log", "Registro sessione diagnostica"), ENTRY("discover.annotation", "Annotazione prova"), ENTRY("discover.add_annotation", "Aggiungi annotazione"),
    ENTRY("discover.no_dll", "Nessuna DLL FunctionLibrary J2534 selezionata."), ENTRY("discover.evidence_failed", "Impossibile creare il file prove JSONL."),
    ENTRY("discover.connect_first", "Connettere prima il dispositivo OpenPort/J2534."), ENTRY("discover.no_evidence", "Non sono ancora state registrate prove."),
    ENTRY("discover.about_line", "Rilevamento OpenPort 2.0 / SAE J2534 in sola lettura e acquisizione prove."),
    ENTRY("discover.about_safety", "I servizi diagnostici non sicuri o sconosciuti vengono bloccati prima della trasmissione.")
};

static const InfiltratrI18nCatalog catalogs[] = {
    {"en-AU", en_au, COUNT(en_au)},
    {"en-US", en_us, COUNT(en_us)},
    {"de-DE", de_de, COUNT(de_de)},
    {"fr-FR", fr_fr, COUNT(fr_fr)},
    {"es-ES", es_es, COUNT(es_es)},
    {"it-IT", it_it, COUNT(it_it)}
};

static const char *const locale_names[] = {
    "English (Australia)",
    "English (United States)",
    "Deutsch",
    "Français",
    "Español",
    "Italiano"
};

static InfiltratrI18n context;
static bool initialised;

void link_i18n_init(void)
{
    if (initialised) return;
    initialised = infiltratr_i18n_init(&context, catalogs, COUNT(catalogs), "en-AU");
}

bool link_i18n_set_locale(const char *locale)
{
    link_i18n_init();
    if (!initialised) return false;
    return infiltratr_i18n_set_locale(&context, locale);
}

const char *link_i18n_locale(void)
{
    link_i18n_init();
    return initialised ? infiltratr_i18n_locale(&context) : "en-AU";
}

const char *link_i18n_tr(const char *key)
{
    link_i18n_init();
    return initialised ? infiltratr_i18n_get(&context, key) : (key != NULL ? key : "");
}

size_t link_i18n_format(char *destination, size_t capacity, const char *key,
                        const InfiltratrI18nArgument *arguments,
                        size_t argument_count)
{
    return infiltratr_i18n_format(destination, capacity, link_i18n_tr(key),
                                  arguments, argument_count);
}

size_t link_i18n_supported_locale_count(void)
{
    return COUNT(catalogs);
}

const char *link_i18n_supported_locale(size_t index)
{
    return index < COUNT(catalogs) ? catalogs[index].locale : NULL;
}

const char *link_i18n_supported_locale_name(size_t index)
{
    return index < COUNT(locale_names) ? locale_names[index] : NULL;
}

#undef COUNT
#undef ENTRY
