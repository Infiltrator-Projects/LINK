// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LINK_PARAMETER_STORE_H
#define LINK_PARAMETER_STORE_H

#include "link/parameter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_PARAMETER_STORE_DEFINITION_CAPACITY 256U
#define LINK_PARAMETER_STORE_HISTORY_CAPACITY 1024U

typedef enum {
    LINK_PARAMETER_STORE_OK = 0,
    LINK_PARAMETER_STORE_INVALID_ARGUMENT,
    LINK_PARAMETER_STORE_FULL,
    LINK_PARAMETER_STORE_DUPLICATE_KEY,
    LINK_PARAMETER_STORE_DUPLICATE_STABLE_KEY,
    LINK_PARAMETER_STORE_NOT_FOUND,
    LINK_PARAMETER_STORE_DEFINITION_MISMATCH
} LinkParameterStoreResult;

typedef struct {
    const LinkParameterDefinition *definition;
    LinkParameterSample latest;
    bool latest_valid;
    bool favourite;
} LinkParameterStoreSlot;

typedef struct {
    LinkParameterStoreSlot slots[LINK_PARAMETER_STORE_DEFINITION_CAPACITY];
    LinkParameterSample history[LINK_PARAMETER_STORE_HISTORY_CAPACITY];
    size_t slot_count;
    size_t history_head;
    size_t history_count;
    uint64_t total_sample_count;
} LinkParameterStore;

const char *link_parameter_store_result_name(LinkParameterStoreResult result);
void link_parameter_store_init(LinkParameterStore *store);
void link_parameter_store_clear_samples(LinkParameterStore *store);
LinkParameterStoreResult link_parameter_store_register(LinkParameterStore *store, const LinkParameterDefinition *definition);
size_t link_parameter_store_definition_count(const LinkParameterStore *store);
const LinkParameterDefinition *link_parameter_store_definition_at(const LinkParameterStore *store, size_t index);
const LinkParameterDefinition *link_parameter_store_definition(const LinkParameterStore *store, const LinkParameterKey *key);
const LinkParameterDefinition *link_parameter_store_definition_for_stable_key(const LinkParameterStore *store, const char *stable_key);
LinkParameterStoreResult link_parameter_store_set_favourite(LinkParameterStore *store, const LinkParameterKey *key, bool favourite);
bool link_parameter_store_is_favourite(const LinkParameterStore *store, const LinkParameterKey *key);
LinkParameterStoreResult link_parameter_store_record(LinkParameterStore *store, const LinkParameterSample *sample);
bool link_parameter_store_latest(const LinkParameterStore *store, const LinkParameterKey *key, LinkParameterSample *sample);
size_t link_parameter_store_history_count(const LinkParameterStore *store);
uint64_t link_parameter_store_total_sample_count(const LinkParameterStore *store);
bool link_parameter_store_history_at(const LinkParameterStore *store, size_t chronological_index, LinkParameterSample *sample);

#ifdef __cplusplus
}
#endif
#endif
