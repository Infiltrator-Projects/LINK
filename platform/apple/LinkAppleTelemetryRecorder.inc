// SPDX-License-Identifier: GPL-3.0-or-later
/* Internal implementation include; compiled through LinkDiagnosticsController.m. */

NS_ASSUME_NONNULL_BEGIN

static uint64_t LinkAppleTelemetryMonotonicMilliseconds(void)
{
    NSTimeInterval uptime = NSProcessInfo.processInfo.systemUptime;
    if (uptime <= 0.0) return 0U;
    const double milliseconds = uptime * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}

static uint64_t LinkAppleTelemetryElapsedMilliseconds(uint64_t startedMs)
{
    const uint64_t nowMs = LinkAppleTelemetryMonotonicMilliseconds();
    return nowMs >= startedMs ? nowMs - startedMs : 0U;
}

static uint64_t LinkAppleTelemetryEpochMilliseconds(void)
{
    NSTimeInterval seconds = [NSDate date].timeIntervalSince1970;
    if (seconds <= 0.0) return 0U;
    const double milliseconds = seconds * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}

static bool LinkAppleTelemetryAppendCSV(void *context, const char *bytes, size_t length)
{
    if (context == NULL || bytes == NULL) return false;
    NSMutableData *data = (__bridge NSMutableData *)context;
    [data appendBytes:bytes length:length];
    return true;
}

@interface LinkAppleTelemetryRecorder ()
- (void)rewriteCurrentSessionMetadataKey:(NSString *)key value:(const char *)value;
- (void)recordNativeStreamEvent:(LinkMercedesMeStreamEventKind)kind
                          bytes:(const uint8_t *)bytes
                           size:(size_t)size;
@end

static void LinkAppleTelemetryNativeStreamEvent(
    void *context,
    LinkMercedesMeStreamEventKind kind,
    const uint8_t *bytes,
    size_t size)
{
    LinkAppleTelemetryRecorder *recorder =
        (__bridge LinkAppleTelemetryRecorder *)context;
    if (recorder == nil) return;
    [recorder recordNativeStreamEvent:kind bytes:bytes size:size];
}

@implementation LinkAppleTelemetryRecorder {
    NSString *_productSlug;
    LinkTelemetryStore _telemetry;
    LinkResponderTelemetryStore _responderTelemetry;
    LinkStructuredTelemetryStore _structuredTelemetry;
    LinkTelemetryRecorder _recorder;
    LinkTelemetrySessionMetadata _sessionMetadata;
    LinkMercedesMeStreamParser _nativeStreamParser;
    NSMutableData *_sessionCSV;
    NSUInteger _currentSessionCSVStart;
    uint64_t _sessionMonotonicStartMs;
    NSString *_lastRecordedTransportStatus;
}

- (instancetype)initWithProductSlug:(NSString *)productSlug
{
    self = [super init];
    if (self == nil) return nil;
    _productSlug = [productSlug copy];
    link_telemetry_store_init(&_telemetry);
    link_responder_telemetry_store_init(&_responderTelemetry);
    link_structured_telemetry_store_init(&_structuredTelemetry);
    link_telemetry_recorder_init(&_recorder);
    link_mercedes_me_stream_parser_init(&_nativeStreamParser);
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
    [self finish];
}

- (BOOL)prepareForStart
{
    if (_recorder.started && !_recorder.finished) [self finish];

    link_telemetry_store_clear_samples(&_telemetry);
    link_responder_telemetry_store_clear(&_responderTelemetry);
    link_structured_telemetry_store_clear(&_structuredTelemetry);
    link_telemetry_recorder_init(&_recorder);
    link_mercedes_me_stream_parser_init(&_nativeStreamParser);
    _sessionMonotonicStartMs = LinkAppleTelemetryMonotonicMilliseconds();
    link_telemetry_session_metadata_init(
        &_sessionMetadata, LinkAppleTelemetryEpochMilliseconds(), NULL, NULL);
    _lastRecordedTransportStatus = nil;

    const BOOL continuingEvidence = _sessionCSV.length != 0U;
    _currentSessionCSVStart = _sessionCSV.length;
    const bool started = continuingEvidence
        ? link_telemetry_recorder_continue(
            &_recorder, &_sessionMetadata, _productSlug.UTF8String,
            LinkAppleTelemetryAppendCSV, (__bridge void *)_sessionCSV)
        : link_telemetry_recorder_begin(
            &_recorder, &_sessionMetadata, _productSlug.UTF8String,
            LinkAppleTelemetryAppendCSV, (__bridge void *)_sessionCSV);
    if (!started) [_sessionCSV setLength:_currentSessionCSVStart];
    return started ? YES : NO;
}

- (void)finish
{
    if (!_recorder.started || _recorder.finished) return;
    const uint64_t endedEpochMs = LinkAppleTelemetryEpochMilliseconds();
    link_telemetry_session_metadata_finish(&_sessionMetadata, endedEpochMs);
    (void)link_telemetry_recorder_finish(&_recorder, endedEpochMs);
}

- (void)rewriteCurrentSessionMetadataKey:(NSString *)key value:(const char *)value
{
    if (key.length == 0U || value == NULL || value[0] == '\0' ||
        _currentSessionCSVStart >= _sessionCSV.length) return;

    NSString *identifier = [NSString stringWithUTF8String:value];
    if (identifier.length == 0U) return;
    NSString *escaped = [identifier stringByReplacingOccurrencesOfString:@"\""
                                                               withString:@"\"\""];
    NSData *needle = [[NSString stringWithFormat:@"# %@,\"\"\n", key]
        dataUsingEncoding:NSUTF8StringEncoding];
    NSData *replacement = [[NSString stringWithFormat:
        @"# %@,\"%@\"\n", key, escaped]
        dataUsingEncoding:NSUTF8StringEncoding];
    NSRange range = NSMakeRange(
        _currentSessionCSVStart, _sessionCSV.length - _currentSessionCSVStart);
    NSRange match = [_sessionCSV rangeOfData:needle options:0 range:range];
    if (match.location != NSNotFound) {
        [_sessionCSV replaceBytesInRange:match
                               withBytes:replacement.bytes
                                  length:replacement.length];
    }
}

- (void)setAdapterIdentifier:(const char *)identifier
{
    if (identifier == NULL || identifier[0] == '\0') return;
    link_telemetry_session_metadata_set_adapter(&_sessionMetadata, identifier);
    [self rewriteCurrentSessionMetadataKey:@"adapter_identifier" value:identifier];
}

- (void)setVehicleIdentifier:(const char *)vehicleIdentifier
{
    if (vehicleIdentifier == NULL || vehicleIdentifier[0] == '\0') return;
    link_telemetry_session_metadata_set_vehicle(
        &_sessionMetadata, vehicleIdentifier);
    [self rewriteCurrentSessionMetadataKey:@"vehicle_identifier"
                                     value:vehicleIdentifier];
}

- (void)setOBDProtocolText:(NSString *)protocolText
{
    if (protocolText.length == 0U) return;
    link_telemetry_session_metadata_set_obd_protocol(
        &_sessionMetadata, protocolText.UTF8String);
    [self rewriteCurrentSessionMetadataKey:@"obd_protocol"
                                     value:protocolText.UTF8String];
}

- (void)recordTransportStateName:(NSString *)stateName
                      statusText:(NSString *)statusText
{
    if (!_recorder.started || _recorder.finished) return;
    NSString *recordingKey = [NSString stringWithFormat:
        @"%@|%@", stateName != nil ? stateName : @"",
        statusText != nil ? statusText : @""];
    if ([_lastRecordedTransportStatus isEqualToString:recordingKey]) return;
    _lastRecordedTransportStatus = recordingKey;
    (void)link_telemetry_recorder_record_response_named(
        &_recorder,
        LinkAppleTelemetryElapsedMilliseconds(_sessionMonotonicStartMs),
        "BLE", stateName.UTF8String, statusText.UTF8String);
}

- (void)recordNativeTransportBytes:(const uint8_t *)data size:(size_t)size
{
    if (data == NULL || size == 0U || !_recorder.started || _recorder.finished)
        return;

    NSMutableString *hex =
        [[NSMutableString alloc] initWithCapacity:(NSUInteger)size * 2U];
    for (size_t index = 0U; index < size; ++index)
        [hex appendFormat:@"%02X", (unsigned int)data[index]];
    (void)link_telemetry_recorder_record_response_named(
        &_recorder,
        LinkAppleTelemetryElapsedMilliseconds(_sessionMonotonicStartMs),
        "NATIVE_RX", "bytes", hex.UTF8String);

    (void)link_mercedes_me_stream_parser_feed(
        &_nativeStreamParser, data, size,
        LinkAppleTelemetryNativeStreamEvent, (__bridge void *)self);
}

- (void)recordNativeStreamEvent:(LinkMercedesMeStreamEventKind)kind
                          bytes:(const uint8_t *)bytes
                           size:(size_t)size
{
    if (!_recorder.started || _recorder.finished) return;
    const char *name;
    switch (kind) {
    case LINK_MERCEDES_ME_STREAM_RECORD: name = "NATIVE_RECORD"; break;
    case LINK_MERCEDES_ME_STREAM_NACK: name = "NATIVE_NACK"; break;
    case LINK_MERCEDES_ME_STREAM_OVERFLOW: name = "NATIVE_OVERFLOW"; break;
    default: return;
    }

    NSMutableString *hex =
        [[NSMutableString alloc] initWithCapacity:(NSUInteger)size * 2U];
    for (size_t index = 0U; bytes != NULL && index < size; ++index)
        [hex appendFormat:@"%02X", (unsigned int)bytes[index]];
    (void)link_telemetry_recorder_record_response_named(
        &_recorder,
        LinkAppleTelemetryElapsedMilliseconds(_sessionMonotonicStartMs),
        name, "bytes", hex.UTF8String);
}

- (BOOL)recordTranscriptCommand:(const char *)command
                     resultCode:(uint32_t)resultCode
                     resultName:(const char *)resultName
                   responseText:(const char *)responseText
{
    const uint64_t elapsed =
        LinkAppleTelemetryElapsedMilliseconds(_sessionMonotonicStartMs);
    (void)link_telemetry_store_record_transcript(
        &_telemetry, elapsed, command,
        resultCode, responseText);
    if (!_recorder.started || _recorder.finished) return YES;
    return link_telemetry_recorder_record_response_named(
        &_recorder, elapsed, command, resultName, responseText) ? YES : NO;
}

- (nullable NSString *)recordFlowEvent:(const LinkDiagnosticFlowEvent *)event
{
    if (event == NULL) return @"Could not record live telemetry sample";
    const uint64_t elapsed =
        LinkAppleTelemetryElapsedMilliseconds(_sessionMonotonicStartMs);

    if (event->kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE) {
        LinkTelemetryMeasurement measurement = {
            .pid = event->sample.pid,
            .value = event->sample.value,
            .unit = event->sample.unit
        };
        if (!link_telemetry_store_record(&_telemetry, elapsed, &measurement))
            return @"Could not record live telemetry sample";

        bool recordedAttributedSample = false;
        for (size_t responderIndex = 0U;
             responderIndex < event->responder_samples.count;
             ++responderIndex) {
            const LinkObd2ResponderSample *responder =
                &event->responder_samples.samples[responderIndex];
            if (!responder->responder_id_available) continue;
            LinkTelemetryMeasurement attributed = {
                .pid = responder->sample.pid,
                .value = responder->sample.value,
                .unit = responder->sample.unit
            };
            if (!link_responder_telemetry_store_record(
                    &_responderTelemetry, elapsed,
                    responder->responder_id, responder->extended_id,
                    &attributed))
                return @"Could not record responder-attributed telemetry sample";
            recordedAttributedSample = true;

            const size_t historyCount =
                link_responder_telemetry_store_history_count(&_responderTelemetry);
            LinkResponderTelemetrySample recordedResponder;
            if (_recorder.started && !_recorder.finished &&
                (historyCount == 0U ||
                 !link_responder_telemetry_store_history_at(
                    &_responderTelemetry, historyCount - 1U,
                    &recordedResponder) ||
                 !link_telemetry_recorder_record_responder_sample_named(
                    &_recorder, &recordedResponder,
                    link_telemetry_store_is_favourite(
                        &_telemetry, responder->sample.pid),
                    link_obd2_pid_name(responder->sample.pid),
                    link_obd2_unit_name(responder->sample.unit))))
                return @"Could not append responder-attributed session recording";
        }

        LinkTelemetrySample recorded;
        if (!recordedAttributedSample &&
            _recorder.started && !_recorder.finished &&
            link_telemetry_store_latest(&_telemetry, event->sample.pid, &recorded) &&
            !link_telemetry_recorder_record_sample_named(
                &_recorder, &recorded,
                link_telemetry_store_is_favourite(&_telemetry, event->sample.pid),
                link_obd2_pid_name(event->sample.pid),
                link_obd2_unit_name(event->sample.unit)))
            return @"Could not append session recording";
        return nil;
    }

    if (event->kind == LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED) {
        for (size_t responderIndex = 0U;
             responderIndex < event->responder_decoded.count;
             ++responderIndex) {
            const LinkObd2ResponderDecodedPid *responder =
                &event->responder_decoded.entries[responderIndex];
            if (responder->decoded.definition == NULL) continue;
            if (!link_structured_telemetry_store_record(
                    &_structuredTelemetry, elapsed,
                    responder->responder_id_available,
                    responder->responder_id, responder->extended_id,
                    &responder->decoded))
                return @"Could not record structured live telemetry sample";

            const size_t historyCount =
                link_structured_telemetry_store_history_count(&_structuredTelemetry);
            LinkStructuredTelemetrySample recordedStructured;
            if (_recorder.started && !_recorder.finished &&
                (historyCount == 0U ||
                 !link_structured_telemetry_store_history_at(
                    &_structuredTelemetry, historyCount - 1U,
                    &recordedStructured) ||
                 !link_telemetry_recorder_record_structured_pid_named(
                    &_recorder, &recordedStructured,
                    link_telemetry_store_is_favourite(
                        &_telemetry, responder->decoded.definition->pid),
                    responder->decoded.definition->name != NULL
                        ? responder->decoded.definition->name
                        : link_obd2_pid_name(responder->decoded.definition->pid))))
                return @"Could not append structured SAE session recording";
        }
        return nil;
    }

    return nil;
}

- (BOOL)latestStructuredSampleForPID:(uint8_t)pid
                              sample:(LinkStructuredTelemetrySample *)sample
{
    const size_t count =
        link_structured_telemetry_store_history_count(&_structuredTelemetry);
    if (sample == NULL) return NO;
    for (size_t index = count; index != 0U; --index) {
        LinkStructuredTelemetrySample candidate;
        if (link_structured_telemetry_store_history_at(
                &_structuredTelemetry, index - 1U, &candidate) &&
            candidate.decoded.definition != NULL &&
            candidate.decoded.definition->pid == pid) {
            *sample = candidate;
            return YES;
        }
    }
    return NO;
}

- (NSUInteger)recordedSampleCount
{
    uint64_t total = link_telemetry_store_total_sample_count(&_telemetry);
    const uint64_t structured =
        link_structured_telemetry_store_total_sample_count(&_structuredTelemetry);
    total = UINT64_MAX - total < structured ? UINT64_MAX : total + structured;
    return total > (uint64_t)NSUIntegerMax ? NSUIntegerMax : (NSUInteger)total;
}

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit
{
    if (limit == 0U) return @[];
    NSMutableArray<NSNumber *> *values =
        [[NSMutableArray alloc] initWithCapacity:limit];
    const size_t count = link_telemetry_store_history_count(&_telemetry);
    for (size_t reverseIndex = count;
         reverseIndex > 0U && values.count < limit; --reverseIndex) {
        LinkTelemetrySample sample;
        if (!link_telemetry_store_history_at(
                &_telemetry, reverseIndex - 1U, &sample) ||
            sample.measurement.pid != pid) continue;
        [values insertObject:@(sample.measurement.value) atIndex:0U];
    }
    return values;
}

- (NSArray<NSNumber *> *)observedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                      extendedID:(BOOL)extendedID
{
    const uint32_t maximumIdentifier = extendedID
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (responderCANIdentifier > maximumIdentifier) return @[];

    bool observed[256] = {false};
    const size_t count =
        link_responder_telemetry_store_history_count(&_responderTelemetry);
    for (size_t index = 0U; index < count; ++index) {
        LinkResponderTelemetrySample sample;
        if (!link_responder_telemetry_store_history_at(
                &_responderTelemetry, index, &sample) ||
            sample.responder_id != responderCANIdentifier ||
            sample.extended_id != extendedID) continue;
        observed[sample.measurement.pid] = true;
    }

    const size_t structuredCount =
        link_structured_telemetry_store_history_count(&_structuredTelemetry);
    for (size_t index = 0U; index < structuredCount; ++index) {
        LinkStructuredTelemetrySample sample;
        if (!link_structured_telemetry_store_history_at(
                &_structuredTelemetry, index, &sample) ||
            !sample.responder_id_available ||
            sample.responder_id != responderCANIdentifier ||
            sample.extended_id != extendedID ||
            sample.decoded.definition == NULL) continue;
        observed[sample.decoded.definition->pid] = true;
    }

    NSMutableArray<NSNumber *> *pids = [[NSMutableArray alloc] init];
    for (NSUInteger pid = 0U; pid < 256U; ++pid)
        if (observed[pid]) [pids addObject:@(pid)];
    return [pids copy];
}

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                     responderCANIdentifier:(uint32_t)responderCANIdentifier
                                  extendedID:(BOOL)extendedID
                                       limit:(NSUInteger)limit
{
    if (limit == 0U) return @[];
    const uint32_t maximumIdentifier = extendedID
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (responderCANIdentifier > maximumIdentifier) return @[];

    NSMutableArray<NSNumber *> *values =
        [[NSMutableArray alloc] initWithCapacity:limit];
    const size_t count =
        link_responder_telemetry_store_history_count(&_responderTelemetry);
    for (size_t reverseIndex = count;
         reverseIndex > 0U && values.count < limit; --reverseIndex) {
        LinkResponderTelemetrySample sample;
        if (!link_responder_telemetry_store_history_at(
                &_responderTelemetry, reverseIndex - 1U, &sample) ||
            sample.measurement.pid != pid ||
            sample.responder_id != responderCANIdentifier ||
            sample.extended_id != extendedID) continue;
        [values insertObject:@(sample.measurement.value) atIndex:0U];
    }
    return values;
}

- (BOOL)favouriteForPID:(uint8_t)pid
{
    return link_telemetry_store_is_favourite(&_telemetry, pid) ? YES : NO;
}

- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid
{
    link_telemetry_store_set_favourite(&_telemetry, pid, favourite);
}

- (nullable NSData *)csvDataSnapshot
{
    return _sessionCSV.length == 0U ? nil : [_sessionCSV copy];
}

- (nullable NSString *)csvSnapshot
{
    NSData *snapshot = [self csvDataSnapshot];
    return snapshot == nil ? nil
        : [[NSString alloc] initWithData:snapshot encoding:NSUTF8StringEncoding];
}

@end

NS_ASSUME_NONNULL_END
