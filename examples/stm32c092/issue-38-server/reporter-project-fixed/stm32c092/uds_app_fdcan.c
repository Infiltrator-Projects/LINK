#include "uds_app_fdcan.h"

#include "uds_app_config.h"
#include "uds_platform_fdcan.h"

#include <stddef.h>
#include <string.h>

static UdsC092FdcanTransport *s_transport;
static UdsIsoTpEndpoint s_endpoint;
#define UDS_C092_RX_QUEUE_CAPACITY 8U
static IsoTpCanFrame s_rx_queue[UDS_C092_RX_QUEUE_CAPACITY];
static volatile uint8_t s_rx_head;
static volatile uint8_t s_rx_tail;
static volatile uint32_t s_rx_dropped;
static IsoTpCanFrame s_deferred_queue[UDS_C092_RX_QUEUE_CAPACITY];
static uint8_t s_deferred_head;
static uint8_t s_deferred_tail;
static uint32_t s_deferred_dropped;
static bool s_initialized;

void uds_c092_app_init(UdsC092FdcanTransport *transport, uint32_t now_ms,
                       const UdsCallbacks *application_callbacks, void *uds_context,
                       UdsIsoTpResetEventFn reset_event, void *reset_event_context) {
    if (transport == NULL)
        return;

    UdsIsoTpEndpointConfig config = {0};
    isotp_config_classic_can(&config.isotp_config);
#if UDS_C092_CLASSIC_PADDING_ENABLED
    isotp_config_set_padding(&config.isotp_config, true, UDS_C092_CLASSIC_PADDING_VALUE);
#endif
    config.send_frame = uds_c092_fdcan_send;
    config.tx_complete = uds_c092_fdcan_tx_complete;
    config.clock_ms = uds_c092_fdcan_clock;
    config.reset_event = reset_event;
    config.context = transport;
    config.reset_event_context = reset_event_context;
    config.request_id = UDS_C092_REQUEST_ID;
    config.response_id = UDS_C092_RESPONSE_ID;
    config.functional_request_id = UDS_C092_FUNCTIONAL_REQUEST_ID;
    if (application_callbacks != NULL)
        config.uds_callbacks = *application_callbacks;
    config.uds_callbacks.ecu_reset = uds_c092_platform_reset_prepare;
    config.uds_callbacks.ecu_reset_execute = uds_c092_platform_reset_execute;
    config.uds_context = uds_context;

    s_transport = transport;
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_rx_dropped = 0U;
    s_deferred_head = 0U;
    s_deferred_tail = 0U;
    s_deferred_dropped = 0U;
    s_initialized = uds_isotp_endpoint_init(&s_endpoint, &config, now_ms);
}

void uds_c092_app_init_default(UdsC092FdcanTransport *transport, uint32_t now_ms) {
    uds_c092_app_init(transport, now_ms, NULL, NULL, NULL, NULL);
}

void uds_c092_app_rx_from_isr(uint32_t can_id, const uint8_t *data, uint8_t dlc, bool is_fd,
                              bool bit_rate_switch) {
    uint8_t head;
    IsoTpCanFrame *slot;

    if (!s_initialized || (data == NULL) || (dlc == 0U) ||
        (dlc > (is_fd ? ISOTP_MAX_FRAME_DATA : 8U)) ||
        ((can_id != UDS_C092_REQUEST_ID) && (can_id != UDS_C092_FUNCTIONAL_REQUEST_ID)))
        return;

    head = s_rx_head;
    if ((uint8_t)(head - s_rx_tail) >= UDS_C092_RX_QUEUE_CAPACITY) {
        ++s_rx_dropped;
        return;
    }
    slot = &s_rx_queue[head % UDS_C092_RX_QUEUE_CAPACITY];
    slot->can_id = can_id;
    slot->dlc = dlc;
    slot->is_fd = is_fd;
    slot->bit_rate_switch = bit_rate_switch;
    (void)memcpy(slot->data, data, dlc);
    s_rx_head = (uint8_t)(head + 1U);
}

static bool endpoint_response_active(void) {
    return s_endpoint.tx_pending || s_endpoint.tx_in_flight ||
           (isotp_tx_state(&s_endpoint.tx) != ISOTP_TX_STATE_IDLE) ||
           s_endpoint.queued_response_pending;
}

static bool frame_is_flow_control(const IsoTpCanFrame *frame) {
    return (frame != NULL) && (frame->can_id == UDS_C092_REQUEST_ID) &&
           (frame->dlc != 0U) && ((frame->data[0] >> 4U) == 0x03U);
}

static bool pop_rx_frame(IsoTpCanFrame *frame) {
    uint8_t tail;
    bool has_frame = false;

    if (frame == NULL)
        return false;
    __disable_irq();
    tail = s_rx_tail;
    if (tail != s_rx_head) {
        *frame = s_rx_queue[tail % UDS_C092_RX_QUEUE_CAPACITY];
        s_rx_tail = (uint8_t)(tail + 1U);
        has_frame = true;
    }
    __enable_irq();
    return has_frame;
}

static bool defer_rx_frame(const IsoTpCanFrame *frame) {
    uint8_t head;
    if (frame == NULL)
        return false;
    head = s_deferred_head;
    if ((uint8_t)(head - s_deferred_tail) >= UDS_C092_RX_QUEUE_CAPACITY) {
        ++s_deferred_dropped;
        return false;
    }
    s_deferred_queue[head % UDS_C092_RX_QUEUE_CAPACITY] = *frame;
    s_deferred_head = (uint8_t)(head + 1U);
    return true;
}

static bool pop_deferred_frame(IsoTpCanFrame *frame) {
    uint8_t tail;
    if (frame == NULL)
        return false;
    tail = s_deferred_tail;
    if (tail == s_deferred_head)
        return false;
    *frame = s_deferred_queue[tail % UDS_C092_RX_QUEUE_CAPACITY];
    s_deferred_tail = (uint8_t)(tail + 1U);
    return true;
}

void uds_c092_app_process(uint32_t now_ms) {
    IsoTpCanFrame frame = {0};

    if (!s_initialized || (s_transport == NULL))
        return;

    uds_c092_fdcan_poll_tx_events(s_transport);
    if (uds_c092_fdcan_tx_complete(s_transport))
        uds_isotp_endpoint_tx_complete(&s_endpoint);
    (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);

    /* Replay exactly one deferred diagnostic request when the previous
     * response is fully idle. Further queued requests remain ordered. */
    if (!endpoint_response_active() && pop_deferred_frame(&frame)) {
        (void)uds_isotp_endpoint_receive(&s_endpoint, &frame, now_ms);
        (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);
    }

    /*
     * Drain the hardware-facing queue. FlowControl must reach an active
     * segmented response immediately; ordinary PCAN requests that race that
     * response move to the deferred FIFO instead of being discarded.
     */
    while (pop_rx_frame(&frame)) {
        if (endpoint_response_active() && !frame_is_flow_control(&frame)) {
            (void)defer_rx_frame(&frame);
            continue;
        }
        (void)uds_isotp_endpoint_receive(&s_endpoint, &frame, now_ms);
        (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);
    }

    (void)uds_isotp_endpoint_process(&s_endpoint, now_ms);
    (void)uds_isotp_endpoint_tick(&s_endpoint, now_ms);
}

uint32_t uds_c092_app_rx_dropped(void) {
    return s_rx_dropped;
}

uint32_t uds_c092_app_deferred_dropped(void) {
    return s_deferred_dropped;
}
