// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dtc_knowledge.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *code;
    const char *title;
    const char *category;
} LinkDtcEntry;

static const LinkDtcEntry standard_entries[] = {
    {"P0001", "Fuel Volume Regulator Control Circuit/Open", "Fuel delivery"},
    {"P0002", "Fuel Volume Regulator Control Circuit Range/Performance", "Fuel delivery"},
    {"P0003", "Fuel Volume Regulator Control Circuit Low", "Fuel delivery"},
    {"P0004", "Fuel Volume Regulator Control Circuit High", "Fuel delivery"},
    {"P0087", "Fuel Rail/System Pressure Too Low", "Fuel pressure"},
    {"P0088", "Fuel Rail/System Pressure Too High", "Fuel pressure"},
    {"P0089", "Fuel Pressure Regulator 1 Performance", "Fuel pressure"},
    {"P0090", "Fuel Pressure Regulator 1 Control Circuit/Open", "Fuel pressure"},
    {"P0091", "Fuel Pressure Regulator 1 Control Circuit Low", "Fuel pressure"},
    {"P0092", "Fuel Pressure Regulator 1 Control Circuit High", "Fuel pressure"},
    {"P0093", "Fuel System Leak Detected - Large Leak", "Fuel delivery"},
    {"P0094", "Fuel System Leak Detected - Small Leak", "Fuel delivery"},
    {"P0100", "Mass or Volume Air Flow A Circuit", "Air metering"},
    {"P0101", "Mass or Volume Air Flow A Circuit Range/Performance", "Air metering"},
    {"P0102", "Mass or Volume Air Flow A Circuit Low", "Air metering"},
    {"P0103", "Mass or Volume Air Flow A Circuit High", "Air metering"},
    {"P0104", "Mass or Volume Air Flow A Circuit Intermittent", "Air metering"},
    {"P0105", "Manifold Absolute Pressure/Barometric Pressure Circuit", "Air metering"},
    {"P0106", "Manifold Absolute Pressure/Barometric Pressure Circuit Range/Performance", "Air metering"},
    {"P0107", "Manifold Absolute Pressure/Barometric Pressure Circuit Low", "Air metering"},
    {"P0108", "Manifold Absolute Pressure/Barometric Pressure Circuit High", "Air metering"},
    {"P0109", "Manifold Absolute Pressure/Barometric Pressure Circuit Intermittent", "Air metering"},
    {"P0110", "Intake Air Temperature Sensor 1 Circuit", "Temperature sensing"},
    {"P0111", "Intake Air Temperature Sensor 1 Circuit Range/Performance", "Temperature sensing"},
    {"P0112", "Intake Air Temperature Sensor 1 Circuit Low", "Temperature sensing"},
    {"P0113", "Intake Air Temperature Sensor 1 Circuit High", "Temperature sensing"},
    {"P0114", "Intake Air Temperature Sensor 1 Circuit Intermittent", "Temperature sensing"},
    {"P0115", "Engine Coolant Temperature Sensor 1 Circuit", "Temperature sensing"},
    {"P0116", "Engine Coolant Temperature Sensor 1 Circuit Range/Performance", "Temperature sensing"},
    {"P0117", "Engine Coolant Temperature Sensor 1 Circuit Low", "Temperature sensing"},
    {"P0118", "Engine Coolant Temperature Sensor 1 Circuit High", "Temperature sensing"},
    {"P0119", "Engine Coolant Temperature Sensor 1 Circuit Intermittent", "Temperature sensing"},
    {"P0190", "Fuel Rail Pressure Sensor A Circuit", "Fuel pressure"},
    {"P0191", "Fuel Rail Pressure Sensor A Circuit Range/Performance", "Fuel pressure"},
    {"P0192", "Fuel Rail Pressure Sensor A Circuit Low", "Fuel pressure"},
    {"P0193", "Fuel Rail Pressure Sensor A Circuit High", "Fuel pressure"},
    {"P0194", "Fuel Rail Pressure Sensor A Circuit Intermittent", "Fuel pressure"},
    {"P0200", "Injector Circuit/Open", "Fuel injection"},
    {"P0234", "Turbocharger/Supercharger A Overboost Condition", "Boost control"},
    {"P0299", "Turbocharger/Supercharger A Underboost Condition", "Boost control"},
    {"P0300", "Random/Multiple Cylinder Misfire Detected", "Combustion/misfire"},
    {"P0335", "Crankshaft Position Sensor A Circuit", "Engine position sensing"},
    {"P0336", "Crankshaft Position Sensor A Circuit Range/Performance", "Engine position sensing"},
    {"P0337", "Crankshaft Position Sensor A Circuit Low", "Engine position sensing"},
    {"P0338", "Crankshaft Position Sensor A Circuit High", "Engine position sensing"},
    {"P0339", "Crankshaft Position Sensor A Circuit Intermittent", "Engine position sensing"},
    {"P0340", "Camshaft Position Sensor A Circuit - Bank 1 or Single Sensor", "Engine position sensing"},
    {"P0341", "Camshaft Position Sensor A Circuit Range/Performance - Bank 1 or Single Sensor", "Engine position sensing"},
    {"P0342", "Camshaft Position Sensor A Circuit Low - Bank 1 or Single Sensor", "Engine position sensing"},
    {"P0343", "Camshaft Position Sensor A Circuit High - Bank 1 or Single Sensor", "Engine position sensing"},
    {"P0344", "Camshaft Position Sensor A Circuit Intermittent - Bank 1 or Single Sensor", "Engine position sensing"},
    {"P0400", "Exhaust Gas Recirculation Flow Malfunction", "EGR/emissions"},
    {"P0401", "Exhaust Gas Recirculation Flow Insufficient Detected", "EGR/emissions"},
    {"P0402", "Exhaust Gas Recirculation Flow Excessive Detected", "EGR/emissions"},
    {"P0403", "Exhaust Gas Recirculation Control Circuit", "EGR/emissions"},
    {"P0404", "Exhaust Gas Recirculation Control Circuit Range/Performance", "EGR/emissions"},
    {"P0405", "Exhaust Gas Recirculation Sensor A Circuit Low", "EGR/emissions"},
    {"P0406", "Exhaust Gas Recirculation Sensor A Circuit High", "EGR/emissions"},
    {"P0407", "Exhaust Gas Recirculation Sensor B Circuit Low", "EGR/emissions"},
    {"P0408", "Exhaust Gas Recirculation Sensor B Circuit High", "EGR/emissions"},
    {"P0409", "Exhaust Gas Recirculation Sensor A Circuit", "EGR/emissions"},
    {"P0420", "Catalyst System Efficiency Below Threshold - Bank 1", "Catalyst/aftertreatment"},
    {"P0430", "Catalyst System Efficiency Below Threshold - Bank 2", "Catalyst/aftertreatment"},
    {"P0470", "Exhaust Pressure Sensor A Circuit", "Exhaust pressure"},
    {"P0471", "Exhaust Pressure Sensor A Circuit Range/Performance", "Exhaust pressure"},
    {"P0472", "Exhaust Pressure Sensor A Circuit Low", "Exhaust pressure"},
    {"P0473", "Exhaust Pressure Sensor A Circuit High", "Exhaust pressure"},
    {"P0474", "Exhaust Pressure Sensor A Circuit Intermittent", "Exhaust pressure"},
    {"P0480", "Cooling Fan 1 Control Circuit", "Cooling/electrical"},
    {"P0481", "Cooling Fan 2 Control Circuit", "Cooling/electrical"},
    {"P0500", "Vehicle Speed Sensor A", "Vehicle speed sensing"},
    {"P0544", "Exhaust Gas Temperature Sensor Circuit - Bank 1 Sensor 1", "Exhaust temperature"},
    {"P0545", "Exhaust Gas Temperature Sensor Circuit Low - Bank 1 Sensor 1", "Exhaust temperature"},
    {"P0546", "Exhaust Gas Temperature Sensor Circuit High - Bank 1 Sensor 1", "Exhaust temperature"},
    {"P0560", "System Voltage", "Electrical supply"},
    {"P0562", "System Voltage Low", "Electrical supply"},
    {"P0563", "System Voltage High", "Electrical supply"},
    {"P0600", "Serial Communication Link", "Control module"},
    {"P0601", "Internal Control Module Memory Check Sum Error", "Control module"},
    {"P0602", "Control Module Programming Error", "Control module"},
    {"P0603", "Internal Control Module Keep Alive Memory Error", "Control module"},
    {"P0604", "Internal Control Module Random Access Memory Error", "Control module"},
    {"P0605", "Internal Control Module Read Only Memory Error", "Control module"},
    {"P0606", "PCM/ECM Processor Fault", "Control module"},
    {"P0627", "Fuel Pump A Control Circuit/Open", "Fuel delivery"},
    {"P0628", "Fuel Pump A Control Circuit Low", "Fuel delivery"},
    {"P0629", "Fuel Pump A Control Circuit High", "Fuel delivery"},
    {"P0670", "Glow Plug Control Module Control Circuit/Open", "Glow plug/preheat"},
    {"P2002", "Diesel Particulate Filter Efficiency Below Threshold - Bank 1", "DPF/aftertreatment"},
    {"P2003", "Diesel Particulate Filter Efficiency Below Threshold - Bank 2", "DPF/aftertreatment"},
    {"P2031", "Exhaust Gas Temperature Sensor Circuit - Bank 1 Sensor 2", "Exhaust temperature"},
    {"P2032", "Exhaust Gas Temperature Sensor Circuit Low - Bank 1 Sensor 2", "Exhaust temperature"},
    {"P2033", "Exhaust Gas Temperature Sensor Circuit High - Bank 1 Sensor 2", "Exhaust temperature"},
    {"P2200", "NOx Sensor Circuit - Bank 1", "NOx/aftertreatment"},
    {"P2201", "NOx Sensor Circuit Range/Performance - Bank 1", "NOx/aftertreatment"},
    {"P2202", "NOx Sensor Circuit Low - Bank 1", "NOx/aftertreatment"},
    {"P2203", "NOx Sensor Circuit High - Bank 1", "NOx/aftertreatment"},
    {"P2204", "NOx Sensor Circuit Intermittent - Bank 1", "NOx/aftertreatment"},
    {"P2291", "Injector Control Pressure Too Low - Engine Cranking", "Fuel injection"},
    {"P2293", "Fuel Pressure Regulator 2 Performance", "Fuel pressure"},
    {"P2294", "Fuel Pressure Regulator 2 Control Circuit/Open", "Fuel pressure"},
    {"P2295", "Fuel Pressure Regulator 2 Control Circuit Low", "Fuel pressure"},
    {"P2296", "Fuel Pressure Regulator 2 Control Circuit High", "Fuel pressure"},
    {"P242A", "Exhaust Gas Temperature Sensor Circuit - Bank 1 Sensor 3", "Exhaust temperature"},
    {"P242B", "Exhaust Gas Temperature Sensor Circuit Range/Performance - Bank 1 Sensor 3", "Exhaust temperature"},
    {"P242C", "Exhaust Gas Temperature Sensor Circuit Low - Bank 1 Sensor 3", "Exhaust temperature"},
    {"P242D", "Exhaust Gas Temperature Sensor Circuit High - Bank 1 Sensor 3", "Exhaust temperature"},
    {"P242E", "Exhaust Gas Temperature Sensor Circuit Intermittent/Erratic - Bank 1 Sensor 3", "Exhaust temperature"},
    {"P242F", "Diesel Particulate Filter Restriction - Ash Accumulation", "DPF/aftertreatment"},
    {"P2452", "Diesel Particulate Filter Pressure Sensor A Circuit", "DPF/aftertreatment"},
    {"P2453", "Diesel Particulate Filter Pressure Sensor A Circuit Range/Performance", "DPF/aftertreatment"},
    {"P2454", "Diesel Particulate Filter Pressure Sensor A Circuit Low", "DPF/aftertreatment"},
    {"P2455", "Diesel Particulate Filter Pressure Sensor A Circuit High", "DPF/aftertreatment"},
    {"P2456", "Diesel Particulate Filter Pressure Sensor A Circuit Intermittent/Erratic", "DPF/aftertreatment"},
    {"P2458", "Diesel Particulate Filter Regeneration Duration", "DPF/aftertreatment"},
    {"P2459", "Diesel Particulate Filter Regeneration Frequency", "DPF/aftertreatment"},
    {"P2463", "Diesel Particulate Filter Restriction - Soot Accumulation", "DPF/aftertreatment"},
    {"U0001", "High Speed CAN Communication Bus", "Vehicle network"},
    {"U0100", "Lost Communication With ECM/PCM A", "Vehicle network"},
    {"U0101", "Lost Communication With TCM", "Vehicle network"},
    {"U0121", "Lost Communication With Anti-Lock Brake System Control Module", "Vehicle network"},
    {"U0140", "Lost Communication With Body Control Module", "Vehicle network"},
    {"U0155", "Lost Communication With Instrument Panel Cluster Control Module", "Vehicle network"},
    {"U0401", "Invalid Data Received From ECM/PCM A", "Vehicle network"}
};

static bool valid_hex(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'F');
}

static LinkDtcSystem system_from_code(char value)
{
    switch (value) {
    case 'P': return LINK_DTC_SYSTEM_POWERTRAIN;
    case 'C': return LINK_DTC_SYSTEM_CHASSIS;
    case 'B': return LINK_DTC_SYSTEM_BODY;
    case 'U': return LINK_DTC_SYSTEM_NETWORK;
    default: return LINK_DTC_SYSTEM_UNKNOWN;
    }
}

static LinkDtcOrigin origin_from_code(const char code[LINK_DTC_CODE_LENGTH])
{
    if (code[0] == 'P') {
        if (code[1] == '0' || code[1] == '2') return LINK_DTC_ORIGIN_STANDARD_GENERIC;
        if (code[1] == '1') return LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
        if (code[1] == '3') {
            return code[2] >= '4' ? LINK_DTC_ORIGIN_STANDARD_GENERIC
                                  : LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
        }
    } else {
        if (code[1] == '0' || code[1] == '3') return LINK_DTC_ORIGIN_STANDARD_GENERIC;
        if (code[1] == '1' || code[1] == '2') return LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC;
    }
    return LINK_DTC_ORIGIN_UNKNOWN;
}

static void copy_text(char *destination, size_t size, const char *source)
{
    if (destination == NULL || size == 0U) return;
    if (source == NULL) source = "";
    (void)snprintf(destination, size, "%s", source);
}

static bool resolve_pattern(const char code[LINK_DTC_CODE_LENGTH], LinkDtcKnowledge *knowledge)
{
    unsigned int numeric;
    unsigned int cylinder;
    unsigned int offset;

    if (sscanf(code + 1, "%4x", &numeric) != 1) return false;

    if (numeric >= 0x0201U && numeric <= 0x020cU) {
        cylinder = numeric - 0x0200U;
        (void)snprintf(knowledge->title, sizeof(knowledge->title),
                       "Injector Circuit/Open - Cylinder %u", cylinder);
        copy_text(knowledge->category, sizeof(knowledge->category), "Fuel injection");
        return true;
    }
    if (numeric >= 0x0261U && numeric <= 0x0296U) {
        offset = numeric - 0x0261U;
        cylinder = offset / 3U + 1U;
        switch (offset % 3U) {
        case 0U:
            (void)snprintf(knowledge->title, sizeof(knowledge->title),
                           "Cylinder %u Injector Circuit Low", cylinder);
            break;
        case 1U:
            (void)snprintf(knowledge->title, sizeof(knowledge->title),
                           "Cylinder %u Injector Circuit High", cylinder);
            break;
        default:
            (void)snprintf(knowledge->title, sizeof(knowledge->title),
                           "Cylinder %u Contribution/Balance", cylinder);
            break;
        }
        copy_text(knowledge->category, sizeof(knowledge->category), "Fuel injection");
        return true;
    }
    if (numeric >= 0x0301U && numeric <= 0x030cU) {
        cylinder = numeric - 0x0300U;
        (void)snprintf(knowledge->title, sizeof(knowledge->title),
                       "Cylinder %u Misfire Detected", cylinder);
        copy_text(knowledge->category, sizeof(knowledge->category), "Combustion/misfire");
        return true;
    }
    if (numeric >= 0x0671U && numeric <= 0x0682U) {
        cylinder = numeric - 0x0670U;
        (void)snprintf(knowledge->title, sizeof(knowledge->title),
                       "Cylinder %u Glow Plug Circuit/Open", cylinder);
        copy_text(knowledge->category, sizeof(knowledge->category), "Glow plug/preheat");
        return true;
    }
    return false;
}

bool link_dtc_resolve(const char *code, LinkDtcKnowledge *knowledge)
{
    LinkDtcKnowledge resolved = {0};
    size_t index;

    if (code == NULL || knowledge == NULL || strlen(code) != 5U) return false;
    for (index = 0U; index < 5U; ++index) {
        resolved.code[index] = (char)toupper((unsigned char)code[index]);
    }
    resolved.code[5] = '\0';
    if (system_from_code(resolved.code[0]) == LINK_DTC_SYSTEM_UNKNOWN ||
        resolved.code[1] < '0' || resolved.code[1] > '3' ||
        !valid_hex(resolved.code[2]) || !valid_hex(resolved.code[3]) ||
        !valid_hex(resolved.code[4])) {
        return false;
    }

    resolved.system = system_from_code(resolved.code[0]);
    resolved.origin = origin_from_code(resolved.code);
    resolved.source = LINK_DTC_SOURCE_UNKNOWN;

    for (index = 0U; index < sizeof(standard_entries) / sizeof(standard_entries[0]); ++index) {
        if (strcmp(resolved.code, standard_entries[index].code) == 0) {
            resolved.definition_known = true;
            resolved.origin = LINK_DTC_ORIGIN_STANDARD_GENERIC;
            resolved.source = LINK_DTC_SOURCE_STANDARD_GENERIC;
            copy_text(resolved.title, sizeof(resolved.title), standard_entries[index].title);
            copy_text(resolved.category, sizeof(resolved.category), standard_entries[index].category);
            *knowledge = resolved;
            return true;
        }
    }

    if (resolved.system == LINK_DTC_SYSTEM_POWERTRAIN && resolve_pattern(resolved.code, &resolved)) {
        resolved.definition_known = true;
        resolved.origin = LINK_DTC_ORIGIN_STANDARD_GENERIC;
        resolved.source = LINK_DTC_SOURCE_STANDARD_GENERIC;
    }
    *knowledge = resolved;
    return true;
}

const char *link_dtc_system_name(LinkDtcSystem system)
{
    switch (system) {
    case LINK_DTC_SYSTEM_POWERTRAIN: return "Powertrain";
    case LINK_DTC_SYSTEM_CHASSIS: return "Chassis";
    case LINK_DTC_SYSTEM_BODY: return "Body";
    case LINK_DTC_SYSTEM_NETWORK: return "Network";
    case LINK_DTC_SYSTEM_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}

const char *link_dtc_origin_name(LinkDtcOrigin origin)
{
    switch (origin) {
    case LINK_DTC_ORIGIN_STANDARD_GENERIC: return "Standard generic";
    case LINK_DTC_ORIGIN_MANUFACTURER_SPECIFIC: return "Manufacturer-specific";
    case LINK_DTC_ORIGIN_UNKNOWN: return "Unknown";
    }
    return "Unknown";
}

const char *link_dtc_source_name(LinkDtcSource source)
{
    switch (source) {
    case LINK_DTC_SOURCE_STANDARD_GENERIC: return "SAE/ISO generic definition";
    case LINK_DTC_SOURCE_UNKNOWN: return "Unmapped";
    }
    return "Unmapped";
}

bool link_dtc_format_uds_status(uint8_t status, char *buffer, size_t buffer_size)
{
    static const struct {
        uint8_t mask;
        const char *text;
    } bits[] = {
        {0x01U, "Test failed"},
        {0x02U, "Failed this operation cycle"},
        {0x04U, "Pending"},
        {0x08U, "Confirmed"},
        {0x10U, "Test not completed since last clear"},
        {0x20U, "Failed since last clear"},
        {0x40U, "Test not completed this operation cycle"},
        {0x80U, "Warning indicator requested"}
    };
    size_t index;
    size_t used = 0U;

    if (buffer == NULL || buffer_size == 0U) return false;
    buffer[0] = '\0';
    if (status == 0U) {
        return snprintf(buffer, buffer_size, "No status bits set") > 0 &&
               strlen(buffer) < buffer_size;
    }

    for (index = 0U; index < sizeof(bits) / sizeof(bits[0]); ++index) {
        int written;
        if ((status & bits[index].mask) == 0U) continue;
        written = snprintf(buffer + used, buffer_size - used, "%s%s",
                           used == 0U ? "" : " · ", bits[index].text);
        if (written < 0 || (size_t)written >= buffer_size - used) {
            buffer[0] = '\0';
            return false;
        }
        used += (size_t)written;
    }
    return true;
}
