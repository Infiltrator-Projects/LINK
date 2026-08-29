// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file transport.h
 * @brief Platform-neutral byte-stream transport boundary for LINK.
 *
 * Providers implement this contract at the operating-system/framework edge.
 * The portable diagnostic engine owns no provider resources and may copy the
 * descriptor; therefore `context` and every provider resource it references
 * must outlive all copies using them.
 */
#ifndef LINK_TRANSPORT_H
#define LINK_TRANSPORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LINK_TRANSPORT_ABI 1U

typedef enum LinkTransportStatus {
    LINK_TRANSPORT_OK = 0,
    LINK_TRANSPORT_NOT_CONNECTED,
    LINK_TRANSPORT_BUSY,
    LINK_TRANSPORT_TIMEOUT,
    LINK_TRANSPORT_IO_ERROR,
    LINK_TRANSPORT_UNSUPPORTED,
    LINK_TRANSPORT_INVALID_ARGUMENT
} LinkTransportStatus;

typedef enum LinkAdapterKind {
    LINK_ADAPTER_KIND_UNKNOWN = 0,
    LINK_ADAPTER_KIND_ELM327,
    LINK_ADAPTER_KIND_TACTRIX_OPENPORT2,
    LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE,
    LINK_ADAPTER_KIND_STM32_LINK
} LinkAdapterKind;

/*
 * Capability flags describe protocol surfaces that LINK can actually exploit.
 * They are deliberately more precise than an adapter name: a provider may
 * emulate the ELM command surface while using native ISO-TP underneath.
 * Numeric maxima are zero when the provider/adapter limit is not proven here.
 */
#define LINK_ADAPTER_CAP_BYTE_STREAM          UINT32_C(0x00000001)
#define LINK_ADAPTER_CAP_ELM_COMMAND_SURFACE  UINT32_C(0x00000002)
#define LINK_ADAPTER_CAP_RAW_CAN              UINT32_C(0x00000004)
#define LINK_ADAPTER_CAP_ISOTP                UINT32_C(0x00000008)
#define LINK_ADAPTER_CAP_CAN_11BIT             UINT32_C(0x00000010)
#define LINK_ADAPTER_CAP_CAN_29BIT             UINT32_C(0x00000020)
#define LINK_ADAPTER_CAP_CAN_FILTERS           UINT32_C(0x00000040)
#define LINK_ADAPTER_CAP_MULTI_RESPONSE        UINT32_C(0x00000080)
#define LINK_ADAPTER_CAP_RESPONSE_CAN_ID       UINT32_C(0x00000100)
#define LINK_ADAPTER_CAP_SECURE_SESSION        UINT32_C(0x00000200)
#define LINK_ADAPTER_CAP_NATIVE_DIAGNOSTIC     UINT32_C(0x00000400)

typedef struct LinkAdapterCapabilities {
    LinkAdapterKind kind;
    uint32_t flags;
    uint32_t max_standard_can_id;
    size_t max_raw_can_payload;
    size_t max_isotp_payload;
    size_t max_filter_ids;
    uint64_t default_response_timeout_ms;
} LinkAdapterCapabilities;

/** Classify a Bluetooth local/alias name without probing or transmitting. */
LinkAdapterKind link_adapter_kind_from_bluetooth_name(const char *name);
/** Stable diagnostic/log label for an adapter kind. */
const char *link_adapter_kind_name(LinkAdapterKind kind);
/** True only for adapters whose bytes require a native non-ELM session. */
bool link_adapter_kind_requires_native_protocol(LinkAdapterKind kind);

/** Return the strongest protocol capabilities currently proved for this kind. */
bool link_adapter_capabilities(
    LinkAdapterKind kind,
    LinkAdapterCapabilities *capabilities);

/** Convenience test for one or more required capability bits. */
bool link_adapter_has_capability(LinkAdapterKind kind, uint32_t capability);

/** Borrowed receive bytes are valid only for the duration of the callback. */
typedef void (*LinkTransportReceiveFn)(void *context,
                                       const uint8_t *data,
                                       size_t size);

/**
 * Platform-neutral byte-stream provider contract.
 *
 * `write()` must consume or copy its input before returning.
 * `set_receiver(context, NULL, NULL)` must detach a previous receiver.
 * Providers must serialize callback delivery for one transport instance.
 */
typedef struct LinkTransport {
    size_t struct_size;
    uint32_t abi_version;
    void *context;
    LinkTransportStatus (*connect)(void *context);
    void (*disconnect)(void *context);
    bool (*is_connected)(void *context);
    LinkTransportStatus (*write)(void *context,
                                 const uint8_t *data,
                                 size_t size);
    void (*set_receiver)(void *context,
                         LinkTransportReceiveFn receiver,
                         void *receiver_context);
} LinkTransport;

#define LINK_TRANSPORT_INIT \
    { .struct_size = sizeof(LinkTransport), \
      .abi_version = LINK_TRANSPORT_ABI, \
      .context = NULL, .connect = NULL, .disconnect = NULL, \
      .is_connected = NULL, .write = NULL, .set_receiver = NULL }

/** Validate ABI metadata and every mandatory operation pointer. */
bool link_transport_is_valid(const LinkTransport *transport);

#ifdef __cplusplus
}
#endif

#endif
