// SPDX-License-Identifier: GPL-3.0-or-later
#import "LinkDiagnosticsController.h"

#import "link/elm327.h"
#import "link/obd2.h"
#import "link/telemetry.h"
#import "link/transport.h"

#include <stdint.h>

@interface LinkDiagnosticsController () <LinkBLETransportDelegate>
@property(nonatomic, copy, readwrite) NSString *statusText;
@property(nonatomic, copy, readwrite, nullable) NSString *peripheralName;
@property(nonatomic, copy, readwrite, nullable) NSString *adapterIdentifier;
@property(nonatomic, copy, readwrite) NSString *faultScanStatusText;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *storedDTCs;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *pendingDTCs;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *permanentDTCs;
@property(nonatomic, readwrite, getter=isActive) BOOL active;
@property(nonatomic, readwrite, getter=isReady) BOOL ready;

- (void)notifyDelegate;
- (void)setSharedStatus:(NSString *)status;
- (void)prepareForStart;
- (void)beginPortableSession;
- (void)startTickTimer;
- (void)stopTickTimer;
- (BOOL)beginCommand:(const char *)command timeout:(uint64_t)timeoutMs;
- (void)notifyManufacturerFailure:(NSString *)status;
- (void)recoverManufacturerExtensionAfterFailure:(NSString *)status;
- (void)finishManufacturerRecovery;
- (void)handleSessionEvent:(const LinkElm327Session *)session;
- (void)processCompletedResponse;
- (BOOL)applyFlowEvent:(const LinkDiagnosticFlowEvent *)event;
- (void)driveDiagnosticFlow;
@end

@implementation LinkDiagnosticsController {
    LinkBLETransport *_provider;
    LinkElm327Session _session;
    BOOL _sessionInitialized;
    BOOL _simulated;
    BOOL _manufacturerExtensionActive;
    BOOL _manufacturerRecoveryActive;
    LinkElm327Simulator _simulator;
    LinkDiagnosticFlow _flow;
    LinkDiagnosticFlowConfig _flowConfig;

    LinkTelemetryStore _telemetry;
    LinkTelemetryRecorder _recorder;
    LinkTelemetrySessionMetadata _sessionMetadata;
    NSMutableData *_sessionCSV;

    dispatch_source_t _tickTimer;
    NSUInteger _pollGeneration;
    uint64_t _sessionMonotonicStartMs;

    NSString *_productSlug;
    NSString *_liveStatusText;
    NSString *_simulatedLiveStatusText;
    NSString *_standardVINStatusText;
}

static uint64_t LinkAppleMonotonicMilliseconds(void)
{
    NSTimeInterval uptime = NSProcessInfo.processInfo.systemUptime;
    if (uptime <= 0.0) return 0U;
    const double milliseconds = uptime * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}

static uint64_t LinkAppleElapsedMilliseconds(uint64_t startedMs)
{
    const uint64_t nowMs = LinkAppleMonotonicMilliseconds();
    return nowMs >= startedMs ? nowMs - startedMs : 0U;
}

static uint64_t LinkAppleEpochMilliseconds(void)
{
    NSTimeInterval seconds = [NSDate date].timeIntervalSince1970;
    if (seconds <= 0.0) return 0U;
    const double milliseconds = seconds * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}

static NSString *LinkAppleStringFromCString(const char *value)
{
    if (value == NULL) return @"unknown";
    NSString *string = [NSString stringWithUTF8String:value];
    return string != nil ? string : @"unknown";
}

static NSArray<NSString *> *LinkAppleDTCStrings(const LinkObd2DtcList *list)
{
    if (list == NULL || list->count == 0U) return @[];
    NSMutableArray<NSString *> *values =
        [[NSMutableArray alloc] initWithCapacity:list->count];
    for (size_t index = 0U; index < list->count; ++index) {
        NSString *code = LinkAppleStringFromCString(list->entries[index].code);
        if (code.length != 0U) [values addObject:code];
    }
    return [values copy];
}

static bool LinkAppleAppendCSV(void *context, const char *bytes, size_t length)
{
    if (context == NULL || bytes == NULL) return false;
    NSMutableData *data = (__bridge NSMutableData *)context;
    [data appendBytes:bytes length:length];
    return true;
}

static bool LinkAppleFlowIsFaultScan(const LinkDiagnosticFlow *flow)
{
    if (flow == NULL) return false;
    return flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS;
}

static void LinkAppleSessionEvent(
    void *context,
    const LinkElm327Session *session)
{
    LinkDiagnosticsController *controller =
        (__bridge LinkDiagnosticsController *)context;
    if (controller == nil || session == NULL) return;
    [controller handleSessionEvent:session];
}

- (instancetype)initWithProductSlug:(NSString *)productSlug
                         flowConfig:(LinkDiagnosticFlowConfig)flowConfig
                     liveStatusText:(NSString *)liveStatusText
            simulatedLiveStatusText:(NSString *)simulatedLiveStatusText
              standardVINStatusText:(NSString *)standardVINStatusText
{
    self = [super init];
    if (self == nil) return nil;

    _productSlug = [productSlug copy];
    _liveStatusText = [liveStatusText copy];
    _simulatedLiveStatusText = [simulatedLiveStatusText copy];
    _standardVINStatusText = [standardVINStatusText copy];
    _flowConfig = flowConfig;

    _provider = [[LinkBLETransport alloc] init];
    _provider.delegate = self;

    _statusText = @"Idle";
    _faultScanStatusText = @"Not scanned";
    _storedDTCs = @[];
    _pendingDTCs = @[];
    _permanentDTCs = @[];

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    link_telemetry_store_init(&_telemetry);
    link_telemetry_recorder_init(&_recorder);
    _sessionCSV = [[NSMutableData alloc] init];
    link_telemetry_store_set_favourite(&_telemetry, UINT8_C(0x0c), true);
    link_telemetry_store_set_favourite(&_telemetry, UINT8_C(0x0d), true);
    link_telemetry_store_set_favourite(&_telemetry, UINT8_C(0x05), true);
    link_telemetry_store_set_favourite(&_telemetry, UINT8_C(0x0b), true);
    link_telemetry_session_metadata_init(&_sessionMetadata, 0U, NULL, NULL);
    return self;
}

- (void)dealloc
{
    _provider.delegate = nil;
    [self stopTickTimer];
    if (_recorder.started && !_recorder.finished)
        (void)link_telemetry_recorder_finish(
            &_recorder, LinkAppleEpochMilliseconds());

    if (_sessionInitialized) {
        _sessionInitialized = NO;
        link_elm327_session_disconnect(&_session);
        link_elm327_session_deinit(&_session);
    } else if (!_simulated) {
        [_provider disconnect];
    }
}

- (BOOL)isSimulated
{
    return _simulated;
}

- (BOOL)isManufacturerExtensionActive
{
    return _manufacturerExtensionActive;
}

- (const LinkDiagnosticFlow *)diagnosticFlow
{
    return &_flow;
}

- (void)notifyDelegate
{
    id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
    if (delegate != nil) [delegate linkDiagnosticsControllerDidUpdate:self];
}

- (void)setSharedStatus:(NSString *)status
{
    self.statusText = status != nil ? status : @"";
    [self notifyDelegate];
}

- (void)prepareForStart
{
    _pollGeneration++;
    self.active = YES;
    self.ready = NO;
    self.adapterIdentifier = nil;
    self.faultScanStatusText = @"Waiting for vehicle connection";
    self.storedDTCs = @[];
    self.pendingDTCs = @[];
    self.permanentDTCs = @[];
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    link_telemetry_store_clear_samples(&_telemetry);
    link_telemetry_recorder_init(&_recorder);
    _sessionCSV = [[NSMutableData alloc] init];
    _sessionMonotonicStartMs = LinkAppleMonotonicMilliseconds();
    link_telemetry_session_metadata_init(
        &_sessionMetadata, LinkAppleEpochMilliseconds(), NULL, NULL);
}

- (void)start
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self start]; });
        return;
    }
    if (self.active) return;

    _simulated = NO;
    [self prepareForStart];
    self.peripheralName = nil;
    [self notifyDelegate];
    [_provider start];
}

- (void)startSimulatedWithAdapterIdentifier:(const char *)adapterIdentifier
                                        vin:(const char *)vin
                            customResponder:
                                (LinkElm327SimulatorCustomResponderFn)responder
                                    context:(void *)context
{
    if (![NSThread isMainThread]) {
        NSString *adapterCopy = LinkAppleStringFromCString(adapterIdentifier);
        NSString *vinCopy = LinkAppleStringFromCString(vin);
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startSimulatedWithAdapterIdentifier:adapterCopy.UTF8String
                                                  vin:vinCopy.UTF8String
                                      customResponder:responder
                                              context:context];
        });
        return;
    }
    if (self.active) return;

    _simulated = YES;
    [self prepareForStart];
    self.peripheralName = @"Simulated ELM327";

    LinkElm327SimulatorConfig config = LINK_ELM327_SIMULATOR_CONFIG_INIT;
    config.adapter_identifier = adapterIdentifier;
    config.vin = vin;
    config.custom_responder = responder;
    config.custom_context = context;
    link_elm327_simulator_init(&_simulator, &config);
    [self notifyDelegate];
    [self beginPortableSession];
}

- (void)disconnect
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self disconnect]; });
        return;
    }

    _pollGeneration++;
    [self stopTickTimer];
    if (_sessionInitialized) {
        _sessionInitialized = NO;
        link_elm327_session_disconnect(&_session);
        link_elm327_session_deinit(&_session);
    } else if (!_simulated) {
        [_provider disconnect];
    }

    const uint64_t endedEpochMs = LinkAppleEpochMilliseconds();
    link_telemetry_session_metadata_finish(&_sessionMetadata, endedEpochMs);
    if (_recorder.started && !_recorder.finished)
        (void)link_telemetry_recorder_finish(&_recorder, endedEpochMs);

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;
    _simulated = NO;
    self.active = NO;
    self.ready = NO;
    [self setSharedStatus:@"Disconnected"];
}

- (void)bleTransportDidUpdate:(LinkBLETransport *)transport
{
    if (_simulated) return;

    self.peripheralName = transport.peripheralName;
    self.adapterIdentifier = transport.adapterIdentifier;
    if (transport.adapterIdentifier != nil) {
        link_telemetry_session_metadata_set_adapter(
            &_sessionMetadata, transport.adapterIdentifier.UTF8String);
    }

    if (transport.isReady && !_sessionInitialized) {
        [self beginPortableSession];
        return;
    }

    if (!transport.isReady &&
        _sessionInitialized &&
        transport.state != LinkBLETransportStateProbing) {
        _pollGeneration++;
        [self stopTickTimer];
        _sessionInitialized = NO;
        link_elm327_session_deinit(&_session);
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        if (_manufacturerExtensionActive)
            [self notifyManufacturerFailure:
                @"Vehicle transport disconnected during manufacturer extension"];
        _manufacturerExtensionActive = NO;
        self.ready = NO;
    }

    if (!_sessionInitialized) self.statusText = transport.statusText;
    [self notifyDelegate];
}

- (void)beginPortableSession
{
    LinkTransport transport = _simulated
        ? link_elm327_simulator_transport(&_simulator)
        : LinkBLETransportMakeCTransport(_provider);

    if (!link_transport_is_valid(&transport) ||
        !link_elm327_session_init(
            &_session, &transport, LinkAppleSessionEvent,
            (__bridge void *)self)) {
        [self failWithStatus:@"Failed to initialise portable diagnostic session"];
        return;
    }

    _sessionInitialized = YES;
    if (_simulated &&
        link_elm327_session_connect(&_session) != LINK_TRANSPORT_OK) {
        _sessionInitialized = NO;
        link_elm327_session_deinit(&_session);
        [self failWithStatus:@"Failed to connect simulated ELM327 transport"];
        return;
    }

    if (!_recorder.started &&
        !link_telemetry_recorder_begin(
            &_recorder, &_sessionMetadata, _productSlug.UTF8String,
            LinkAppleAppendCSV, (__bridge void *)_sessionCSV)) {
        _sessionInitialized = NO;
        link_elm327_session_disconnect(&_session);
        link_elm327_session_deinit(&_session);
        [self failWithStatus:@"Could not start portable session recorder"];
        return;
    }

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    if (link_diagnostic_flow_start(&_flow) !=
        LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        [self failWithStatus:@"Could not start shared diagnostic flow"];
        return;
    }

    [self startTickTimer];
    [self setSharedStatus:_simulated
        ? @"Initialising simulated ELM327 adapter"
        : @"Initialising ELM327 adapter"];
    [self driveDiagnosticFlow];
}

- (void)startTickTimer
{
    [self stopTickTimer];
    _tickTimer = dispatch_source_create(
        DISPATCH_SOURCE_TYPE_TIMER, 0U, 0U, dispatch_get_main_queue());
    dispatch_source_set_timer(
        _tickTimer,
        dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC),
        100 * NSEC_PER_MSEC,
        20 * NSEC_PER_MSEC);

    __weak LinkDiagnosticsController *weakSelf = self;
    dispatch_source_set_event_handler(_tickTimer, ^{
        LinkDiagnosticsController *strongSelf = weakSelf;
        if (strongSelf == nil || !strongSelf->_sessionInitialized) return;
        (void)link_elm327_session_tick(
            &strongSelf->_session, LinkAppleMonotonicMilliseconds());
    });
    dispatch_resume(_tickTimer);
}

- (void)stopTickTimer
{
    if (_tickTimer != nil) {
        dispatch_source_cancel(_tickTimer);
        _tickTimer = nil;
    }
}

- (BOOL)beginCommand:(const char *)command timeout:(uint64_t)timeoutMs
{
    if (!_sessionInitialized || command == NULL) return NO;

    LinkElm327SessionOpResult result = link_elm327_session_begin(
        &_session, command, LinkAppleMonotonicMilliseconds(), timeoutMs);
    if (result != LINK_ELM327_SESSION_OP_OK) {
        NSString *reason = LinkAppleStringFromCString(
            link_elm327_session_op_result_name(result));
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:[NSString stringWithFormat:
            @"Diagnostic command failed: %@", reason]];
        return NO;
    }
    return YES;
}

- (BOOL)beginManufacturerCommand:(const char *)command
                         timeout:(uint64_t)timeoutMs
{
    if (!_manufacturerExtensionActive) return NO;
    return [self beginCommand:command timeout:timeoutMs];
}

- (void)notifyManufacturerFailure:(NSString *)status
{
    id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:
            @selector(linkDiagnosticsController:
                manufacturerExtensionDidFailWithStatus:)]) {
        [delegate linkDiagnosticsController:self
            manufacturerExtensionDidFailWithStatus:status];
    }
}

- (void)recoverManufacturerExtensionAfterFailure:(NSString *)status
{
    if (!_manufacturerExtensionActive || !_sessionInitialized) return;

    [self notifyManufacturerFailure:status];
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = YES;
    _flow.config.restore_adapter_after_manufacturer_extension = true;

    LinkDiagnosticFlowResult flowResult =
        link_diagnostic_flow_resume_after_manufacturer(&_flow);
    if (flowResult != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        _manufacturerRecoveryActive = NO;
        [self failWithStatus:
            @"Manufacturer scan stopped and shared flow could not resume"];
        return;
    }

    LinkElm327SessionOpResult sessionResult =
        link_elm327_session_begin_resynchronization(
            &_session, LinkAppleMonotonicMilliseconds(), UINT64_C(2500));
    if (sessionResult != LINK_ELM327_SESSION_OP_OK) {
        _manufacturerRecoveryActive = NO;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:[NSString stringWithFormat:
            @"Manufacturer scan stopped; adapter resynchronisation could not start: %@",
            LinkAppleStringFromCString(
                link_elm327_session_op_result_name(sessionResult))]];
        return;
    }
    [self setSharedStatus:
        @"Manufacturer scan interrupted; resynchronising adapter"];
}

- (void)finishManufacturerRecovery
{
    if (!_manufacturerRecoveryActive) return;
    _manufacturerRecoveryActive = NO;
    [self setSharedStatus:
        @"Manufacturer scan interrupted; continuing standard diagnostics"];
    [self driveDiagnosticFlow];
}

- (void)handleSessionEvent:(const LinkElm327Session *)session
{
    if (session == NULL) return;

    if (session->status == LINK_ELM327_SESSION_COMPLETE) {
        dispatch_async(
            dispatch_get_main_queue(), ^{ [self processCompletedResponse]; });
        return;
    }

    if (session->status == LINK_ELM327_SESSION_RESYNCHRONIZED) {
        if (_manufacturerRecoveryActive) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self finishManufacturerRecovery];
            });
        }
        return;
    }

    if (session->status == LINK_ELM327_SESSION_TIMED_OUT) {
        if (_manufacturerExtensionActive) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self recoverManufacturerExtensionAfterFailure:
                    @"Manufacturer diagnostic request timed out"];
            });
            return;
        }
        if (_manufacturerRecoveryActive) {
            _manufacturerRecoveryActive = NO;
            _flow.elm_failure = session->elm_result;
            link_diagnostic_flow_fail(
                &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
            [self setSharedStatus:
                @"Adapter resynchronisation timed out; reconnect required"];
            return;
        }
        if (LinkAppleFlowIsFaultScan(&_flow))
            self.faultScanStatusText =
                @"Fault scan timed out; reconnect required";
        _flow.elm_failure = session->elm_result;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:
            @"Diagnostic request timed out; reconnect to resynchronise"];
        return;
    }

    if (session->status == LINK_ELM327_SESSION_FAILED) {
        NSString *reason = LinkAppleStringFromCString(
            link_elm327_result_name(session->elm_result));
        if (_manufacturerExtensionActive && session->needs_resync) {
            NSString *status = [NSString stringWithFormat:
                @"Manufacturer diagnostic adapter error: %@", reason];
            dispatch_async(dispatch_get_main_queue(), ^{
                [self recoverManufacturerExtensionAfterFailure:status];
            });
            return;
        }
        if (_manufacturerRecoveryActive) _manufacturerRecoveryActive = NO;
        if (LinkAppleFlowIsFaultScan(&_flow)) {
            self.faultScanStatusText = [NSString stringWithFormat:
                @"Fault scan adapter error: %@", reason];
        }
        _flow.elm_failure = session->elm_result;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        if (_manufacturerExtensionActive) {
            [self notifyManufacturerFailure:[NSString stringWithFormat:
                @"Manufacturer diagnostic adapter error: %@", reason]];
        }
        _manufacturerExtensionActive = NO;
        [self setSharedStatus:[NSString stringWithFormat:
            @"Adapter response failed: %@", reason]];
        return;
    }

    if (session->status == LINK_ELM327_SESSION_CANCELLED) {
        (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
        if (_manufacturerExtensionActive)
            [self notifyManufacturerFailure:
                @"Manufacturer diagnostic request cancelled"];
        _manufacturerExtensionActive = NO;
        _manufacturerRecoveryActive = NO;
        [self setSharedStatus:@"Diagnostic request cancelled"];
    }
}

- (void)processCompletedResponse
{
    const LinkElm327Response *response =
        link_elm327_session_response(&_session);
    if (response == NULL) {
        [self failWithStatus:@"Diagnostic response was unavailable"];
        return;
    }

    const uint64_t elapsed =
        LinkAppleElapsedMilliseconds(_sessionMonotonicStartMs);
    (void)link_telemetry_store_record_transcript(
        &_telemetry, elapsed, _session.parser.command,
        (uint32_t)response->result, response->text);

    if (_recorder.started && !_recorder.finished &&
        !link_telemetry_recorder_record_response_named(
            &_recorder, elapsed, _session.parser.command,
            link_elm327_result_name(response->result), response->text)) {
        [self failWithStatus:@"Could not append diagnostic transcript"];
        return;
    }

    if (_manufacturerExtensionActive) {
        id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
        if ([delegate respondsToSelector:
                @selector(linkDiagnosticsController:
                    didReceiveManufacturerResponse:)]) {
            [delegate linkDiagnosticsController:self
                didReceiveManufacturerResponse:response];
        } else {
            [self failWithStatus:
                @"Manufacturer extension has no response handler"];
        }
        return;
    }

    LinkDiagnosticFlowEvent event;
    LinkDiagnosticFlowResult result = link_diagnostic_flow_accept_response(
        &_flow, response, LinkAppleMonotonicMilliseconds(), &event);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        NSString *reason = LinkAppleStringFromCString(
            link_diagnostic_flow_result_name(result));
        [self failWithStatus:[NSString stringWithFormat:
            @"Shared diagnostic flow failed: %@", reason]];
        return;
    }

    if (![self applyFlowEvent:&event]) return;
    [self driveDiagnosticFlow];
}

- (BOOL)applyFlowEvent:(const LinkDiagnosticFlowEvent *)event
{
    if (event == NULL) return NO;

    switch (event->kind) {
    case LINK_DIAGNOSTIC_FLOW_EVENT_NONE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_PID_DISCOVERY_COMPLETE:
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_ADAPTER_IDENTIFIED: {
        const char *identifier =
            link_diagnostic_flow_adapter_identifier(&_flow);
        if (identifier != NULL) {
            self.adapterIdentifier =
                LinkAppleStringFromCString(identifier);
            link_telemetry_session_metadata_set_adapter(
                &_sessionMetadata, identifier);
        }
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN:
        if (event->vin_available && event->vin != NULL) {
            link_telemetry_session_metadata_set_vehicle(
                &_sessionMetadata, event->vin);
        }
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_DTC_LIST: {
        NSArray<NSString *> *codes =
            LinkAppleDTCStrings(event->dtc_list);
        switch (event->dtc_kind) {
        case LINK_OBD2_DTC_STORED:
            self.storedDTCs = codes;
            break;
        case LINK_OBD2_DTC_PENDING:
            self.pendingDTCs = codes;
            break;
        case LINK_OBD2_DTC_PERMANENT:
            self.permanentDTCs = codes;
            self.faultScanStatusText = [NSString stringWithFormat:
                @"Complete · %lu stored · %lu pending · %lu permanent",
                (unsigned long)self.storedDTCs.count,
                (unsigned long)self.pendingDTCs.count,
                (unsigned long)self.permanentDTCs.count];
            break;
        }
        if (event->became_ready) self.ready = YES;
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE: {
        LinkTelemetryMeasurement measurement = {
            .pid = event->sample.pid,
            .value = event->sample.value,
            .unit = event->sample.unit
        };
        const uint64_t elapsed =
            LinkAppleElapsedMilliseconds(_sessionMonotonicStartMs);
        if (!link_telemetry_store_record(
                &_telemetry, elapsed, &measurement)) {
            [self failWithStatus:
                @"Could not record live telemetry sample"];
            return NO;
        }

        LinkTelemetrySample recorded;
        if (_recorder.started && !_recorder.finished &&
            link_telemetry_store_latest(
                &_telemetry, event->sample.pid, &recorded) &&
            !link_telemetry_recorder_record_sample_named(
                &_recorder, &recorded,
                link_telemetry_store_is_favourite(
                    &_telemetry, event->sample.pid),
                link_obd2_pid_name(event->sample.pid),
                link_obd2_unit_name(event->sample.unit))) {
            [self failWithStatus:
                @"Could not append session recording"];
            return NO;
        }

        self.ready = YES;
        self.statusText = _simulated
            ? _simulatedLiveStatusText : _liveStatusText;
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_NO_DATA:
        self.statusText =
            @"Live OBD-II data; one PID returned no data";
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_UNSUPPORTED:
        self.statusText =
            @"Live OBD-II data; one advertised sub-field is unavailable";
        break;
    }

    id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
    if ([delegate respondsToSelector:
            @selector(linkDiagnosticsController:didReceiveFlowEvent:)]) {
        [delegate linkDiagnosticsController:self didReceiveFlowEvent:event];
    }
    [self notifyDelegate];
    return YES;
}

- (void)driveDiagnosticFlow
{
    const BOOL transportReady = _simulated
        ? (_sessionInitialized &&
           link_elm327_session_is_connected(&_session))
        : _provider.isReady;

    if (!_sessionInitialized || !transportReady ||
        _flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED ||
        _manufacturerExtensionActive) {
        return;
    }

    LinkDiagnosticFlowAction action;
    LinkDiagnosticFlowResult result = link_diagnostic_flow_next_action(
        &_flow, LinkAppleMonotonicMilliseconds(), &action);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        NSString *reason = LinkAppleStringFromCString(
            link_diagnostic_flow_result_name(result));
        [self failWithStatus:[NSString stringWithFormat:
            @"Shared diagnostic flow failed: %@", reason]];
        return;
    }

    switch (action.kind) {
    case LINK_DIAGNOSTIC_FLOW_ACTION_NONE:
        return;

    case LINK_DIAGNOSTIC_FLOW_ACTION_SEND_COMMAND:
        if (_flow.stage == LINK_DIAGNOSTIC_FLOW_INITIALIZING) {
            self.statusText = _simulated
                ? @"Initialising simulated ELM327 adapter"
                : @"Initialising ELM327 adapter";
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_RESTORING_AFTER_MANUFACTURER) {
            self.statusText =
                @"Restoring standard OBD-II adapter channel";
        } else if (_flow.stage == LINK_DIAGNOSTIC_FLOW_DISCOVERING_PIDS) {
            self.statusText = _flow.supported_pid_base == 0U
                ? @"Checking standard OBD-II capabilities"
                : [NSString stringWithFormat:
                    @"Checking OBD-II PID block 0x%02X",
                    (unsigned int)_flow.supported_pid_base];
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_READING_STANDARD_VIN) {
            self.statusText = _standardVINStatusText;
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS) {
            self.faultScanStatusText =
                @"Scanning stored, pending and permanent OBD-II faults";
            self.statusText =
                @"Scanning stored OBD-II fault codes";
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS) {
            self.statusText =
                @"Scanning pending OBD-II fault codes";
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS) {
            self.statusText =
                @"Scanning permanent OBD-II fault codes";
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_READING_LIVE) {
            self.statusText = _simulated
                ? _simulatedLiveStatusText : _liveStatusText;
        }
        [self notifyDelegate];
        (void)[self beginCommand:action.command
                         timeout:action.timeout_ms];
        return;

    case LINK_DIAGNOSTIC_FLOW_ACTION_WAIT: {
        const uint64_t waitMs =
            action.wait_ms > 60000U ? 60000U : action.wait_ms;
        const NSUInteger generation = _pollGeneration;
        dispatch_after(
            dispatch_time(
                DISPATCH_TIME_NOW,
                (int64_t)waitMs * NSEC_PER_MSEC),
            dispatch_get_main_queue(), ^{
                if (generation == self->_pollGeneration)
                    [self driveDiagnosticFlow];
            });
        return;
    }

    case LINK_DIAGNOSTIC_FLOW_ACTION_MANUFACTURER_EXTENSION: {
        id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
        if (![delegate respondsToSelector:
                @selector(linkDiagnosticsControllerBeginManufacturerExtension:)]) {
            [self failWithStatus:
                @"Manufacturer extension requested without a product handler"];
            return;
        }
        _manufacturerExtensionActive = YES;
        [delegate linkDiagnosticsControllerBeginManufacturerExtension:self];
        return;
    }

    case LINK_DIAGNOSTIC_FLOW_ACTION_READY:
        self.ready = YES;
        [self setSharedStatus:
            _flow.scheduler.count == 0U
                ? @"Connected; no supported dashboard PIDs were advertised"
                : (_simulated
                    ? _simulatedLiveStatusText
                    : _liveStatusText)];
        return;

    case LINK_DIAGNOSTIC_FLOW_ACTION_FAILED:
        [self failWithStatus:
            @"Shared diagnostic flow entered the failed state"];
        return;
    }
}

- (BOOL)completeManufacturerExtensionRestoringAdapter:(BOOL)restore
{
    if (!_manufacturerExtensionActive) return NO;

    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;
    _flow.config.restore_adapter_after_manufacturer_extension = restore;
    LinkDiagnosticFlowResult result =
        link_diagnostic_flow_resume_after_manufacturer(&_flow);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        [self failWithStatus:
            @"Could not resume shared diagnostic flow after manufacturer extension"];
        return NO;
    }

    if (restore)
        [self setSharedStatus:
            @"Restoring standard OBD-II adapter channel"];
    [self driveDiagnosticFlow];
    return YES;
}

- (void)failWithStatus:(NSString *)status
{
    link_diagnostic_flow_fail(
        &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_INVALID_STATE);
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;
    self.ready = NO;
    [self setSharedStatus:status];
}

- (void)updateStatusText:(NSString *)status
{
    [self setSharedStatus:status];
}

- (void)setVehicleIdentifier:(const char *)vehicleIdentifier
{
    link_telemetry_session_metadata_set_vehicle(
        &_sessionMetadata, vehicleIdentifier);
}

- (NSUInteger)recordedSampleCount
{
    uint64_t total =
        link_telemetry_store_total_sample_count(&_telemetry);
    return total > (uint64_t)NSUIntegerMax
        ? NSUIntegerMax : (NSUInteger)total;
}

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit
{
    if (limit == 0U) return @[];

    NSMutableArray<NSNumber *> *values =
        [[NSMutableArray alloc] initWithCapacity:limit];
    const size_t count =
        link_telemetry_store_history_count(&_telemetry);

    for (size_t reverseIndex = count;
         reverseIndex > 0U && values.count < limit;
         --reverseIndex) {
        LinkTelemetrySample sample;
        if (!link_telemetry_store_history_at(
                &_telemetry, reverseIndex - 1U, &sample) ||
            sample.measurement.pid != pid) {
            continue;
        }
        [values insertObject:@(sample.measurement.value) atIndex:0U];
    }
    return values;
}

- (BOOL)favouriteForPID:(uint8_t)pid
{
    return link_telemetry_store_is_favourite(&_telemetry, pid);
}

- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid
{
    link_telemetry_store_set_favourite(&_telemetry, pid, favourite);
    [self notifyDelegate];
}

- (nullable NSString *)csvSnapshot
{
    if (_sessionCSV.length == 0U) return nil;
    return [[NSString alloc]
        initWithData:[_sessionCSV copy]
        encoding:NSUTF8StringEncoding];
}

@end
