// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/mercedes_me_diaglogic.h"

#include <stdio.h>
#include <string.h>

#define CHECK(e) do { \
    if (!(e)) { \
        fprintf(stderr, "check failed: %s at %s:%d\n", \
                #e, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct Capture {
    size_t measured_count;
    size_t dtc_count;
    char measured_id[32];
    char requested_id[16];
    char trouble_code[16];
    double rpm;
} Capture;

static bool slice_equals(LinkMercedesMeProtoSlice slice, const char *text)
{
    size_t length = strlen(text);
    return slice.size == length &&
           (length == 0U || memcmp(slice.data, text, length) == 0);
}

static void measured_callback(
    void *context,
    const LinkMercedesMeDiaglogicMeasuredItem *item)
{
    Capture *capture = context;
    size_t count;
    if (capture == NULL || item == NULL) return;
    ++capture->measured_count;
    count = item->data_id.size < sizeof(capture->measured_id) - 1U
        ? item->data_id.size : sizeof(capture->measured_id) - 1U;
    memcpy(capture->measured_id, item->data_id.data, count);
    capture->measured_id[count] = '\0';
    if (item->has_value && item->value.has_double_value)
        capture->rpm = item->value.double_value;
}

static void dtc_callback(
    void *context,
    LinkMercedesMeProtoSlice requested_device_id,
    const LinkMercedesMeDiaglogicDtc *dtc)
{
    Capture *capture = context;
    size_t count;
    if (capture == NULL || dtc == NULL) return;
    ++capture->dtc_count;
    count = requested_device_id.size < sizeof(capture->requested_id) - 1U
        ? requested_device_id.size : sizeof(capture->requested_id) - 1U;
    memcpy(capture->requested_id, requested_device_id.data, count);
    capture->requested_id[count] = '\0';
    count = dtc->trouble_code.size < sizeof(capture->trouble_code) - 1U
        ? dtc->trouble_code.size : sizeof(capture->trouble_code) - 1U;
    memcpy(capture->trouble_code, dtc->trouble_code.data, count);
    capture->trouble_code[count] = '\0';
}

int main(void)
{
    /*
     * VehicleStatus:
     * assignedVin = "WDD207"
     * DtcCollection(requestedDeviceId="ECM",
     *               Dtc("P1234", STATIC, "Test", "7E8"))
     * MeasuredItem(dataId="engineRpm", unit="rpm",
     *              Value(DOUBLE, 1234.5), address=0x7e8, timestamp=123)
     * VehicleConfiguration("v1", "x", 42)
     * obdAdapterSwVersion = "1.2"
     */
    static const uint8_t vehicle_status[] = {
        0x0a,0x06,'W','D','D','2','0','7',
        0x22,0x1b,
          0x0a,0x03,'E','C','M',
          0x12,0x14,
            0x0a,0x05,'P','1','2','3','4',
            0x10,0x02,
            0x1a,0x04,'T','e','s','t',
            0x22,0x03,'7','E','8',
        0x2a,0x22,
          0x0a,0x09,'e','n','g','i','n','e','R','p','m',
          0x12,0x03,'r','p','m',
          0x1a,0x0b,
            0x08,0x02,
            0x19,0x00,0x00,0x00,0x00,0x00,0x4a,0x93,0x40,
          0x20,0xe8,0x0f,
          0x28,0x7b,
        0x32,0x09,
          0x0a,0x02,'v','1',
          0x12,0x01,'x',
          0x18,0x2a,
        0x3a,0x03,'1','.','2'
    };
    static const uint8_t preview_bytes[] = {
        0x08,0x01,0x12,0x03,'a','b','c',0x18,0x01
    };
    static const uint8_t long_value_bytes[] = {
        0x08,0x03,0x20,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x01
    };
    static const uint8_t missing_required_value[] = {
        0x10,0x01
    };
    static const uint8_t malformed_value[] = {
        0x18,0x01
    };
    LinkMercedesMeDiaglogicVehicleStatus status;
    LinkMercedesMeDiaglogicCallbacks callbacks;
    LinkMercedesMeDiaglogicPreview preview;
    LinkMercedesMeDiaglogicValue value;
    Capture capture;
    LinkMercedesMeDiaglogicResult result;

    memset(&capture, 0, sizeof(capture));
    callbacks.measured_item = measured_callback;
    callbacks.dtc = dtc_callback;
    callbacks.context = &capture;

    result = link_mercedes_me_diaglogic_decode_vehicle_status(
        vehicle_status, sizeof(vehicle_status), &status, &callbacks);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_OK);
    CHECK(slice_equals(status.assigned_vin, "WDD207"));
    CHECK(status.dtc_collection_count == 1U);
    CHECK(status.measured_item_count == 1U);
    CHECK(status.has_vehicle_configuration);
    CHECK(slice_equals(status.vehicle_configuration.version, "v1"));
    CHECK(slice_equals(status.vehicle_configuration.variant, "x"));
    CHECK(status.vehicle_configuration.has_timestamp);
    CHECK(status.vehicle_configuration.timestamp == 42);
    CHECK(slice_equals(status.obd_adapter_sw_version, "1.2"));
    CHECK(capture.measured_count == 1U);
    CHECK(strcmp(capture.measured_id, "engineRpm") == 0);
    CHECK(capture.rpm == 1234.5);
    CHECK(capture.dtc_count == 1U);
    CHECK(strcmp(capture.requested_id, "ECM") == 0);
    CHECK(strcmp(capture.trouble_code, "P1234") == 0);

    result = link_mercedes_me_diaglogic_decode_preview(
        preview_bytes, sizeof(preview_bytes), &preview);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_OK);
    CHECK(preview.cycle_completed);
    CHECK(slice_equals(preview.pending_action_token, "abc"));
    CHECK(preview.has_repeatable && preview.repeatable);

    result = link_mercedes_me_diaglogic_decode_value(
        long_value_bytes, sizeof(long_value_bytes), &value);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_OK);
    CHECK(value.value_type == LINK_MERCEDES_ME_VALUE_LONG);
    CHECK(value.has_long_value);
    CHECK(value.long_value == -1);

    result = link_mercedes_me_diaglogic_decode_value(
        missing_required_value, sizeof(missing_required_value), &value);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_REQUIRED_FIELD_MISSING);

    result = link_mercedes_me_diaglogic_decode_value(
        malformed_value, sizeof(malformed_value), &value);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED);

    result = link_mercedes_me_diaglogic_decode_vehicle_status(
        vehicle_status, sizeof(vehicle_status) - 1U, &status, &callbacks);
    CHECK(result == LINK_MERCEDES_ME_DIAGLOGIC_TRUNCATED);

    CHECK(strcmp(link_mercedes_me_diaglogic_result_name(
                     LINK_MERCEDES_ME_DIAGLOGIC_MALFORMED),
                 "malformed") == 0);
    return 0;
}
