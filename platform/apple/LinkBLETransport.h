// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file LinkBLETransport.h
 * @brief Shared CoreBluetooth byte-stream provider for LINK product faces.
 *
 * CoreBluetooth is necessarily an Objective-C platform edge. ELM parsing,
 * diagnostic policy and vehicle interpretation remain in portable LINK C.
 */
#import <Foundation/Foundation.h>

#import "link/transport.h"

NS_ASSUME_NONNULL_BEGIN

@class LinkBLETransport;

typedef NS_ENUM(NSInteger, LinkBLETransportState) {
    LinkBLETransportStateIdle = 0,
    LinkBLETransportStateWaitingForBluetooth,
    LinkBLETransportStateScanning,
    LinkBLETransportStateConnecting,
    LinkBLETransportStateDiscovering,
    LinkBLETransportStateProbing,
    LinkBLETransportStateReady,
    LinkBLETransportStateDisconnected,
    LinkBLETransportStateFailed
};

@protocol LinkBLETransportDelegate <NSObject>
- (void)bleTransportDidUpdate:(LinkBLETransport *)transport;
@end

@interface LinkBLETransport : NSObject

@property(nonatomic, weak, nullable) id<LinkBLETransportDelegate> delegate;
@property(nonatomic, readonly) LinkBLETransportState state;
@property(nonatomic, copy, readonly) NSString *statusText;
@property(nonatomic, copy, readonly, nullable) NSString *peripheralName;
@property(nonatomic, copy, readonly, nullable) NSString *adapterIdentifier;
@property(nonatomic, copy, readonly, nullable) NSString *serviceUUID;
@property(nonatomic, copy, readonly, nullable) NSString *writeCharacteristicUUID;
@property(nonatomic, copy, readonly, nullable) NSString *notifyCharacteristicUUID;
@property(nonatomic, readonly, getter=isReady) BOOL ready;
@property(nonatomic, readonly) LinkAdapterKind adapterKind;
@property(nonatomic, readonly, getter=isNativeAdapter) BOOL nativeAdapter;

- (void)start;
- (void)disconnect;

@end

/** Return LINK's portable C transport facade backed by the BLE provider. */
LinkTransport LinkBLETransportMakeCTransport(LinkBLETransport *transport);

NS_ASSUME_NONNULL_END
