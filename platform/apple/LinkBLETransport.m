// SPDX-License-Identifier: GPL-3.0-or-later
#import "LinkBLETransport.h"

#import "link/elm327.h"
#import "link/mercedes_me_adapter.h"

#import <CoreBluetooth/CoreBluetooth.h>
#import <ExternalAccessory/ExternalAccessory.h>

NS_ASSUME_NONNULL_BEGIN

static const NSTimeInterval LinkScanTimeoutSeconds = 12.0;
static const NSTimeInterval LinkConnectTimeoutSeconds = 8.0;
static const NSTimeInterval LinkDiscoveryTimeoutSeconds = 8.0;
static const NSTimeInterval LinkProbeTimeoutSeconds = 5.0;
static const NSTimeInterval LinkReconnectDelaySeconds = 0.75;
static const NSUInteger LinkRecoveryAttemptLimit = 5U;
static const NSUInteger LinkScanAttemptLimit = 4U;
static const NSUInteger LinkWriteQueueLimit = 65536U;
static NSString * const LinkKnownPeripheralDefaultsKey =
    @"link.ble.knownPeripheralIdentifier.v1";

@interface LinkBLECandidate : NSObject
@property(nonatomic, strong) CBService *service;
@property(nonatomic, strong) CBCharacteristic *writeCharacteristic;
@property(nonatomic, strong) CBCharacteristic *notifyCharacteristic;
@property(nonatomic) CBCharacteristicWriteType writeType;
@property(nonatomic) NSInteger score;
@end

@implementation LinkBLECandidate
@end

static NSComparisonResult LinkCompareCandidates(LinkBLECandidate *left,
                                                 LinkBLECandidate *right)
{
    if (left.score > right.score) return NSOrderedAscending;
    if (left.score < right.score) return NSOrderedDescending;
    NSString *leftKey = [NSString stringWithFormat:@"%@/%@/%@",
                         left.service.UUID.UUIDString,
                         left.writeCharacteristic.UUID.UUIDString,
                         left.notifyCharacteristic.UUID.UUIDString];
    NSString *rightKey = [NSString stringWithFormat:@"%@/%@/%@",
                          right.service.UUID.UUIDString,
                          right.writeCharacteristic.UUID.UUIDString,
                          right.notifyCharacteristic.UUID.UUIDString];
    return [leftKey compare:rightKey];
}

static void LinkSortCandidates(NSMutableArray<LinkBLECandidate *> *candidates)
{
    NSUInteger count = candidates.count;
    for (NSUInteger leftIndex = 0U; leftIndex < count; ++leftIndex) {
        for (NSUInteger rightIndex = leftIndex + 1U; rightIndex < count; ++rightIndex) {
            LinkBLECandidate *left = [candidates objectAtIndex:leftIndex];
            LinkBLECandidate *right = [candidates objectAtIndex:rightIndex];
            if (LinkCompareCandidates(left, right) == NSOrderedDescending) {
                [candidates exchangeObjectAtIndex:leftIndex withObjectAtIndex:rightIndex];
            }
        }
    }
}

static LinkAdapterKind LinkPeripheralAdapterKind(NSString *name)
{
    if (name.length == 0U) return LINK_ADAPTER_KIND_UNKNOWN;
    return link_adapter_kind_from_bluetooth_name(name.UTF8String);
}

static BOOL LinkPeripheralNameLooksLikeAdapter(NSString *name)
{
    LinkMercedesMeAdapterFamily family;
    if (name.length == 0U) return NO;
    family = link_mercedes_me_adapter_family_from_name(name.UTF8String);
    if (family != LINK_MERCEDES_ME_ADAPTER_UNKNOWN)
        return family == LINK_MERCEDES_ME_ADAPTER_BLE;
    return LinkPeripheralAdapterKind(name) != LINK_ADAPTER_KIND_UNKNOWN;
}

/*
 * CoreBluetooth cannot enumerate or open a Mercedes RFCOMM/SPP adapter.  When
 * iOS exposes a paired MFi accessory through External Accessory, inspect its
 * public identity fields so an MB-2/3/4/5/6/7 device produces an immediate,
 * truthful result instead of an unrelated BLE scan loop.
 */
static NSString * _Nullable LinkConnectedClassicMercedesAdapterName(void)
{
    NSArray<EAAccessory *> *accessories =
        [EAAccessoryManager sharedAccessoryManager].connectedAccessories;
    for (EAAccessory *accessory in accessories) {
        NSArray<NSString *> *identities = @[
            accessory.name ?: @"",
            accessory.modelNumber ?: @"",
            accessory.serialNumber ?: @""
        ];
        for (NSString *identity in identities) {
            LinkMercedesMeAdapterFamily family;
            if (identity.length == 0U) continue;
            family = link_mercedes_me_adapter_family_from_name(
                identity.UTF8String);
            if (link_mercedes_me_adapter_prefers_classic_spp(family))
                return identity;
        }
    }
    return nil;
}

static BOOL LinkUUIDEquals(CBUUID *uuid, const char *value)
{
    NSString *candidate;
    NSString *expected;
    if (uuid == nil || value == NULL) return NO;
    candidate = uuid.UUIDString;
    expected = [NSString stringWithUTF8String:value];
    return expected != nil &&
        [candidate caseInsensitiveCompare:expected] == NSOrderedSame;
}

static BOOL LinkRemainingBytesAreWhitespace(const uint8_t *bytes,
                                             NSUInteger start,
                                             NSUInteger length)
{
    if (bytes == NULL || start > length) return NO;
    for (NSUInteger index = start; index < length; ++index) {
        uint8_t value = bytes[index];
        if (value != (uint8_t)' ' && value != (uint8_t)'\t' &&
            value != (uint8_t)'\r' && value != (uint8_t)'\n') return NO;
    }
    return YES;
}

@interface LinkBLETransport () <CBCentralManagerDelegate, CBPeripheralDelegate>
@property(nonatomic, readwrite) LinkBLETransportState state;
@property(nonatomic, copy, readwrite) NSString *statusText;
@property(nonatomic, copy, readwrite, nullable) NSString *peripheralName;
@property(nonatomic, copy, readwrite, nullable) NSString *adapterIdentifier;
@property(nonatomic, copy, readwrite, nullable) NSString *serviceUUID;
@property(nonatomic, copy, readwrite, nullable) NSString *writeCharacteristicUUID;
@property(nonatomic, copy, readwrite, nullable) NSString *notifyCharacteristicUUID;
@property(nonatomic, readwrite) LinkAdapterKind adapterKind;
- (void)beginScan;
- (void)connectPeripheral:(CBPeripheral *)peripheral
                     name:(NSString *)name;
- (void)buildAndProbeCandidates;
- (void)probeNextCandidate;
- (void)sendProbeIfPossible;
- (void)failCurrentProbe;
- (void)flushWrites;
- (void)resetSelection;
- (void)recoverAfterTransientFailure:(NSString *)status;
- (void)failAndStop:(NSString *)status;
- (void)scheduleStateTimeout:(LinkBLETransportState)state
                  generation:(NSUInteger)generation
                       after:(NSTimeInterval)delay
                     message:(NSString *)message
                     recover:(BOOL)recover;
- (LinkTransportStatus)enqueueApplicationBytes:(const uint8_t *)bytes
                                          size:(size_t)size;
- (void)setCReceiver:(LinkTransportReceiveFn)receiver context:(void *)context;
@end

@implementation LinkBLETransport {
    CBCentralManager *_Nullable _central;
    CBPeripheral *_Nullable _peripheral;
    BOOL _startRequested;
    NSUInteger _operationGeneration;
    NSUInteger _scanAttempt;
    NSUInteger _recoveryAttempt;
    BOOL _knownPeripheralAttempted;
    NSUInteger _pendingServiceDiscoveries;
    NSArray<LinkBLECandidate *> *_Nullable _candidates;
    NSUInteger _candidateIndex;
    NSUInteger _channelProbeAttempt;
    LinkBLECandidate *_Nullable _probingCandidate;
    NSUInteger _probeGeneration;
    BOOL _probeSent;
    BOOL _probeParserActive;
    LinkElm327Parser _probeParser;
    CBCharacteristic *_Nullable _selectedWrite;
    CBCharacteristic *_Nullable _selectedNotify;
    CBCharacteristicWriteType _selectedWriteType;
    NSMutableData *_writeQueue;
    BOOL _writeWithResponseInFlight;
    LinkTransportReceiveFn _Nullable _receiver;
    void *_Nullable _receiverContext;
}

- (instancetype)init
{
    self = [super init];
    if (self != nil) {
        _state = LinkBLETransportStateIdle;
        _statusText = @"Idle";
        _adapterKind = LINK_ADAPTER_KIND_UNKNOWN;
        _writeQueue = [[NSMutableData alloc] init];
    }
    return self;
}

- (BOOL)isNativeAdapter
{
    return self.adapterKind == LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE;
}

- (BOOL)isReady
{
    return self.state == LinkBLETransportStateReady &&
           _peripheral.state == CBPeripheralStateConnected &&
           _selectedWrite != nil && _selectedNotify != nil;
}

- (void)notifyDelegate
{
    id<LinkBLETransportDelegate> delegate = self.delegate;
    if (delegate != nil) [delegate bleTransportDidUpdate:self];
}

- (void)setState:(LinkBLETransportState)state status:(NSString *)status
{
    self.state = state;
    self.statusText = status;
    [self notifyDelegate];
}

- (void)scheduleStateTimeout:(LinkBLETransportState)state
                  generation:(NSUInteger)generation
                       after:(NSTimeInterval)delay
                     message:(NSString *)message
                     recover:(BOOL)recover
{
    __weak LinkBLETransport *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(delay * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        LinkBLETransport *strongSelf = weakSelf;
        if (strongSelf == nil || !strongSelf->_startRequested ||
            strongSelf->_operationGeneration != generation ||
            strongSelf.state != state) return;
        if (recover) [strongSelf recoverAfterTransientFailure:message];
        else [strongSelf failAndStop:message];
    });
}

- (void)start
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self start]; });
        return;
    }
    if (!_startRequested) {
        _scanAttempt = 0U;
        _recoveryAttempt = 0U;
        _channelProbeAttempt = 0U;
        _knownPeripheralAttempted = NO;
    }
    _startRequested = YES;
    if (self.isReady) { [self notifyDelegate]; return; }
    if (_central == nil) {
        _central = [[CBCentralManager alloc] initWithDelegate:self
                                                       queue:dispatch_get_main_queue()];
        [self setState:LinkBLETransportStateWaitingForBluetooth status:@"Waiting for Bluetooth"];
        return;
    }
    if (_central.state == CBManagerStatePoweredOn) [self beginScan];
    else [self setState:LinkBLETransportStateWaitingForBluetooth status:@"Waiting for Bluetooth"];
}

- (void)disconnect
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self disconnect]; });
        return;
    }
    _startRequested = NO;
    _scanAttempt = 0U;
    _recoveryAttempt = 0U;
    _channelProbeAttempt = 0U;
    _knownPeripheralAttempted = NO;
    _operationGeneration++;
    _probeGeneration++;
    [_central stopScan];
    [_writeQueue setLength:0U];
    _writeWithResponseInFlight = NO;
    _probeParserActive = NO;
    CBPeripheral *peripheral = _peripheral;
    _peripheral = nil;
    [self resetSelection];
    if (peripheral != nil && peripheral.state != CBPeripheralStateDisconnected)
        [_central cancelPeripheralConnection:peripheral];
    [self setState:LinkBLETransportStateDisconnected status:@"Disconnected"];
}

- (void)resetSelection
{
    _candidates = nil;
    _candidateIndex = 0U;
    _probingCandidate = nil;
    _probeSent = NO;
    _probeParserActive = NO;
    _selectedWrite = nil;
    _selectedNotify = nil;
    _writeWithResponseInFlight = NO;
    self.serviceUUID = nil;
    self.writeCharacteristicUUID = nil;
    self.notifyCharacteristicUUID = nil;
}

- (void)failAndStop:(NSString *)status
{
    _startRequested = NO;
    _scanAttempt = 0U;
    _recoveryAttempt = 0U;
    _knownPeripheralAttempted = NO;
    _operationGeneration++;
    _probeGeneration++;
    [_central stopScan];
    [_writeQueue setLength:0U];
    _probeParserActive = NO;
    CBPeripheral *peripheral = _peripheral;
    _peripheral = nil;
    [self resetSelection];
    if (peripheral != nil && peripheral.state != CBPeripheralStateDisconnected)
        [_central cancelPeripheralConnection:peripheral];
    [self setState:LinkBLETransportStateFailed status:status];
}

- (void)recoverAfterTransientFailure:(NSString *)status
{
    if (!_startRequested) return;
    _recoveryAttempt++;
    if (_recoveryAttempt > LinkRecoveryAttemptLimit) {
        [self failAndStop:[NSString stringWithFormat:
            @"%@; retry limit reached", status]];
        return;
    }

    _operationGeneration++;
    NSUInteger recoveryGeneration = _operationGeneration;
    _probeGeneration++;
    [_central stopScan];
    [_writeQueue setLength:0U];
    _probeParserActive = NO;
    CBPeripheral *peripheral = _peripheral;
    _peripheral = nil;
    [self resetSelection];
    if (peripheral != nil && peripheral.state != CBPeripheralStateDisconnected)
        [_central cancelPeripheralConnection:peripheral];
    [self setState:LinkBLETransportStateDisconnected status:status];
    __weak LinkBLETransport *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
                                 (int64_t)(LinkReconnectDelaySeconds * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        LinkBLETransport *strongSelf = weakSelf;
        if (strongSelf == nil || !strongSelf->_startRequested ||
            strongSelf->_operationGeneration != recoveryGeneration) return;
        if (strongSelf->_central.state == CBManagerStatePoweredOn) [strongSelf beginScan];
        else [strongSelf setState:LinkBLETransportStateWaitingForBluetooth status:@"Waiting for Bluetooth"];
    });
}

- (void)beginScan
{
    if (!_startRequested || _central.state != CBManagerStatePoweredOn) return;
    NSString *classicAdapter = LinkConnectedClassicMercedesAdapterName();
    if (classicAdapter != nil) {
        self.peripheralName = classicAdapter;
        self.adapterKind = LINK_ADAPTER_KIND_MERCEDES_ME_NATIVE;
        [self failAndStop:[NSString stringWithFormat:
            @"%@ is a Bluetooth Classic Mercedes adapter. iPhone cannot open its RFCOMM/SPP channel through CoreBluetooth; an authorised External Accessory protocol is required.",
            classicAdapter]];
        return;
    }
    _operationGeneration++;
    _probeGeneration++;
    [_central stopScan];
    [_writeQueue setLength:0U];
    _writeWithResponseInFlight = NO;
    CBPeripheral *oldPeripheral = _peripheral;
    _peripheral = nil;
    if (oldPeripheral != nil && oldPeripheral.state != CBPeripheralStateDisconnected)
        [_central cancelPeripheralConnection:oldPeripheral];
    self.peripheralName = nil;
    self.adapterIdentifier = nil;
    self.adapterKind = LINK_ADAPTER_KIND_UNKNOWN;
    [self resetSelection];

    /*
     * CoreBluetooth gives an app a stable peripheral identifier. Reuse the
     * last channel that completed an ELM ATI probe before relying on local-name
     * advertisements again. A stale identifier still falls back to the normal
     * bounded scan path after the connection timeout.
     */
    if (!_knownPeripheralAttempted) {
        _knownPeripheralAttempted = YES;
        NSString *savedIdentifier = [[NSUserDefaults standardUserDefaults]
            stringForKey:LinkKnownPeripheralDefaultsKey];
        NSUUID *identifier = savedIdentifier.length != 0U
            ? [[NSUUID alloc] initWithUUIDString:savedIdentifier] : nil;
        if (identifier != nil) {
            NSArray<CBPeripheral *> *saved =
                [_central retrievePeripheralsWithIdentifiers:@[identifier]];
            CBPeripheral *known = saved.firstObject;
            if (known != nil) {
                NSString *name = known.name.length != 0U
                    ? known.name : @"saved BLE OBD adapter";
                [self connectPeripheral:known name:name];
                return;
            }
        }
    }

    _scanAttempt++;
    NSUInteger generation = _operationGeneration;
    /*
     * Some dual-mode ELM/Vgate adapters do not include their local name in
     * every advertising packet.  With CoreBluetooth's default duplicate
     * suppression the first nameless packet can be the only callback, which
     * makes the first scan miss the adapter while the second succeeds from
     * cached metadata.  Keep duplicate advertisements enabled until a matching
     * adapter is selected so the first user connection can see the later
     * named packet.
     */
    [_central scanForPeripheralsWithServices:nil
                                    options:@{
        CBCentralManagerScanOptionAllowDuplicatesKey: @YES
    }];
    [self setState:LinkBLETransportStateScanning
            status:_scanAttempt == 1U
                ? @"Scanning for Bluetooth diagnostic adapter"
                : @"Retrying compatible BLE diagnostic adapter scan"];
    [self scheduleStateTimeout:LinkBLETransportStateScanning generation:generation
                         after:LinkScanTimeoutSeconds
                       message:_scanAttempt < LinkScanAttemptLimit
                           ? @"No Bluetooth diagnostic adapter found; retrying"
                           : @"No compatible BLE diagnostic adapter found. MB-2/3/4/5/6/7 Mercedes adapters use Bluetooth Classic and require an authorised iPhone External Accessory protocol."
                       recover:_scanAttempt < LinkScanAttemptLimit];
}

- (void)connectPeripheral:(CBPeripheral *)peripheral
                     name:(NSString *)name
{
    if (!_startRequested || peripheral == nil) return;
    [_central stopScan];
    _operationGeneration++;
    NSUInteger generation = _operationGeneration;
    _peripheral = peripheral;
    _peripheral.delegate = self;
    self.peripheralName = name;
    self.adapterKind = LinkPeripheralAdapterKind(name);
    [self setState:LinkBLETransportStateConnecting
            status:[NSString stringWithFormat:@"Connecting to %@", name]];
    [_central connectPeripheral:peripheral options:nil];
    [self scheduleStateTimeout:LinkBLETransportStateConnecting
                    generation:generation
                         after:LinkConnectTimeoutSeconds
                       message:@"BLE adapter connection timed out"
                       recover:YES];
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central
{
    switch (central.state) {
    case CBManagerStatePoweredOn:
        if (_startRequested) [self beginScan];
        break;
    case CBManagerStatePoweredOff:
        _operationGeneration++;
        [central stopScan];
        [self setState:LinkBLETransportStateWaitingForBluetooth status:@"Bluetooth is off"];
        break;
    case CBManagerStateUnauthorized:
        [self failAndStop:@"Bluetooth permission denied"];
        break;
    case CBManagerStateUnsupported:
        [self failAndStop:@"Bluetooth LE is unsupported"];
        break;
    case CBManagerStateResetting:
    case CBManagerStateUnknown:
        _operationGeneration++;
        [self setState:LinkBLETransportStateWaitingForBluetooth status:@"Bluetooth is not ready"];
        break;
    }
}

- (void)centralManager:(CBCentralManager *)central
 didDiscoverPeripheral:(CBPeripheral *)peripheral
     advertisementData:(NSDictionary<NSString *, id> *)advertisementData
                  RSSI:(NSNumber *)RSSI
{
    if (!_startRequested || self.state != LinkBLETransportStateScanning) return;
    NSString *name = advertisementData[CBAdvertisementDataLocalNameKey];
    if (name.length == 0U) name = peripheral.name;
    if (name.length == 0U || !LinkPeripheralNameLooksLikeAdapter(name)) return;
    [self connectPeripheral:peripheral name:name];
    (void)RSSI;
    (void)central;
}

- (void)centralManager:(CBCentralManager *)central
  didConnectPeripheral:(CBPeripheral *)peripheral
{
    if (peripheral != _peripheral || !_startRequested) return;
    _operationGeneration++;
    NSUInteger generation = _operationGeneration;
    [self setState:LinkBLETransportStateDiscovering status:@"Discovering adapter services"];
    [peripheral discoverServices:nil];
    [self scheduleStateTimeout:LinkBLETransportStateDiscovering generation:generation
                         after:LinkDiscoveryTimeoutSeconds message:@"BLE service discovery timed out" recover:YES];
    (void)central;
}

- (void)centralManager:(CBCentralManager *)central
 didFailToConnectPeripheral:(CBPeripheral *)peripheral
                 error:(NSError * _Nullable)error
{
    if (peripheral == _peripheral && _startRequested) {
        NSString *message = error.localizedDescription != nil
            ? error.localizedDescription : @"BLE adapter connection failed";
        [self recoverAfterTransientFailure:message];
    }
    (void)central;
}

- (void)centralManager:(CBCentralManager *)central
 didDisconnectPeripheral:(CBPeripheral *)peripheral
                  error:(NSError * _Nullable)error
{
    if (peripheral != _peripheral) return;
    if (_startRequested) {
        NSString *message = error.localizedDescription != nil
            ? error.localizedDescription : @"Adapter disconnected; reconnecting";
        [self recoverAfterTransientFailure:message];
    } else {
        _peripheral = nil;
        [self resetSelection];
        [self setState:LinkBLETransportStateDisconnected status:@"Disconnected"];
    }
    (void)central;
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverServices:(NSError * _Nullable)error
{
    if (peripheral != _peripheral || !_startRequested) return;
    if (error != nil) { [self recoverAfterTransientFailure:error.localizedDescription]; return; }
    NSArray<CBService *> *services = peripheral.services;
    if (services.count == 0U) { [self recoverAfterTransientFailure:@"Adapter exposes no BLE services"]; return; }
    _pendingServiceDiscoveries = services.count;
    for (CBService *service in services) [peripheral discoverCharacteristics:nil forService:service];
}

- (void)peripheral:(CBPeripheral *)peripheral
didDiscoverCharacteristicsForService:(CBService *)service
             error:(NSError * _Nullable)error
{
    if (peripheral != _peripheral || !_startRequested) return;
    if (_pendingServiceDiscoveries > 0U) _pendingServiceDiscoveries--;
    (void)error;
    if (_pendingServiceDiscoveries == 0U) { _operationGeneration++; [self buildAndProbeCandidates]; }
    (void)service;
}

- (void)buildAndProbeCandidates
{
    _channelProbeAttempt++;
    NSMutableArray<LinkBLECandidate *> *result = [[NSMutableArray alloc] init];
    for (CBService *service in _peripheral.services) {
        NSArray<CBCharacteristic *> *characteristics = service.characteristics;
        for (CBCharacteristic *notify in characteristics) {
            CBCharacteristicProperties notifyProperties = notify.properties;
            BOOL canNotify = (notifyProperties & CBCharacteristicPropertyNotify) != 0;
            BOOL canIndicate = (notifyProperties & CBCharacteristicPropertyIndicate) != 0;
            if (!canNotify && !canIndicate) continue;
            for (CBCharacteristic *write in characteristics) {
                CBCharacteristicProperties writeProperties = write.properties;
                BOOL withoutResponse = (writeProperties & CBCharacteristicPropertyWriteWithoutResponse) != 0;
                BOOL withResponse = (writeProperties & CBCharacteristicPropertyWrite) != 0;
                if (!withoutResponse && !withResponse) continue;
                LinkBLECandidate *candidate = [[LinkBLECandidate alloc] init];
                candidate.service = service;
                candidate.writeCharacteristic = write;
                candidate.notifyCharacteristic = notify;
                candidate.writeType = withoutResponse ? CBCharacteristicWriteWithoutResponse : CBCharacteristicWriteWithResponse;
                NSInteger score = 0;
                score += withoutResponse ? 8 : 4;
                score += canNotify ? 4 : 2;
                if (write == notify) score += 1;
                candidate.score = score;
                [result addObject:candidate];
            }
        }
    }
    LinkSortCandidates(result);
    _candidates = [result copy];
    _candidateIndex = 0U;
    if (_candidates.count == 0U) {
        [self failAndStop:@"No writable/notify Bluetooth data channel found"];
        return;
    }

    if (self.isNativeAdapter) {
        LinkBLECandidate *candidate = nil;
        for (LinkBLECandidate *current in _candidates) {
            if (LinkUUIDEquals(current.service.UUID,
                               LINK_MERCEDES_ME_NUS_SERVICE_UUID) &&
                LinkUUIDEquals(current.writeCharacteristic.UUID,
                               LINK_MERCEDES_ME_NUS_RX_UUID) &&
                LinkUUIDEquals(current.notifyCharacteristic.UUID,
                               LINK_MERCEDES_ME_NUS_TX_UUID)) {
                candidate = current;
                break;
            }
        }
        if (candidate == nil) {
            for (LinkBLECandidate *current in _candidates) {
                if (LinkUUIDEquals(current.service.UUID,
                                   LINK_MERCEDES_ME_TOSHIBA_SERVICE_UUID) &&
                    LinkUUIDEquals(current.writeCharacteristic.UUID,
                                   LINK_MERCEDES_ME_TOSHIBA_CHARACTERISTIC_UUID) &&
                    current.writeCharacteristic == current.notifyCharacteristic) {
                    candidate = current;
                    break;
                }
            }
        }
        /*
         * Preserve passive-capture capability for an unexpected firmware
         * variant, but only after the two channels present in the official app
         * have been checked explicitly.
         */
        if (candidate == nil) candidate = _candidates.firstObject;
        _selectedWrite = candidate.writeCharacteristic;
        _selectedNotify = candidate.notifyCharacteristic;
        _selectedWriteType = candidate.writeType;
        self.serviceUUID = candidate.service.UUID.UUIDString;
        self.writeCharacteristicUUID =
            candidate.writeCharacteristic.UUID.UUIDString;
        self.notifyCharacteristicUUID =
            candidate.notifyCharacteristic.UUID.UUIDString;
        self.adapterIdentifier =
            @"Mercedes me Adapter A2138203202 · BLE family (native Bluetooth)";
        [self setState:LinkBLETransportStateProbing
                status:@"Preparing Mercedes me Nordic UART capture"];
        [_peripheral setNotifyValue:YES forCharacteristic:_selectedNotify];
        return;
    }

    [self setState:LinkBLETransportStateProbing
            status:_channelProbeAttempt == 1U
                ? @"Validating ELM327 BLE channel"
                : @"Retrying ELM327 BLE channel validation"];
    [self probeNextCandidate];
}

- (void)probeNextCandidate
{
    if (!_startRequested || _peripheral.state != CBPeripheralStateConnected) return;
    if (_candidateIndex >= _candidates.count) {
        if (_channelProbeAttempt < 2U) {
            [self recoverAfterTransientFailure:
                @"ELM327 channel did not wake; reconnecting once"];
        } else {
            [self failAndStop:@"No ELM327 command channel responded"];
        }
        return;
    }
    _probingCandidate = [_candidates objectAtIndex:_candidateIndex++];
    _probeGeneration++;
    NSUInteger generation = _probeGeneration;
    _probeSent = NO;
    _probeParserActive = link_elm327_parser_begin(&_probeParser, "ATI") == LINK_ELM327_RESULT_OK;
    if (!_probeParserActive) { [self failCurrentProbe]; return; }
    [_peripheral setNotifyValue:YES forCharacteristic:_probingCandidate.notifyCharacteristic];
    __weak LinkBLETransport *weakSelf = self;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(LinkProbeTimeoutSeconds * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        LinkBLETransport *strongSelf = weakSelf;
        if (strongSelf != nil && strongSelf->_startRequested &&
            strongSelf->_probeGeneration == generation &&
            strongSelf.state == LinkBLETransportStateProbing && strongSelf->_probingCandidate != nil)
            [strongSelf failCurrentProbe];
    });
}

- (void)sendProbeIfPossible
{
    if (_probingCandidate == nil || _probeSent || !_probeParserActive) return;
    if (_probingCandidate.writeType == CBCharacteristicWriteWithoutResponse && !_peripheral.canSendWriteWithoutResponse) return;
    uint8_t frame[LINK_ELM327_MAX_COMMAND + 1U];
    size_t frameSize = 0U;
    if (link_elm327_build_command("ATI", frame, sizeof(frame), &frameSize) != LINK_ELM327_RESULT_OK) {
        [self failCurrentProbe]; return;
    }
    NSUInteger maximum = [_peripheral maximumWriteValueLengthForType:_probingCandidate.writeType];
    if (maximum == 0U || frameSize > (size_t)maximum) { [self failCurrentProbe]; return; }
    NSData *probe = [NSData dataWithBytes:frame length:(NSUInteger)frameSize];
    _probeSent = YES;
    [_peripheral writeValue:probe forCharacteristic:_probingCandidate.writeCharacteristic type:_probingCandidate.writeType];
}

- (void)failCurrentProbe
{
    LinkBLECandidate *candidate = _probingCandidate;
    _probeGeneration++;
    _probingCandidate = nil;
    _probeSent = NO;
    _probeParserActive = NO;
    if (candidate != nil && candidate.notifyCharacteristic.isNotifying)
        [_peripheral setNotifyValue:NO forCharacteristic:candidate.notifyCharacteristic];
    dispatch_async(dispatch_get_main_queue(), ^{ [self probeNextCandidate]; });
}

- (void)peripheral:(CBPeripheral *)peripheral
didUpdateNotificationStateForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError * _Nullable)error
{
    if (peripheral != _peripheral) return;
    if (_probingCandidate != nil && characteristic == _probingCandidate.notifyCharacteristic) {
        if (error != nil || !characteristic.isNotifying) { [self failCurrentProbe]; return; }
        [self sendProbeIfPossible];
        return;
    }
    if (self.isNativeAdapter &&
        self.state == LinkBLETransportStateProbing &&
        characteristic == _selectedNotify) {
        if (error != nil || !characteristic.isNotifying) {
            [self recoverAfterTransientFailure:
                error.localizedDescription != nil
                    ? error.localizedDescription
                    : @"Mercedes me notification channel could not be enabled"];
            return;
        }
        [[NSUserDefaults standardUserDefaults]
            setObject:_peripheral.identifier.UUIDString
               forKey:LinkKnownPeripheralDefaultsKey];
        _scanAttempt = 0U;
        _recoveryAttempt = 0U;
        _channelProbeAttempt = 0U;
        [self setState:LinkBLETransportStateReady
                status:@"Mercedes me Adapter connected · native capture ready"];
        return;
    }
    if (characteristic == _selectedNotify && error != nil)
        [self recoverAfterTransientFailure:error.localizedDescription];
}

- (void)peripheral:(CBPeripheral *)peripheral
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError * _Nullable)error
{
    if (peripheral != _peripheral) return;
    if (error != nil) {
        if (_probingCandidate != nil && characteristic == _probingCandidate.notifyCharacteristic) [self failCurrentProbe];
        else [self recoverAfterTransientFailure:error.localizedDescription];
        return;
    }
    NSData *value = characteristic.value;
    if (value.length == 0U) return;
    if (_probingCandidate != nil && characteristic == _probingCandidate.notifyCharacteristic) {
        if (!_probeParserActive) { [self failCurrentProbe]; return; }
        size_t consumed = 0U;
        LinkElm327Result parseResult = link_elm327_parser_feed(&_probeParser, value.bytes, value.length, &consumed);
        if (parseResult == LINK_ELM327_RESULT_MORE_DATA) return;
        if (parseResult != LINK_ELM327_RESULT_OK) { [self failCurrentProbe]; return; }
        if (consumed < value.length && !LinkRemainingBytesAreWhitespace(value.bytes, (NSUInteger)consumed, value.length)) {
            [self failCurrentProbe]; return;
        }
        LinkElm327Response response;
        LinkElm327Result finishResult = link_elm327_parser_finish(&_probeParser, &response);
        if (finishResult != LINK_ELM327_RESULT_OK || response.length == 0U || response.line_count == 0U) {
            [self failCurrentProbe]; return;
        }
        NSString *identifier = [[NSString alloc] initWithBytes:response.text length:response.length encoding:NSASCIIStringEncoding];
        if (identifier.length == 0U) { [self failCurrentProbe]; return; }
        LinkBLECandidate *candidate = _probingCandidate;
        _probeGeneration++;
        _probingCandidate = nil;
        _probeSent = NO;
        _probeParserActive = NO;
        _selectedWrite = candidate.writeCharacteristic;
        _selectedNotify = candidate.notifyCharacteristic;
        _selectedWriteType = candidate.writeType;
        self.adapterIdentifier = identifier;
        [[NSUserDefaults standardUserDefaults]
            setObject:_peripheral.identifier.UUIDString
               forKey:LinkKnownPeripheralDefaultsKey];
        _scanAttempt = 0U;
        _recoveryAttempt = 0U;
        _channelProbeAttempt = 0U;
        self.serviceUUID = candidate.service.UUID.UUIDString;
        self.writeCharacteristicUUID = candidate.writeCharacteristic.UUID.UUIDString;
        self.notifyCharacteristicUUID = candidate.notifyCharacteristic.UUID.UUIDString;
        [self setState:LinkBLETransportStateReady status:@"BLE adapter ready"];
        return;
    }
    if (self.isReady && characteristic == _selectedNotify) {
        LinkTransportReceiveFn receiver = NULL;
        void *receiverContext = NULL;
        @synchronized (self) { receiver = _receiver; receiverContext = _receiverContext; }
        if (receiver != NULL) receiver(receiverContext, value.bytes, value.length);
    }
}

- (LinkTransportStatus)enqueueApplicationBytes:(const uint8_t *)bytes size:(size_t)size
{
    if (!self.isReady) return LINK_TRANSPORT_NOT_CONNECTED;
    if (bytes == NULL || size == 0U || size > (size_t)NSUIntegerMax) return LINK_TRANSPORT_INVALID_ARGUMENT;
    if (size > (size_t)LinkWriteQueueLimit || _writeQueue.length > LinkWriteQueueLimit - (NSUInteger)size)
        return LINK_TRANSPORT_BUSY;
    [_writeQueue appendBytes:bytes length:(NSUInteger)size];
    [self flushWrites];
    return LINK_TRANSPORT_OK;
}

- (void)flushWrites
{
    while (self.isReady && _writeQueue.length != 0U) {
        if (_selectedWriteType == CBCharacteristicWriteWithResponse && _writeWithResponseInFlight) return;
        if (_selectedWriteType == CBCharacteristicWriteWithoutResponse && !_peripheral.canSendWriteWithoutResponse) return;
        NSUInteger maximum = [_peripheral maximumWriteValueLengthForType:_selectedWriteType];
        if (maximum == 0U) { [self recoverAfterTransientFailure:@"BLE adapter reported zero write capacity"]; return; }
        NSUInteger chunkLength = MIN(maximum, _writeQueue.length);
        NSData *chunk = [_writeQueue subdataWithRange:NSMakeRange(0U, chunkLength)];
        [_writeQueue replaceBytesInRange:NSMakeRange(0U, chunkLength) withBytes:NULL length:0U];
        if (_selectedWriteType == CBCharacteristicWriteWithResponse) _writeWithResponseInFlight = YES;
        [_peripheral writeValue:chunk forCharacteristic:_selectedWrite type:_selectedWriteType];
        if (_selectedWriteType == CBCharacteristicWriteWithResponse) return;
    }
}

- (void)peripheralIsReadyToSendWriteWithoutResponse:(CBPeripheral *)peripheral
{
    if (peripheral != _peripheral) return;
    if (_probingCandidate != nil && !_probeSent) { [self sendProbeIfPossible]; return; }
    [self flushWrites];
}

- (void)peripheral:(CBPeripheral *)peripheral
didWriteValueForCharacteristic:(CBCharacteristic *)characteristic
             error:(NSError * _Nullable)error
{
    if (peripheral != _peripheral) return;
    if (_probingCandidate != nil && characteristic == _probingCandidate.writeCharacteristic) {
        if (error != nil) [self failCurrentProbe];
        return;
    }
    if (characteristic == _selectedWrite) {
        _writeWithResponseInFlight = NO;
        if (error != nil) { [self recoverAfterTransientFailure:error.localizedDescription]; return; }
        [self flushWrites];
    }
}

- (void)setCReceiver:(LinkTransportReceiveFn)receiver context:(void *)context
{
    @synchronized (self) { _receiver = receiver; _receiverContext = context; }
}

@end

static LinkTransportStatus LinkCTransportConnect(void *context)
{
    LinkBLETransport *transport = (__bridge LinkBLETransport *)context;
    if (transport == nil) return LINK_TRANSPORT_INVALID_ARGUMENT;
    if ([NSThread isMainThread]) [transport start];
    else dispatch_sync(dispatch_get_main_queue(), ^{ [transport start]; });
    return LINK_TRANSPORT_OK;
}

static void LinkCTransportDisconnect(void *context)
{
    LinkBLETransport *transport = (__bridge LinkBLETransport *)context;
    if (transport == nil) return;
    if ([NSThread isMainThread]) [transport disconnect];
    else dispatch_sync(dispatch_get_main_queue(), ^{ [transport disconnect]; });
}

static bool LinkCTransportIsConnected(void *context)
{
    LinkBLETransport *transport = (__bridge LinkBLETransport *)context;
    if (transport == nil) return false;
    __block BOOL ready = NO;
    if ([NSThread isMainThread]) ready = transport.isReady;
    else dispatch_sync(dispatch_get_main_queue(), ^{ ready = transport.isReady; });
    return ready;
}

static LinkTransportStatus LinkCTransportWrite(void *context, const uint8_t *data, size_t size)
{
    LinkBLETransport *transport = (__bridge LinkBLETransport *)context;
    if (transport == nil) return LINK_TRANSPORT_INVALID_ARGUMENT;
    __block LinkTransportStatus status = LINK_TRANSPORT_IO_ERROR;
    void (^writeBlock)(void) = ^{ status = [transport enqueueApplicationBytes:data size:size]; };
    if ([NSThread isMainThread]) writeBlock();
    else dispatch_sync(dispatch_get_main_queue(), writeBlock);
    return status;
}

static void LinkCTransportSetReceiver(void *context,
                                      LinkTransportReceiveFn receiver,
                                      void *receiverContext)
{
    LinkBLETransport *transport = (__bridge LinkBLETransport *)context;
    if (transport == nil) return;
    [transport setCReceiver:receiver context:receiverContext];
}

LinkTransport LinkBLETransportMakeCTransport(LinkBLETransport *transport)
{
    LinkTransport result = LINK_TRANSPORT_INIT;
    if (transport == nil) return result;
    result.context = (__bridge void *)transport;
    result.connect = LinkCTransportConnect;
    result.disconnect = LinkCTransportDisconnect;
    result.is_connected = LinkCTransportIsConnected;
    result.write = LinkCTransportWrite;
    result.set_receiver = LinkCTransportSetReceiver;
    return result;
}

NS_ASSUME_NONNULL_END
