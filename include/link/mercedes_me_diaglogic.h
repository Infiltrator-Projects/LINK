// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file mercedes_me_diaglogic.h
 * @brief Allocation-free decoder for the archived DiagLogic Values.proto ABI.
 *
 * The schema is an interoperability fact recovered from the official Mercedes
 * me Adapter 4.7.61 application. Decoding this protobuf does not imply that
 * LINK knows how to invoke the missing native diaglogic/gdk libraries.
 */
#ifndef LINK_MERCEDES_ME_DIAGLOGIC_H
#define LINK_MERCEDES_ME_DIAGLOGIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkMercedesMeDiaglogicResult {
    LINK_MERCEDES_ME_DIAGLOGIC_OK = 0,
    LINK_MERCEDES_ME_DIAGLOGIC_INVALID_ARGUMENT,
    LINK_MERCEDES_ME_DIAGLOGIC_TRUNCATED,
    LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED,
    LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING
} LinkMercedesMeDiaglogicResult;

typedef struct LinkMercedesMeProtoSlice {
    const uint8_t *data;
    size_t size;
} LinkMercedesMeProtoSlice;

typedef enum LinkMercedesMeValueType {
    LINK_MERCEDES_ME_VALUE_UNKNOWN = 0,
    LINK_MERCEDES_ME_VALUE_BOOLEAN = 1,
    LINK_MERCEDES_ME_VALUE_DOUBLE = 2,
    LINK_MERCEDES_ME_VALUE_LONG = 3,
    LINK_MERCEDES_ME_VALUE_STRING = 4,
    LINK_MERCEDES_ME_VALUE_BYTEARRAY = 5
} LinkMercedesMeValueType;

typedef enum LinkMercedesMeDtcFlag {
    LINK_MERCEDES_ME_DTC_FLAG_UNKNOWN = 0,
    LINK_MERCEDES_ME_DTC_SPORADIC = 1,
    LINK_MERCEDES_ME_DTC_STATIC = 2
} LinkMercedesMeDtcFlag;

typedef struct LinkMercedesMeDiaglogicValue {
    int value_type;
    bool has_boolean_value;
    bool boolean_value;
    bool has_double_value;
    double double_value;
    bool has_long_value;
    int64_t long_value;
    LinkMercedesMeProtoSlice string_value;
    LinkMercedesMeProtoSlice bytes_value;
} LinkMercedesMeDiaglogicValue;

typedef struct LinkMercedesMeDiaglogicMeasuredItem {
    LinkMercedesMeProtoSlice data_id;
    LinkMercedesMeProtoSlice unit;
    bool has_value;
    LinkMercedesMeDiaglogicValue value;
    bool has_responded_device_address;
    int64_t responded_device_address;
    bool has_timestamp;
    int64_t timestamp;
} LinkMercedesMeDiaglogicMeasuredItem;

typedef struct LinkMercedesMeDiaglogicDtc {
    LinkMercedesMeProtoSlice trouble_code;
    int sporadic_flag;
    LinkMercedesMeProtoSlice display_text;
    LinkMercedesMeProtoSlice responded_device_id;
} LinkMercedesMeDiaglogicDtc;

typedef struct LinkMercedesMeDiaglogicVehicleConfiguration {
    LinkMercedesMeProtoSlice version;
    LinkMercedesMeProtoSlice variant;
    bool has_timestamp;
    int64_t timestamp;
} LinkMercedesMeDiaglogicVehicleConfiguration;

typedef struct LinkMercedesMeDiaglogicVehicleStatus {
    LinkMercedesMeProtoSlice assigned_vin;
    LinkMercedesMeProtoSlice error_code_as_string;
    LinkMercedesMeProtoSlice error_message;
    bool has_vehicle_configuration;
    LinkMercedesMeDiaglogicVehicleConfiguration vehicle_configuration;
    LinkMercedesMeProtoSlice obd_adapter_sw_version;
    size_t dtc_collection_count;
    size_t measured_item_count;
} LinkMercedesMeDiaglogicVehicleStatus;

typedef struct LinkMercedesMeDiaglogicPreview {
    bool cycle_completed;
    LinkMercedesMeProtoSlice pending_action_token;
    bool has_repeatable;
    bool repeatable;
} LinkMercedesMeDiaglogicPreview;

/*
 * All slices returned below point directly into the caller-owned protobuf
 * buffer. The buffer must therefore remain alive while the slices are used.
 */
typedef void (*LinkMercedesMeDiaglogicMeasuredItemFn)(
    void *context,
    const LinkMercedesMeDiaglogicMeasuredItem *item);

typedef void (*LinkMercedesMeDiaglogicMeasuredItemCollectionFn)(
    void *context,
    LinkMercedesMeProtoSlice requested_device_id,
    const LinkMercedesMeDiaglogicMeasuredItem *item);

typedef void (*LinkMercedesMeDiaglogicDtcFn)(
    void *context,
    LinkMercedesMeProtoSlice requested_device_id,
    const LinkMercedesMeDiaglogicDtc *dtc);

typedef struct LinkMercedesMeDiaglogicCallbacks {
    LinkMercedesMeDiaglogicMeasuredItemFn measured_item;
    LinkMercedesMeDiaglogicDtcFn dtc;
    void *context;
} LinkMercedesMeDiaglogicCallbacks;

const char *link_mercedes_me_diaglogic_result_name(
    LinkMercedesMeDiaglogicResult result);

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_value(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicValue *value);

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_measured_item(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicMeasuredItem *item);

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_measured_item_collection(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicMeasuredItemCollectionFn item_fn,
    void *context,
    size_t *item_count);

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_dtc(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicDtc *dtc);

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_dtc_collection(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicDtcFn dtc_fn,
    void *context,
    size_t *dtc_count);

LinkMercedesMeDiaglogicResult
link_mercedes_me_diaglogic_decode_vehicle_configuration(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicVehicleConfiguration *configuration);

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_vehicle_status(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicVehicleStatus *status,
    const LinkMercedesMeDiaglogicCallbacks *callbacks);

LinkMercedesMeDiaglogicResult link_mercedes_me_diaglogic_decode_preview(
    const uint8_t *bytes,
    size_t size,
    LinkMercedesMeDiaglogicPreview *preview);

#ifdef __cplusplus
}
#endif
#endif
