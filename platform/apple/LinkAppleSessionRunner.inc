// SPDX-License-Identifier: GPL-3.0-or-later
/* Internal implementation include; compiled through LinkDiagnosticsController.m. */

NS_ASSUME_NONNULL_BEGIN

static uint64_t LinkAppleSessionRunnerMonotonicMilliseconds(void)
{
    NSTimeInterval uptime = NSProcessInfo.processInfo.systemUptime;
    if (uptime <= 0.0) return 0U;
    const double milliseconds = uptime * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}

@interface LinkAppleSessionRunner ()
- (BOOL)startWithTransport:(LinkTransport)transport connectTransport:(BOOL)connect;
- (void)startTickTimer;
- (void)stopTickTimer;
- (void)publishSessionEvent:(const LinkElm327Session *)session;
@end

static void LinkAppleSessionRunnerEvent(
    void *context,
    const LinkElm327Session *session)
{
    LinkAppleSessionRunner *runner =
        (__bridge LinkAppleSessionRunner *)context;
    if (runner == nil || session == NULL) return;
    [runner publishSessionEvent:session];
}

@implementation LinkAppleSessionRunner {
    __weak LinkBLETransport *_provider;
    LinkElm327Session _session;
    LinkElm327Simulator _simulator;
    BOOL _initialized;
    dispatch_source_t _tickTimer;
}

- (instancetype)initWithProvider:(LinkBLETransport *)provider
{
    self = [super init];
    if (self == nil) return nil;
    _provider = provider;
    return self;
}

- (void)dealloc
{
    self.delegate = nil;
    [self disconnect];
}

- (BOOL)isInitialized { return _initialized; }

- (BOOL)isConnected
{
    return _initialized && link_elm327_session_is_connected(&_session);
}

- (LinkElm327SessionStatus)status
{
    return _initialized ? _session.status : LINK_ELM327_SESSION_IDLE;
}

- (LinkElm327Result)elmResult
{
    return _initialized ? _session.elm_result : LINK_ELM327_RESULT_OK;
}

- (BOOL)needsResync
{
    return _initialized && _session.needs_resync;
}

- (BOOL)startWithTransport:(LinkTransport)transport connectTransport:(BOOL)connect
{
    if (_initialized) [self disconnect];
    if (!link_transport_is_valid(&transport) ||
        !link_elm327_session_init(
            &_session, &transport, LinkAppleSessionRunnerEvent,
            (__bridge void *)self)) {
        return NO;
    }

    _initialized = YES;
    if (connect &&
        link_elm327_session_connect(&_session) != LINK_TRANSPORT_OK) {
        _initialized = NO;
        link_elm327_session_deinit(&_session);
        return NO;
    }

    [self startTickTimer];
    return YES;
}

- (BOOL)startReal
{
    LinkBLETransport *provider = _provider;
    if (provider == nil) return NO;
    return [self startWithTransport:LinkBLETransportMakeCTransport(provider)
                   connectTransport:NO];
}

- (BOOL)startSimulatedWithAdapterIdentifier:(const char *)adapterIdentifier
                                        vin:(const char *)vin
                            customResponder:
                                (LinkElm327SimulatorCustomResponderFn)responder
                                    context:(void *)context
{
    LinkElm327SimulatorConfig config = LINK_ELM327_SIMULATOR_CONFIG_INIT;
    config.adapter_identifier = adapterIdentifier;
    config.vin = vin;
    config.custom_responder = responder;
    config.custom_context = context;
    link_elm327_simulator_init(&_simulator, &config);
    return [self startWithTransport:link_elm327_simulator_transport(&_simulator)
                   connectTransport:YES];
}

- (void)disconnect
{
    [self stopTickTimer];
    if (!_initialized) return;
    _initialized = NO;
    link_elm327_session_disconnect(&_session);
    link_elm327_session_deinit(&_session);
}

- (void)invalidate
{
    [self stopTickTimer];
    if (!_initialized) return;
    _initialized = NO;
    link_elm327_session_deinit(&_session);
}

- (LinkElm327SessionOpResult)beginCommand:(const char *)command
                                      now:(uint64_t)nowMs
                                  timeout:(uint64_t)timeoutMs
{
    if (!_initialized || command == NULL)
        return LINK_ELM327_SESSION_OP_NOT_CONNECTED;
    return link_elm327_session_begin(&_session, command, nowMs, timeoutMs);
}

- (LinkElm327SessionOpResult)beginResynchronizationAt:(uint64_t)nowMs
                                               timeout:(uint64_t)timeoutMs
{
    if (!_initialized) return LINK_ELM327_SESSION_OP_NOT_CONNECTED;
    return link_elm327_session_begin_resynchronization(
        &_session, nowMs, timeoutMs);
}

- (const LinkElm327Response *)response
{
    return _initialized ? link_elm327_session_response(&_session) : NULL;
}

- (const char *)currentCommand
{
    return _initialized ? _session.parser.command : NULL;
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

    __weak LinkAppleSessionRunner *weakSelf = self;
    dispatch_source_set_event_handler(_tickTimer, ^{
        LinkAppleSessionRunner *strongSelf = weakSelf;
        if (strongSelf == nil || !strongSelf->_initialized) return;
        (void)link_elm327_session_tick(
            &strongSelf->_session,
            LinkAppleSessionRunnerMonotonicMilliseconds());
    });
    dispatch_resume(_tickTimer);
}

- (void)stopTickTimer
{
    if (_tickTimer == nil) return;
    dispatch_source_cancel(_tickTimer);
    _tickTimer = nil;
}

- (void)publishSessionEvent:(const LinkElm327Session *)session
{
    (void)session;
    id<LinkAppleSessionRunnerDelegate> delegate = self.delegate;
    if (delegate != nil) [delegate linkAppleSessionRunnerDidUpdate:self];
}

@end

NS_ASSUME_NONNULL_END
