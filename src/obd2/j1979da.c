// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file j1979da.c
 * @brief Publicly verified J1979/J1979-DA Mode 05/06 semantics.
 *
 * Registry facts are independently represented rather than copying SAE prose.
 * The baseline was cross-checked against the legally incorporated J1979:2002
 * message format, public J1979DA_201110 tables, later public OEM Mode-06
 * material, and the MIT-licensed shinyorg/obd scaling implementation.
 */
#include "link/j1979da.h"

#include <string.h>

static const LinkJ1979Mode05TidDefinition mode05_tids[] = {
    {0x01U, "Rich-to-lean sensor threshold voltage", 0.005, 0.0, "V", true},
    {0x02U, "Lean-to-rich sensor threshold voltage", 0.005, 0.0, "V", true},
    {0x03U, "Low sensor voltage for switch-time calculation", 0.005, 0.0, "V", true},
    {0x04U, "High sensor voltage for switch-time calculation", 0.005, 0.0, "V", true},
    {0x05U, "Rich-to-lean sensor switch time", 0.004, 0.0, "s", false},
    {0x06U, "Lean-to-rich sensor switch time", 0.004, 0.0, "s", false},
    {0x07U, "Minimum sensor voltage", 0.005, 0.0, "V", false},
    {0x08U, "Maximum sensor voltage", 0.005, 0.0, "V", false},
    {0x09U, "Time between sensor transitions", 0.04, 0.0, "s", false},
    {0x0AU, "Sensor period", 0.04, 0.0, "s", false}
};

static const LinkJ1979Mode06TidDefinition mode06_tids[] = {
    {0x01U, "Rich-to-lean sensor threshold voltage", 0x0AU},
    {0x02U, "Lean-to-rich sensor threshold voltage", 0x0AU},
    {0x03U, "Low sensor voltage for switch-time calculation", 0x0AU},
    {0x04U, "High sensor voltage for switch-time calculation", 0x0AU},
    {0x05U, "Rich-to-lean sensor switch time", 0x10U},
    {0x06U, "Lean-to-rich sensor switch time", 0x10U},
    {0x07U, "Minimum sensor voltage", 0x0AU},
    {0x08U, "Maximum sensor voltage", 0x0AU},
    {0x09U, "Time between sensor transitions", 0x10U},
    {0x0AU, "Sensor period", 0x10U},
    {0x0BU, "EWMA misfire count", 0x24U},
    {0x0CU, "Current/last-cycle misfire count", 0x24U}
};

static const LinkJ1979Mode06MonitorDefinition mode06_monitors[] = {
    {0x01U, "Exhaust gas sensor monitor B1S1"},
    {0x02U, "Exhaust gas sensor monitor B1S2"},
    {0x03U, "Exhaust gas sensor monitor B1S3"},
    {0x04U, "Exhaust gas sensor monitor B1S4"},
    {0x05U, "Exhaust gas sensor monitor B2S1"},
    {0x06U, "Exhaust gas sensor monitor B2S2"},
    {0x07U, "Exhaust gas sensor monitor B2S3"},
    {0x08U, "Exhaust gas sensor monitor B2S4"},
    {0x09U, "Exhaust gas sensor monitor B3S1"},
    {0x0AU, "Exhaust gas sensor monitor B3S2"},
    {0x0BU, "Exhaust gas sensor monitor B3S3"},
    {0x0CU, "Exhaust gas sensor monitor B3S4"},
    {0x0DU, "Exhaust gas sensor monitor B4S1"},
    {0x0EU, "Exhaust gas sensor monitor B4S2"},
    {0x0FU, "Exhaust gas sensor monitor B4S3"},
    {0x10U, "Exhaust gas sensor monitor B4S4"},
    {0x21U, "Catalyst monitor bank 1"},
    {0x22U, "Catalyst monitor bank 2"},
    {0x23U, "Catalyst monitor bank 3"},
    {0x24U, "Catalyst monitor bank 4"},
    {0x31U, "EGR monitor bank 1"},
    {0x32U, "EGR monitor bank 2"},
    {0x33U, "EGR monitor bank 3"},
    {0x34U, "EGR monitor bank 4"},
    {0x35U, "VVT monitor bank 1"},
    {0x36U, "VVT monitor bank 2"},
    {0x37U, "VVT monitor bank 3"},
    {0x38U, "VVT monitor bank 4"},
    {0x39U, "EVAP monitor cap-off / 0.150 inch"},
    {0x3AU, "EVAP monitor 0.090 inch"},
    {0x3BU, "EVAP monitor 0.040 inch"},
    {0x3CU, "EVAP monitor 0.020 inch"},
    {0x3DU, "Purge-flow monitor"},
    {0x41U, "Exhaust gas sensor heater monitor B1S1"},
    {0x42U, "Exhaust gas sensor heater monitor B1S2"},
    {0x43U, "Exhaust gas sensor heater monitor B1S3"},
    {0x44U, "Exhaust gas sensor heater monitor B1S4"},
    {0x45U, "Exhaust gas sensor heater monitor B2S1"},
    {0x46U, "Exhaust gas sensor heater monitor B2S2"},
    {0x47U, "Exhaust gas sensor heater monitor B2S3"},
    {0x48U, "Exhaust gas sensor heater monitor B2S4"},
    {0x49U, "Exhaust gas sensor heater monitor B3S1"},
    {0x4AU, "Exhaust gas sensor heater monitor B3S2"},
    {0x4BU, "Exhaust gas sensor heater monitor B3S3"},
    {0x4CU, "Exhaust gas sensor heater monitor B3S4"},
    {0x4DU, "Exhaust gas sensor heater monitor B4S1"},
    {0x4EU, "Exhaust gas sensor heater monitor B4S2"},
    {0x4FU, "Exhaust gas sensor heater monitor B4S3"},
    {0x50U, "Exhaust gas sensor heater monitor B4S4"},
    {0x61U, "Heated catalyst monitor bank 1"},
    {0x62U, "Heated catalyst monitor bank 2"},
    {0x63U, "Heated catalyst monitor bank 3"},
    {0x64U, "Heated catalyst monitor bank 4"},
    {0x71U, "Secondary-air monitor 1"},
    {0x72U, "Secondary-air monitor 2"},
    {0x73U, "Secondary-air monitor 3"},
    {0x74U, "Secondary-air monitor 4"},
    {0x81U, "Fuel-system monitor bank 1"},
    {0x82U, "Fuel-system monitor bank 2"},
    {0x83U, "Fuel-system monitor bank 3"},
    {0x84U, "Fuel-system monitor bank 4"},
    {0x85U, "Boost-pressure-control monitor bank 1"},
    {0x86U, "Boost-pressure-control monitor bank 2"},
    {0x90U, "NOx adsorber monitor bank 1"},
    {0x91U, "NOx adsorber monitor bank 2"},
    {0x98U, "NOx/SCR catalyst monitor bank 1"},
    {0x99U, "NOx/SCR catalyst monitor bank 2"},
    {0xA1U, "Misfire monitor general"},
    {0xA2U, "Misfire monitor cylinder 1"},
    {0xA3U, "Misfire monitor cylinder 2"},
    {0xA4U, "Misfire monitor cylinder 3"},
    {0xA5U, "Misfire monitor cylinder 4"},
    {0xA6U, "Misfire monitor cylinder 5"},
    {0xA7U, "Misfire monitor cylinder 6"},
    {0xA8U, "Misfire monitor cylinder 7"},
    {0xA9U, "Misfire monitor cylinder 8"},
    {0xAAU, "Misfire monitor cylinder 9"},
    {0xABU, "Misfire monitor cylinder 10"},
    {0xACU, "Misfire monitor cylinder 11"},
    {0xADU, "Misfire monitor cylinder 12"},
    {0xAEU, "Misfire monitor cylinder 13"},
    {0xAFU, "Misfire monitor cylinder 14"},
    {0xB0U, "Misfire monitor cylinder 15"},
    {0xB1U, "Misfire monitor cylinder 16"},
    {0xB2U, "Particulate-matter filter monitor bank 1"},
    {0xB3U, "Particulate-matter filter monitor bank 2"}
};

static const LinkJ1979UnitScaling mode06_uasids[] = {
    {0x01U,1.0,0.0,"count",false},{0x02U,0.1,0.0,"count",false},
    {0x03U,0.01,0.0,"count",false},{0x04U,0.001,0.0,"count",false},
    {0x05U,0.0000305,0.0,"count",false},{0x06U,0.000305,0.0,"count",false},
    {0x07U,0.25,0.0,"rpm",false},{0x08U,0.01,0.0,"km/h",false},
    {0x09U,1.0,0.0,"km/h",false},{0x0AU,0.122,0.0,"mV",false},
    {0x0BU,0.001,0.0,"V",false},{0x0CU,0.01,0.0,"V",false},
    {0x0DU,0.00390625,0.0,"mA",false},{0x0EU,0.001,0.0,"A",false},
    {0x0FU,0.01,0.0,"A",false},{0x10U,1.0,0.0,"ms",false},
    {0x11U,100.0,0.0,"ms",false},{0x12U,1.0,0.0,"s",false},
    {0x13U,1.0,0.0,"mOhm",false},{0x14U,1.0,0.0,"Ohm",false},
    {0x15U,1.0,0.0,"kOhm",false},{0x16U,0.1,-40.0,"degC",false},
    {0x17U,0.01,0.0,"kPa",false},{0x18U,0.0117,0.0,"kPa",false},
    {0x19U,0.079,0.0,"kPa",false},{0x1AU,1.0,0.0,"kPa",false},
    {0x1BU,10.0,0.0,"kPa",false},{0x1CU,0.01,0.0,"deg",false},
    {0x1DU,0.5,0.0,"deg",false},{0x1EU,0.0000305,0.0,"ratio",false},
    {0x1FU,0.05,0.0,"ratio",false},{0x20U,0.00390625,0.0,"ratio",false},
    {0x21U,1.0,0.0,"mHz",false},{0x22U,1.0,0.0,"Hz",false},
    {0x23U,1.0,0.0,"kHz",false},{0x24U,1.0,0.0,"count",false},
    {0x25U,1.0,0.0,"km",false},{0x26U,0.1,0.0,"mV/ms",false},
    {0x27U,0.01,0.0,"g/s",false},{0x28U,1.0,0.0,"g/s",false},
    {0x29U,0.25,0.0,"Pa/s",false},{0x2AU,0.001,0.0,"kg/h",false},
    {0x2BU,1.0,0.0,"count",false},{0x2CU,0.01,0.0,"g",false},
    {0x2DU,0.01,0.0,"mg",false},{0x2EU,1.0,0.0,"boolean",false},
    {0x2FU,0.01,0.0,"%",false},{0x30U,0.001526,0.0,"%",false},
    {0x31U,0.001,0.0,"L",false},{0x32U,0.0000305,0.0,"in",false},
    {0x33U,0.00024414,0.0,"ratio",false},{0x34U,1.0,0.0,"min",false},
    {0x35U,10.0,0.0,"ms",false},{0x36U,0.01,0.0,"g",false},
    {0x37U,0.1,0.0,"g",false},{0x38U,1.0,0.0,"g",false},
    {0x39U,0.01,-327.68,"%",false},{0x3AU,0.001,0.0,"g",false},
    {0x3BU,0.0001,0.0,"g",false},{0x3CU,0.1,0.0,"us",false},
    {0x3DU,0.01,0.0,"mA",false},{0x3EU,0.00006103516,0.0,"mm2",false},
    {0x3FU,0.01,0.0,"L",false},{0x40U,1.0,0.0,"ppm",false},
    {0x41U,0.01,0.0,"uA",false},
    {0x81U,1.0,0.0,"count",true},{0x82U,0.1,0.0,"count",true},
    {0x83U,0.01,0.0,"count",true},{0x84U,0.001,0.0,"count",true},
    {0x85U,0.0000305,0.0,"count",true},{0x86U,0.000305,0.0,"count",true},
    {0x87U,1.0,0.0,"ppm",true},{0x8AU,0.122,0.0,"mV",true},
    {0x8BU,0.001,0.0,"V",true},{0x8CU,0.01,0.0,"V",true},
    {0x8DU,0.00390625,0.0,"mA",true},{0x8EU,0.001,0.0,"A",true},
    {0x90U,1.0,0.0,"ms",true},{0x96U,0.1,0.0,"degC",true},
    {0x99U,0.1,0.0,"kPa",true},{0x9CU,0.01,0.0,"deg",true},
    {0x9DU,0.5,0.0,"deg",true},{0xA8U,1.0,0.0,"g/s",true},
    {0xA9U,0.25,0.0,"Pa/s",true},{0xADU,0.01,0.0,"mg",true},
    {0xAEU,0.1,0.0,"mg",true},{0xAFU,0.01,0.0,"%",true},
    {0xB0U,0.003052,0.0,"%",true},{0xB1U,2.0,0.0,"mV/s",true},
    {0xFCU,0.01,0.0,"kPa",true},{0xFDU,0.001,0.0,"kPa",true},
    {0xFEU,0.25,0.0,"Pa",true}
};

static size_t count05(void) { return sizeof(mode05_tids)/sizeof(mode05_tids[0]); }
static size_t count06t(void) { return sizeof(mode06_tids)/sizeof(mode06_tids[0]); }
static size_t countmid(void) { return sizeof(mode06_monitors)/sizeof(mode06_monitors[0]); }
static size_t countuas(void) { return sizeof(mode06_uasids)/sizeof(mode06_uasids[0]); }

static uint16_t read_u16_be(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

static char hex_digit(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";
    return digits[value & UINT8_C(0x0F)];
}

const char *link_j1979_revision(void) { return "J1979_202505"; }
const char *link_j1979da_revision(void) { return "J1979DA_202607"; }
const char *link_j1978_1_revision(void) { return "J1978-1_202604"; }
const char *link_j1979_2_revision(void) { return "J1979-2_202604"; }
const char *link_j1979da_public_semantics_revision(void)
{
    return "J1979DA_201110+verified-public-updates";
}

LinkJ1979IdentifierClass link_j1979_mode05_tid_classification(uint8_t tid)
{
    if ((tid & UINT8_C(0x1F)) == 0U)
        return LINK_J1979_IDENTIFIER_SUPPORT_BITMAP;
    if (tid >= UINT8_C(0x01) && tid <= UINT8_C(0x0A))
        return LINK_J1979_IDENTIFIER_STANDARD;
    if ((tid >= UINT8_C(0x21) && tid <= UINT8_C(0x7F)) ||
        (tid >= UINT8_C(0x81) && tid <= UINT8_C(0xFE)))
        return LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED;
    return LINK_J1979_IDENTIFIER_RESERVED;
}

const LinkJ1979Mode05TidDefinition *link_j1979_mode05_tid_definition(uint8_t tid)
{
    size_t i;
    for (i=0U;i<count05();++i) if (mode05_tids[i].tid==tid) return &mode05_tids[i];
    return NULL;
}

static bool mode05_scaling(uint8_t tid,double *scale,double *offset,const char **unit)
{
    const LinkJ1979Mode05TidDefinition *d=link_j1979_mode05_tid_definition(tid);
    if (d!=NULL) { *scale=d->scale; *offset=d->offset; *unit=d->unit; return true; }
    *offset=0.0;
    if (tid>=0x21U && tid<=0x2FU) { *scale=0.004; *unit="s"; return true; }
    if (tid>=0x30U && tid<=0x3FU) { *scale=0.04; *unit="s"; return true; }
    if (tid>=0x41U && tid<=0x4FU) { *scale=0.005; *unit="V"; return true; }
    if (tid>=0x50U && tid<=0x5FU) { *scale=0.05; *unit="V"; return true; }
    if (tid>=0x61U && tid<=0x6FU) { *scale=0.1; *unit="Hz"; return true; }
    if (tid>=0x70U && tid<=0x7FU) { *scale=1.0; *unit="count"; return true; }
    return false;
}

LinkObd2Result link_j1979_build_mode05_request(
    uint8_t tid,uint8_t oxygen_sensor,char *buffer,size_t buffer_size)
{
    const uint8_t b[3]={0x05U,tid,oxygen_sensor};
    size_t i;
    if (buffer==NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    if (buffer_size<7U) {
        if (buffer_size!=0U) buffer[0]='\0';
        return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
    }
    for (i=0U;i<3U;++i) {
        buffer[i*2U]=hex_digit((uint8_t)(b[i]>>4U));
        buffer[i*2U+1U]=hex_digit(b[i]);
    }
    buffer[6]='\0';
    return LINK_OBD2_RESULT_OK;
}

LinkObd2Result link_j1979_decode_mode05_response(
    const uint8_t *pdu,size_t pdu_length,LinkJ1979Mode05Result *result)
{
    double scale=0.0,offset=0.0;
    const char *unit=NULL;
    if (pdu==NULL || result==NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    if (pdu_length!=4U && pdu_length!=6U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    if (pdu[0]!=0x45U) return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    memset(result,0,sizeof(*result));
    result->tid=pdu[1]; result->oxygen_sensor=pdu[2]; result->raw_value=pdu[3];
    result->limits_available=pdu_length==6U;
    if (result->limits_available) { result->raw_minimum=pdu[4]; result->raw_maximum=pdu[5]; }
    result->scaling_known=mode05_scaling(result->tid,&scale,&offset,&unit);
    if (result->scaling_known) {
        result->unit=unit; result->value=(double)result->raw_value*scale+offset;
        if (result->limits_available) {
            result->minimum=(double)result->raw_minimum*scale+offset;
            result->maximum=(double)result->raw_maximum*scale+offset;
        }
    }
    return LINK_OBD2_RESULT_OK;
}

const LinkJ1979Mode06MonitorDefinition *link_j1979_mode06_monitor_definition(uint8_t mid)
{
    size_t i;
    for (i=0U;i<countmid();++i) if (mode06_monitors[i].mid==mid) return &mode06_monitors[i];
    return NULL;
}

LinkJ1979IdentifierClass link_j1979_mode06_mid_classification(uint8_t mid)
{
    if ((mid & 0x1FU)==0U) return LINK_J1979_IDENTIFIER_SUPPORT_BITMAP;
    if (mid>=0xE1U) return LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED;
    if (link_j1979_mode06_monitor_definition(mid)!=NULL) return LINK_J1979_IDENTIFIER_STANDARD;
    return LINK_J1979_IDENTIFIER_RESERVED;
}

LinkJ1979IdentifierClass link_j1979_mode06_tid_classification(uint8_t tid)
{
    if (tid>=0x01U && tid<=0x0CU) return LINK_J1979_IDENTIFIER_STANDARD;
    if (tid>=0x80U && tid<=0xFEU) return LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED;
    return LINK_J1979_IDENTIFIER_RESERVED;
}

const LinkJ1979Mode06TidDefinition *link_j1979_mode06_tid_definition(uint8_t tid)
{
    size_t i;
    for (i=0U;i<count06t();++i) if (mode06_tids[i].tid==tid) return &mode06_tids[i];
    return NULL;
}

const LinkJ1979UnitScaling *link_j1979_mode06_uasid_definition(uint8_t uasid)
{
    size_t i;
    for (i=0U;i<countuas();++i) if (mode06_uasids[i].uasid==uasid) return &mode06_uasids[i];
    return NULL;
}

double link_j1979_mode06_apply_scaling(const LinkJ1979UnitScaling *scaling,uint16_t raw)
{
    double value;
    if (scaling==NULL) return 0.0;
    if (scaling->signed_value) {
        const int32_t s=(raw & UINT16_C(0x8000))!=0U ? (int32_t)raw-INT32_C(65536) : (int32_t)raw;
        value=(double)s;
    } else value=(double)raw;
    return value*scaling->scale+scaling->offset;
}

LinkObd2Result link_j1979_decode_mode06_response(
    const uint8_t *pdu,size_t pdu_length,LinkJ1979Mode06ResultList *results)
{
    size_t count,i;
    if (pdu==NULL || results==NULL) return LINK_OBD2_RESULT_INVALID_ARGUMENT;
    memset(results,0,sizeof(*results));
    if (pdu_length==0U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    if (pdu[0]!=0x46U) return LINK_OBD2_RESULT_UNEXPECTED_RESPONSE;
    if (pdu_length<10U || (pdu_length-1U)%9U!=0U) return LINK_OBD2_RESULT_MALFORMED_RESPONSE;
    count=(pdu_length-1U)/9U;
    if (count>LINK_J1979_MODE06_MAX_RESULTS) return LINK_OBD2_RESULT_BUFFER_TOO_SMALL;
    for (i=0U;i<count;++i) {
        const uint8_t *r=pdu+1U+i*9U;
        LinkJ1979Mode06Result *o=&results->entries[i];
        o->mid=r[0]; o->tid=r[1]; o->uasid=r[2];
        o->raw_value=read_u16_be(r+3U); o->raw_minimum=read_u16_be(r+5U); o->raw_maximum=read_u16_be(r+7U);
        o->monitor=link_j1979_mode06_monitor_definition(o->mid);
        o->test=link_j1979_mode06_tid_definition(o->tid);
        o->scaling=link_j1979_mode06_uasid_definition(o->uasid);
        o->scaling_known=o->scaling!=NULL;
        if (o->scaling_known) {
            o->value=link_j1979_mode06_apply_scaling(o->scaling,o->raw_value);
            o->minimum=link_j1979_mode06_apply_scaling(o->scaling,o->raw_minimum);
            o->maximum=link_j1979_mode06_apply_scaling(o->scaling,o->raw_maximum);
            o->pass_known=true;
            o->passed=o->value>=o->minimum && o->value<=o->maximum;
        }
    }
    results->count=count;
    return LINK_OBD2_RESULT_OK;
}
