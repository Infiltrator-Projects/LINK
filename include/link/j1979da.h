// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file j1979da.h
 * @brief Verified SAE J1979/J1979-DA service-05/service-06 registry support.
 *
 * Current standard targets, message-format implementation and compiled semantic
 * registry coverage are deliberately separate claims. Unknown current-annex
 * rows stay explicit/raw rather than receiving guessed semantics.
 */
#ifndef LINK_J1979DA_H
#define LINK_J1979DA_H

#include "link/obd2.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_J1979_MODE06_MAX_RESULTS 32U

typedef enum LinkJ1979IdentifierClass {
    LINK_J1979_IDENTIFIER_RESERVED = 0,
    LINK_J1979_IDENTIFIER_SUPPORT_BITMAP,
    LINK_J1979_IDENTIFIER_STANDARD,
    LINK_J1979_IDENTIFIER_MANUFACTURER_DEFINED,
    LINK_J1979_IDENTIFIER_UNVERIFIED
} LinkJ1979IdentifierClass;

typedef struct LinkJ1979Mode05TidDefinition {
    uint8_t tid;
    const char *name;
    double scale;
    double offset;
    const char *unit;
    bool constant;
} LinkJ1979Mode05TidDefinition;

typedef struct LinkJ1979Mode05Result {
    uint8_t tid;
    uint8_t oxygen_sensor;
    uint8_t raw_value;
    uint8_t raw_minimum;
    uint8_t raw_maximum;
    bool limits_available;
    bool scaling_known;
    double value;
    double minimum;
    double maximum;
    const char *unit;
} LinkJ1979Mode05Result;

typedef struct LinkJ1979Mode06MonitorDefinition {
    uint8_t mid;
    const char *name;
} LinkJ1979Mode06MonitorDefinition;

typedef struct LinkJ1979Mode06TidDefinition {
    uint8_t tid;
    const char *name;
    uint8_t standard_uasid;
} LinkJ1979Mode06TidDefinition;

typedef struct LinkJ1979UnitScaling {
    uint8_t uasid;
    double scale;
    double offset;
    const char *unit;
    bool signed_value;
} LinkJ1979UnitScaling;

typedef struct LinkJ1979Mode06Result {
    uint8_t mid;
    uint8_t tid;
    uint8_t uasid;
    uint16_t raw_value;
    uint16_t raw_minimum;
    uint16_t raw_maximum;
    const LinkJ1979Mode06MonitorDefinition *monitor;
    const LinkJ1979Mode06TidDefinition *test;
    const LinkJ1979UnitScaling *scaling;
    bool scaling_known;
    double value;
    double minimum;
    double maximum;
    bool pass_known;
    bool passed;
} LinkJ1979Mode06Result;

typedef struct LinkJ1979Mode06ResultList {
    LinkJ1979Mode06Result entries[LINK_J1979_MODE06_MAX_RESULTS];
    size_t count;
} LinkJ1979Mode06ResultList;

const char *link_j1979_revision(void);
const char *link_j1979da_revision(void);
const char *link_j1978_1_revision(void);
const char *link_j1979_2_revision(void);
const char *link_j1979da_public_semantics_revision(void);

LinkJ1979IdentifierClass link_j1979_mode05_tid_classification(uint8_t tid);
const LinkJ1979Mode05TidDefinition *link_j1979_mode05_tid_definition(uint8_t tid);

LinkObd2Result link_j1979_build_mode05_request(
    uint8_t tid,
    uint8_t oxygen_sensor,
    char *buffer,
    size_t buffer_size);

LinkObd2Result link_j1979_decode_mode05_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkJ1979Mode05Result *result);

LinkJ1979IdentifierClass link_j1979_mode06_mid_classification(uint8_t mid);
const LinkJ1979Mode06MonitorDefinition *link_j1979_mode06_monitor_definition(
    uint8_t mid);
LinkJ1979IdentifierClass link_j1979_mode06_tid_classification(uint8_t tid);
const LinkJ1979Mode06TidDefinition *link_j1979_mode06_tid_definition(uint8_t tid);
LinkJ1979IdentifierClass link_j1979_mode06_uasid_classification(uint8_t uasid);
const LinkJ1979UnitScaling *link_j1979_mode06_uasid_definition(uint8_t uasid);
LinkJ1979IdentifierClass link_j1979_mode09_infotype_classification(uint8_t info_type);
double link_j1979_mode06_apply_scaling(
    const LinkJ1979UnitScaling *scaling,
    uint16_t raw);

LinkObd2Result link_j1979_decode_mode06_response(
    const uint8_t *pdu,
    size_t pdu_length,
    LinkJ1979Mode06ResultList *results);

#ifdef __cplusplus
}
#endif
#endif
