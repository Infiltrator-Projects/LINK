// SPDX-License-Identifier: GPL-3.0-or-later
#import "LinkDiagnosticsController.h"
#import "LinkAppleSettings.h"
#import "LinkAppleSessionRunner.h"
#import "LinkApplePollingCoordinator.h"
#import "LinkAppleTelemetryRecorder.h"

#import "link/diagnostic_capability.h"
#import "link/dtc_knowledge.h"
#import "link/elm327.h"
#import "link/i18n.h"
#import "link/parameter.h"
#import "link/units.h"
#import "link/mercedes_me_adapter.h"
#import "link/obd2.h"
#import "link/telemetry.h"
#import "link/transport.h"
#import "link/version.h"

#include <stdint.h>

@interface LinkDiagnosticsController () <LinkBLETransportDelegate, LinkAppleSessionRunnerDelegate>
@property(nonatomic, copy, readwrite) NSString *statusText;
@property(nonatomic, copy, readwrite, nullable) NSString *peripheralName;
@property(nonatomic, copy, readwrite, nullable) NSString *adapterIdentifier;
@property(nonatomic, copy, readwrite) NSString *faultScanStatusText;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *storedDTCs;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *pendingDTCs;
@property(nonatomic, copy, readwrite) NSArray<NSString *> *permanentDTCs;
@property(nonatomic, readwrite, getter=isActive) BOOL active;
@property(nonatomic, readwrite, getter=isReady) BOOL ready;
@property(nonatomic, readwrite, getter=isNativeAdapterConnected)
    BOOL nativeAdapterConnected;

- (void)notifyDelegate;
- (void)setSharedStatus:(NSString *)status;
- (BOOL)prepareForStart;
- (void)beginPortableSession;
- (void)recordNativeTransportBytes:(const uint8_t *)data size:(size_t)size;
- (BOOL)beginCommand:(const char *)command timeout:(uint64_t)timeoutMs;
- (void)notifyManufacturerFailure:(NSString *)status;
- (void)recoverManufacturerExtensionAfterFailure:(NSString *)status;
- (void)finishManufacturerRecovery;
- (void)beginLiveRecovery;
- (void)finishLiveRecovery;
- (void)beginDiagnosticFlowAfterSessionStart;
- (void)processCompletedResponse;
- (BOOL)applyFlowEvent:(const LinkDiagnosticFlowEvent *)event;
- (void)applyPollingPreferencesToScheduler;
- (void)driveDiagnosticFlow;
- (LinkMeasurementSystem)resolvedMeasurementSystem;
@end

@implementation LinkDiagnosticsController {
    LinkBLETransport *_provider;
    LinkApplePollingCoordinator *_pollingCoordinator;
    LinkAppleSessionRunner *_sessionRunner;
    LinkAppleTelemetryRecorder *_telemetryRecorder;
    BOOL _simulated;
    BOOL _manufacturerExtensionActive;
    BOOL _manufacturerRecoveryActive;
    BOOL _liveRecoveryActive;
    NSUInteger _consecutiveLiveTimeouts;
    LinkDiagnosticFlow _flow;
    LinkDiagnosticFlowConfig _flowConfig;

    NSUInteger _pollGeneration;

    NSString *_productSlug;
    NSString *_liveStatusText;
    NSString *_simulatedLiveStatusText;
    NSString *_standardVINStatusText;
    BOOL _legacyDiagnosticResponseObserved;
    LinkAppleSettingsStore *_settings;
}

static uint64_t LinkAppleMonotonicMilliseconds(void)
{
    NSTimeInterval uptime = NSProcessInfo.processInfo.systemUptime;
    if (uptime <= 0.0) return 0U;
    const double milliseconds = uptime * 1000.0;
    return milliseconds >= (double)UINT64_MAX
        ? UINT64_MAX : (uint64_t)milliseconds;
}


static NSString *LinkAppleStringFromCString(const char *value)
{
    if (value == NULL) return @"unknown";
    NSString *string = [NSString stringWithUTF8String:value];
    return string != nil ? string : @"unknown";
}

static NSString *LinkAppleBLEStateName(LinkBLETransportState state)
{
    switch (state) {
    case LinkBLETransportStateIdle: return @"idle";
    case LinkBLETransportStateWaitingForBluetooth: return @"waiting";
    case LinkBLETransportStateScanning: return @"scanning";
    case LinkBLETransportStateConnecting: return @"connecting";
    case LinkBLETransportStateDiscovering: return @"discovering";
    case LinkBLETransportStateProbing: return @"probing";
    case LinkBLETransportStateReady: return @"ready";
    case LinkBLETransportStateDisconnected: return @"disconnected";
    case LinkBLETransportStateFailed: return @"failed";
    }
    return @"unknown";
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


static bool LinkAppleFlowIsFaultScan(const LinkDiagnosticFlow *flow)
{
    if (flow == NULL) return false;
    return flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_STORED_DTCS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_PENDING_DTCS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_SCANNING_PERMANENT_DTCS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_READING_READINESS ||
           flow->stage == LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME;
}

static void LinkAppleNativeTransportReceive(
    void *context, const uint8_t *data, size_t size)
{
    LinkDiagnosticsController *controller =
        (__bridge LinkDiagnosticsController *)context;
    if (controller == nil || data == NULL || size == 0U) return;
    [controller recordNativeTransportBytes:data size:size];
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

    link_i18n_init();
    _settings = [[LinkAppleSettingsStore alloc] init];

    _provider = [[LinkBLETransport alloc] init];
    _provider.delegate = self;
    _sessionRunner = [[LinkAppleSessionRunner alloc] initWithProvider:_provider];
    _sessionRunner.delegate = self;
    _telemetryRecorder = [[LinkAppleTelemetryRecorder alloc] initWithProductSlug:_productSlug];
    _pollingCoordinator = [[LinkApplePollingCoordinator alloc] init];

    _statusText = @"Idle";
    _faultScanStatusText = @"Not scanned";
    _storedDTCs = @[];
    _pendingDTCs = @[];
    _permanentDTCs = @[];

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    return self;
}

- (void)dealloc
{
    _provider.delegate = nil;
    _sessionRunner.delegate = nil;
    [_telemetryRecorder finish];
    if (_sessionRunner.isInitialized) {
        [_sessionRunner disconnect];
    } else if (!_simulated) {
        [_provider disconnect];
    }
}

- (NSString *)linkVersionText
{
    return @LINK_VERSION_STRING;
}

- (NSArray<NSString *> *)availableLanguageTags
{
    NSMutableArray<NSString *> *tags = [NSMutableArray array];
    for (size_t index = 0U; index < link_i18n_installed_locale_count(); ++index) {
        const char *tag = link_i18n_installed_locale(index);
        if (tag != NULL && tag[0] != '\0')
            [tags addObject:[NSString stringWithUTF8String:tag]];
    }
    return tags;
}

- (NSArray<NSString *> *)availableLanguageNames
{
    NSMutableArray<NSString *> *names = [NSMutableArray array];
    for (size_t index = 0U; index < link_i18n_installed_locale_count(); ++index) {
        const char *name = link_i18n_installed_locale_name(index);
        [names addObject:name != NULL
            ? [NSString stringWithUTF8String:name] : @"Unknown"];
    }
    return names;
}

- (NSString *)selectedLanguageTag
{
    return _settings.selectedLanguageTag;
}

- (NSString *)effectiveLanguageTag
{
    return self.selectedLanguageTag;
}

- (NSArray<NSString *> *)availableMeasurementSystemKeys
{
    return @[@"metric", @"us-customary"];
}

- (NSArray<NSString *> *)availableMeasurementSystemNames
{
    return @[
        [self localizedTextForKey:@"units.metric"],
        [self localizedTextForKey:@"units.us_customary"]
    ];
}

- (NSString *)selectedMeasurementSystemKey
{
    return _settings.selectedMeasurementSystemKey;
}

- (NSString *)dtcDisplayTextForCode:(NSString *)code
{
    if (code.length == 0U) return @"";

    LinkDtcKnowledge knowledge;
    if (!link_dtc_resolve(code.UTF8String, &knowledge)) return code;

    NSString *normalized = LinkAppleStringFromCString(knowledge.code);
    if (knowledge.definition_known && knowledge.title[0] != '\0') {
        NSString *title = LinkAppleStringFromCString(knowledge.title);
        return [NSString stringWithFormat:@"%@ — %@", normalized, title];
    }

    const char *origin = link_dtc_origin_name(knowledge.origin);
    if (origin != NULL && origin[0] != '\0') {
        return [NSString stringWithFormat:@"%@ — %@", normalized,
            LinkAppleStringFromCString(origin)];
    }
    return normalized;
}

- (NSString *)localizedTextForKey:(NSString *)key
{
    if (key.length == 0U) return @"";
    const char *text = link_i18n_text(key.UTF8String);
    return text != NULL ? [NSString stringWithUTF8String:text] : key;
}

- (void)setSelectedLanguageTag:(NSString *)tag
{
    if ([_settings setSelectedLanguageTag:tag persist:YES])
        [self notifyDelegate];
}

- (void)setSelectedMeasurementSystemKey:(NSString *)key
{
    if ([_settings setSelectedMeasurementSystemKey:key persist:YES])
        [self notifyDelegate];
}

- (LinkMeasurementSystem)resolvedMeasurementSystem
{
    return [_settings resolvedMeasurementSystem];
}

- (double)displayValueForPID:(uint8_t)pid canonicalValue:(double)value
{
    LinkObd2UnitCode unit = LINK_OBD2_UNIT_NONE;
    double display = value;
    const char *label = "";
    if (link_parameter_obd2_expected_unit(pid, &unit) &&
        link_units_convert_obd2(unit, value, [self resolvedMeasurementSystem],
            &display, &label))
        return display;
    return value;
}

- (double)displayTemperatureCelsius:(double)celsius
{
    double display = celsius;
    const char *label = "°C";
    if (link_units_convert_obd2(LINK_OBD2_UNIT_CELSIUS, celsius,
            [self resolvedMeasurementSystem], &display, &label))
        return display;
    return celsius;
}

- (NSString *)displayTemperatureUnit
{
    double ignored = 0.0;
    const char *label = "°C";
    (void)link_units_convert_obd2(LINK_OBD2_UNIT_CELSIUS, 0.0,
        [self resolvedMeasurementSystem], &ignored, &label);
    return label != NULL ? [NSString stringWithUTF8String:label] : @"°C";
}

- (NSArray<NSNumber *> *)displayRecentValuesForPID:(uint8_t)pid
                                             limit:(NSUInteger)limit
{
    NSArray<NSNumber *> *canonical = [self recentValuesForPID:pid limit:limit];
    LinkObd2UnitCode unit = LINK_OBD2_UNIT_NONE;
    if (!link_parameter_obd2_expected_unit(pid, &unit)) return canonical;
    NSMutableArray<NSNumber *> *values =
        [NSMutableArray arrayWithCapacity:canonical.count];
    const LinkMeasurementSystem system = [self resolvedMeasurementSystem];
    for (NSNumber *number in canonical) {
        double display = number.doubleValue;
        const char *label = "";
        if (!link_units_convert_obd2(unit, display, system, &display, &label))
            display = number.doubleValue;
        [values addObject:@(display)];
    }
    return values;
}

- (NSString *)displayUnitForPID:(uint8_t)pid
{
    LinkObd2UnitCode unit = LINK_OBD2_UNIT_NONE;
    if (link_parameter_obd2_expected_unit(pid, &unit)) {
        double ignored = 0.0;
        const char *label = "";
        if (link_units_convert_obd2(
                unit, 0.0, [self resolvedMeasurementSystem],
                &ignored, &label) && label != NULL)
            return [NSString stringWithUTF8String:label];
    }
    const LinkObd2PidDefinition *definition =
        link_obd2_pid_definition(UINT8_C(0x01), pid);
    return definition != NULL && definition->unit != NULL
        ? [NSString stringWithUTF8String:definition->unit] : @"";
}

- (NSArray<NSNumber *> *)displayRangeForPID:(uint8_t)pid
{
    const LinkParameterDefinition *definition =
        link_parameter_obd2_definition(pid);
    LinkObd2UnitCode unit = LINK_OBD2_UNIT_NONE;
    if (definition == NULL ||
        !link_parameter_obd2_expected_unit(pid, &unit))
        return @[];
    double minimum = definition->minimum;
    double maximum = definition->maximum;
    const char *label = "";
    const LinkMeasurementSystem system = [self resolvedMeasurementSystem];
    if (!link_units_convert_obd2(unit, minimum, system, &minimum, &label) ||
        !link_units_convert_obd2(unit, maximum, system, &maximum, &label))
        return @[];
    return @[@(minimum), @(maximum)];
}

- (BOOL)latestStructuredSampleForPID:(uint8_t)pid
                              sample:(LinkStructuredTelemetrySample *)sample
{
    return [_telemetryRecorder latestStructuredSampleForPID:pid sample:sample];
}

- (nullable NSString *)structuredDisplayValueForPID:(uint8_t)pid
{
    LinkStructuredTelemetrySample sample;
    if (![self latestStructuredSampleForPID:pid sample:&sample]) return nil;
    if (sample.decoded.text_available && sample.decoded.text[0] != '\0')
        return [NSString stringWithUTF8String:sample.decoded.text];

    NSMutableArray<NSString *> *parts = [NSMutableArray array];
    const LinkMeasurementSystem system = [self resolvedMeasurementSystem];
    for (size_t index = 0U; index < sample.decoded.signal_count; ++index) {
        const LinkObd2DecodedSignal *signal = &sample.decoded.signals[index];
        double display = signal->value;
        const char *unit = signal->unit != NULL ? signal->unit : "";
        const char *displayUnit = unit;
        LinkObd2Unit decodedUnit = LINK_OBD2_UNIT_NONE;
        if (link_obd2_unit_from_name(unit, &decodedUnit)) {
            (void)link_units_convert_obd2(
                decodedUnit, signal->value, system, &display, &displayUnit);
        }
        [parts addObject:[NSString stringWithFormat:@"%s: %.6g%s%s",
            signal->label != NULL ? signal->label : "Value",
            display,
            displayUnit != NULL && displayUnit[0] != '\0' ? " " : "",
            displayUnit != NULL ? displayUnit : ""]];
    }
    if (parts.count != 0U) return [parts componentsJoinedByString:@" · "];

    NSString *raw = [self structuredRawHexForPID:pid];
    return raw.length != 0U ? [@"RAW " stringByAppendingString:raw] : nil;
}

- (nullable NSString *)structuredRawHexForPID:(uint8_t)pid
{
    LinkStructuredTelemetrySample sample;
    if (![self latestStructuredSampleForPID:pid sample:&sample] ||
        sample.decoded.raw_length == 0U) return nil;
    NSMutableString *raw = [NSMutableString string];
    for (size_t index = 0U; index < sample.decoded.raw_length; ++index) {
        [raw appendFormat:index == 0U ? @"%02X" : @" %02X",
            sample.decoded.raw[index]];
    }
    return [raw copy];
}

static void LinkAppleAppendReadinessMonitor(
    NSMutableArray<NSString *> *rows,
    const char *name,
    bool supported,
    bool incomplete)
{
    if (rows == nil || name == NULL || !supported) return;
    [rows addObject:[NSString stringWithFormat:
        @"%s · %@", name, incomplete ? @"not ready" : @"ready"]];
}

- (NSString *)readinessStatusText
{
    const LinkObd2Readiness *readiness =
        link_diagnostic_flow_readiness(&_flow);
    if (!_flow.readiness_attempted)
        return @"Not collected";
    if (readiness == NULL)
        return @"Unavailable / unsupported";
    return [NSString stringWithFormat:
        @"%@ · %u confirmed DTC%@ · %@ ignition",
        readiness->mil_on ? @"MIL on" : @"MIL off",
        (unsigned int)readiness->confirmed_dtc_count,
        readiness->confirmed_dtc_count == 1U ? @"" : @"s",
        readiness->compression_ignition ? @"compression" : @"spark"];
}

- (NSArray<NSString *> *)readinessMonitorStatus
{
    const LinkObd2Readiness *readiness =
        link_diagnostic_flow_readiness(&_flow);
    if (readiness == NULL) return @[];

    NSMutableArray<NSString *> *rows = [[NSMutableArray alloc] init];
    static const char *continuousNames[3] = {
        "Misfire", "Fuel system", "Comprehensive components"
    };
    for (unsigned int bit = 0U; bit < 3U; ++bit) {
        LinkAppleAppendReadinessMonitor(
            rows, continuousNames[bit],
            (readiness->continuous_supported & (1U << bit)) != 0U,
            (readiness->continuous_incomplete & (1U << bit)) != 0U);
    }

    static const char *sparkNames[8] = {
        "Catalyst", "Heated catalyst", "Evaporative system",
        "Secondary air", "A/C refrigerant", "Oxygen sensor",
        "Oxygen sensor heater", "EGR / VVT"
    };
    static const char *dieselNames[8] = {
        "NMHC catalyst", "NOx / SCR", "Reserved",
        "Boost pressure", "Reserved", "Exhaust gas sensor",
        "Particulate filter", "EGR / VVT"
    };
    const char *const *names =
        readiness->compression_ignition ? dieselNames : sparkNames;
    for (unsigned int bit = 0U; bit < 8U; ++bit) {
        if (strcmp(names[bit], "Reserved") == 0) continue;
        LinkAppleAppendReadinessMonitor(
            rows, names[bit],
            (readiness->noncontinuous_supported & (1U << bit)) != 0U,
            (readiness->noncontinuous_incomplete & (1U << bit)) != 0U);
    }
    return [rows copy];
}

- (NSArray<NSString *> *)freezeFrameContext
{
    size_t count = 0U;
    const LinkObd2Sample *samples =
        link_diagnostic_flow_freeze_frame_samples(&_flow, &count);
    if (samples == NULL || count == 0U) return @[];

    NSMutableArray<NSString *> *rows =
        [[NSMutableArray alloc] initWithCapacity:count];
    for (size_t index = 0U; index < count; ++index) {
        const LinkObd2Sample *sample = &samples[index];
        const char *name = link_obd2_pid_name(sample->pid);
        double displayValue = sample->value;
        const char *unit = "";
        (void)link_units_convert_obd2(
            sample->unit, sample->value, [self resolvedMeasurementSystem],
            &displayValue, &unit);
        NSString *value;
        if (sample->unit == LINK_OBD2_UNIT_NONE ||
            unit == NULL || unit[0] == '\0') {
            value = [NSString stringWithFormat:@"%.2f", displayValue];
        } else {
            value = [NSString stringWithFormat:@"%.2f %s",
                displayValue, unit];
        }
        [rows addObject:[NSString stringWithFormat:
            @"PID 0x%02X · %s · %@",
            (unsigned int)sample->pid,
            name != NULL ? name : "Unknown PID",
            value]];
    }
    return [rows copy];
}

static size_t LinkAppleSupportedPIDCount(const LinkDiagnosticFlow *flow)
{
    size_t count = 0U;
    unsigned int raw;
    if (flow == NULL) return 0U;
    for (raw = 1U; raw <= UINT8_MAX; ++raw) {
        if (link_obd2_pid_set_contains(&flow->supported_pids, (uint8_t)raw))
            ++count;
    }
    return count;
}

- (LinkDiagnosticCapabilityEvidence)diagnosticCapabilityEvidence
{
    LinkDiagnosticCapabilityEvidence evidence = {0};
    evidence.probe_complete =
        _flow.standard_diagnostic_context_complete ||
        self.isReady ||
        _flow.stage == LINK_DIAGNOSTIC_FLOW_FAILED;
    evidence.standard_obd_response =
        _flow.supported_pid_responders.count != 0U ||
        LinkAppleSupportedPIDCount(&_flow) != 0U ||
        _flow.standard_vin_available ||
        _flow.readiness_available ||
        _flow.stored_dtcs.count != 0U ||
        _flow.pending_dtcs.count != 0U ||
        _flow.permanent_dtcs.count != 0U;
    evidence.legacy_diagnostic_response =
        _legacyDiagnosticResponseObserved ? true : false;
    return evidence;
}

- (NSString *)diagnosticCapabilityText
{
    const LinkDiagnosticCapabilityEvidence evidence =
        [self diagnosticCapabilityEvidence];
    const LinkDiagnosticTier tier =
        link_diagnostic_capability_classify(&evidence);
    return [NSString stringWithUTF8String:link_diagnostic_tier_name(tier)];
}

- (NSString *)diagnosticCapabilityDetailText
{
    const LinkDiagnosticCapabilityEvidence evidence =
        [self diagnosticCapabilityEvidence];
    const LinkDiagnosticTier tier =
        link_diagnostic_capability_classify(&evidence);
    return [NSString stringWithUTF8String:link_diagnostic_tier_summary(tier)];
}

- (NSString *)standardResponderSummary
{
    return [NSString stringWithFormat:@"%zu physical responder%@",
        _flow.supported_pid_responders.count,
        _flow.supported_pid_responders.count == 1U ? @"" : @"s"];
}

- (NSString *)supportedPIDSummary
{
    const size_t count = LinkAppleSupportedPIDCount(&_flow);
    return [NSString stringWithFormat:@"%zu advertised PID%@",
        count, count == 1U ? @"" : @"s"];
}

- (NSString *)standardVINText
{
    if (_flow.standard_vin_available && _flow.standard_vin[0] != '\0')
        return [NSString stringWithUTF8String:_flow.standard_vin];
    return @"Unavailable / not yet read";
}

- (NSArray<NSString *> *)standardLiveValueRows
{
    NSMutableArray<NSString *> *rows = [NSMutableArray array];
    for (NSUInteger raw = 1U; raw <= UINT8_MAX; ++raw) {
        const uint8_t pid = (uint8_t)raw;
        if (!link_obd2_pid_set_contains(&_flow.supported_pids, pid)) continue;
        const LinkObd2PidDefinition *definition =
            link_obd2_pid_definition(1U, pid);
        const char *name =
            definition != NULL && definition->name != NULL
                ? definition->name : link_obd2_pid_name(pid);
        const char *translatedName =
            name != NULL ? link_i18n_text(name) : "Unknown";
        NSArray<NSNumber *> *history =
            [self displayRecentValuesForPID:pid limit:1U];
        if (history.count != 0U) {
            NSString *unit = [self displayUnitForPID:pid];
            [rows addObject:[NSString stringWithFormat:
                @"PID %02lX · %s — %.3f%@%@",
                (unsigned long)pid,
                translatedName != NULL ? translatedName : "Unknown",
                history.lastObject.doubleValue,
                unit.length != 0U ? @" " : @"",
                unit]];
        } else {
            [rows addObject:[NSString stringWithFormat:
                @"PID %02lX · %s — waiting",
                (unsigned long)pid,
                name != NULL ? name : "Unknown"]];
        }
    }
    return [rows copy];
}

- (void)setLegacyDiagnosticResponseObserved:(BOOL)observed
{
    if (_legacyDiagnosticResponseObserved == observed) return;
    _legacyDiagnosticResponseObserved = observed;
    [self notifyDelegate];
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

- (NSString *)obdProtocolText
{
    const LinkElm327ProtocolDefinition *protocol =
        link_diagnostic_flow_obd_protocol(&_flow);
    if (protocol == NULL) return @"OBD-II protocol not identified";

    NSMutableArray<NSString *> *parts = [NSMutableArray array];
    [parts addObject:LinkAppleStringFromCString(
        link_elm327_protocol_family_name(protocol->family))];

    if (protocol->bit_rate != 0U) {
        const BOOL canFamily =
            protocol->family == LINK_ELM327_PROTOCOL_FAMILY_ISO_15765_4 ||
            protocol->family == LINK_ELM327_PROTOCOL_FAMILY_SAE_J1939 ||
            protocol->family == LINK_ELM327_PROTOCOL_FAMILY_USER_DEFINED;
        if (canFamily) {
            [parts addObject:[NSString stringWithFormat:@"%u kbit/s",
                (unsigned int)(protocol->bit_rate / 1000U)]];
        } else if ((protocol->bit_rate % 1000U) == 0U) {
            [parts addObject:[NSString stringWithFormat:@"%u kbaud",
                (unsigned int)(protocol->bit_rate / 1000U)]];
        } else {
            [parts addObject:[NSString stringWithFormat:@"%.1f kbaud",
                (double)protocol->bit_rate / 1000.0]];
        }
    }

    if (protocol->family == LINK_ELM327_PROTOCOL_FAMILY_ISO_15765_4 ||
        protocol->family == LINK_ELM327_PROTOCOL_FAMILY_SAE_J1939) {
        [parts addObject:protocol->extended_can_id
            ? @"29-bit CAN" : @"11-bit CAN"];
    }
    switch (protocol->init) {
    case LINK_ELM327_PROTOCOL_INIT_NONE:
        break;
    case LINK_ELM327_PROTOCOL_INIT_FIVE_BAUD:
        [parts addObject:@"5-baud init"];
        break;
    case LINK_ELM327_PROTOCOL_INIT_FAST:
        [parts addObject:@"fast init"];
        break;
    }
    if (link_diagnostic_flow_obd_protocol_was_automatic(&_flow))
        [parts addObject:@"auto-selected"];
    return [parts componentsJoinedByString:@" · "];
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

- (BOOL)prepareForStart
{
    _pollGeneration++;
    self.active = YES;
    self.ready = NO;
    self.adapterIdentifier = nil;
    self.nativeAdapterConnected = NO;
    self.faultScanStatusText = @"Waiting for vehicle connection";
    self.storedDTCs = @[];
    self.pendingDTCs = @[];
    self.permanentDTCs = @[];
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;
    _liveRecoveryActive = NO;
    _consecutiveLiveTimeouts = 0U;

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    if (![_telemetryRecorder prepareForStart]) {
        self.active = NO;
        return NO;
    }
    return YES;
}

- (void)start
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self start]; });
        return;
    }
    if (self.active) return;

    _simulated = NO;
    if (![self prepareForStart]) {
        [self setSharedStatus:@"Could not start diagnostic evidence recorder"];
        return;
    }
    self.peripheralName = nil;
    [self notifyDelegate];
    [_provider start];
}

- (void)startWithPeripheralIdentifier:(NSString *)peripheralIdentifier
{
    if (![NSThread isMainThread]) {
        NSString *copy = [peripheralIdentifier copy];
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startWithPeripheralIdentifier:copy];
        });
        return;
    }
    if (self.active) return;

    _simulated = NO;
    if (![self prepareForStart]) {
        [self setSharedStatus:@"Could not start diagnostic evidence recorder"];
        return;
    }
    self.peripheralName = nil;
    [self notifyDelegate];
    [_provider startWithPeripheralIdentifier:peripheralIdentifier];
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
    if (![self prepareForStart]) {
        [self setSharedStatus:@"Could not start simulated evidence recorder"];
        return;
    }
    self.peripheralName = @"Simulated ELM327";

    [self notifyDelegate];
    if (![_sessionRunner startSimulatedWithAdapterIdentifier:adapterIdentifier
                                                        vin:vin
                                            customResponder:responder
                                                    context:context]) {
        [self failWithStatus:@"Failed to connect simulated ELM327 transport"];
        return;
    }
    [self beginDiagnosticFlowAfterSessionStart];
}

- (void)disconnect
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{ [self disconnect]; });
        return;
    }

    _pollGeneration++;
    if (_sessionRunner.isInitialized) {
        [_sessionRunner disconnect];
    } else if (!_simulated) {
        [_provider disconnect];
    }

    [_telemetryRecorder finish];

    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    _manufacturerExtensionActive = NO;
    _manufacturerRecoveryActive = NO;
    _liveRecoveryActive = NO;
    _consecutiveLiveTimeouts = 0U;
    _simulated = NO;
    self.nativeAdapterConnected = NO;
    self.active = NO;
    self.ready = NO;
    [self setSharedStatus:@"Disconnected"];
}

- (void)bleTransportDidUpdate:(LinkBLETransport *)transport
{
    if (_simulated) return;

    self.peripheralName = transport.peripheralName;
    self.adapterIdentifier = transport.adapterIdentifier;
    if (transport.adapterIdentifier != nil)
        [_telemetryRecorder setAdapterIdentifier:transport.adapterIdentifier.UTF8String];

    NSString *stateName = LinkAppleBLEStateName(transport.state);
    NSString *transportStatus = transport.statusText != nil
        ? transport.statusText : @"";
    [_telemetryRecorder recordTransportStateName:stateName statusText:transportStatus];

    if (transport.isReady && transport.isNativeAdapter &&
        !_sessionRunner.isInitialized) {
        if (!self.nativeAdapterConnected) {
            LinkTransport native = LinkBLETransportMakeCTransport(_provider);
            if (link_transport_is_valid(&native) &&
                native.set_receiver != NULL) {
                native.set_receiver(
                    native.context, LinkAppleNativeTransportReceive,
                    (__bridge void *)self);
            }
            self.nativeAdapterConnected = YES;
            self.ready = NO;
            self.faultScanStatusText =
                @"Not available through native Mercedes me capture yet";
            [self setSharedStatus:
                @"Mercedes me Adapter connected · native protocol capture"];
        }
        return;
    }

    if (transport.isReady && !_sessionRunner.isInitialized &&
        !transport.isNativeAdapter) {
        [self beginPortableSession];
        return;
    }

    if (!transport.isReady && self.nativeAdapterConnected) {
        LinkTransport native = LinkBLETransportMakeCTransport(_provider);
        if (link_transport_is_valid(&native) &&
            native.set_receiver != NULL)
            native.set_receiver(native.context, NULL, NULL);
        self.nativeAdapterConnected = NO;
        self.ready = NO;
    }

    if (!transport.isReady &&
        _sessionRunner.isInitialized &&
        transport.state != LinkBLETransportStateProbing) {
        _pollGeneration++;
        [_sessionRunner invalidate];
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        if (_manufacturerExtensionActive)
            [self notifyManufacturerFailure:
                @"Vehicle transport disconnected during manufacturer extension"];
        _manufacturerExtensionActive = NO;
        self.ready = NO;
    }

    if (!_sessionRunner.isInitialized) self.statusText = transport.statusText;
    if (transport.state == LinkBLETransportStateFailed) {
        [_telemetryRecorder finish];
        self.active = NO;
        self.ready = NO;
    }
    [self notifyDelegate];
}

- (void)recordNativeTransportBytes:(const uint8_t *)data size:(size_t)size
{
    if (!self.nativeAdapterConnected) return;
    [_telemetryRecorder recordNativeTransportBytes:data size:size];
}

- (void)beginPortableSession
{
    if (![_sessionRunner startReal]) {
        [self failWithStatus:@"Failed to initialise portable diagnostic session"];
        return;
    }
    [self beginDiagnosticFlowAfterSessionStart];
}

- (void)beginDiagnosticFlowAfterSessionStart
{
    (void)link_diagnostic_flow_init(&_flow, &_flowConfig);
    if (link_diagnostic_flow_start(&_flow) != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        [self failWithStatus:@"Could not start shared diagnostic flow"];
        return;
    }

    [self setSharedStatus:_simulated
        ? @"Initialising simulated ELM327 adapter"
        : @"Initialising ELM327 adapter"];
    [self driveDiagnosticFlow];
}

- (BOOL)beginCommand:(const char *)command timeout:(uint64_t)timeoutMs
{
    if (!_sessionRunner.isInitialized || command == NULL) return NO;

    LinkElm327SessionOpResult result = [_sessionRunner
        beginCommand:command now:LinkAppleMonotonicMilliseconds() timeout:timeoutMs];
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

- (BOOL)beginLiveManufacturerExtension
{
    if (!_sessionRunner.isInitialized || !self.active ||
        _manufacturerExtensionActive || _manufacturerRecoveryActive ||
        _flow.awaiting_response) {
        return NO;
    }

    LinkDiagnosticFlowResult result =
        link_diagnostic_flow_begin_live_manufacturer_extension(&_flow);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        return NO;
    }

    /*
     * Invalidate any delayed scheduler callback that was queued before the
     * product requested this pause.  The portable scheduler state itself is
     * retained and will resume after the manufacturer extension.
     */
    ++_pollGeneration;
    _manufacturerExtensionActive = YES;
    return YES;
}

- (BOOL)registerLiveManufacturerJobWithToken:(uint32_t)token
                        intervalMilliseconds:(uint32_t)intervalMs
                                    priority:(LinkSchedulerPriority)priority
{
    if (!_sessionRunner.isInitialized || !self.active || token == 0U || intervalMs == 0U)
        return NO;
    const LinkDiagnosticFlowResult result =
        link_diagnostic_flow_register_live_manufacturer_job(
            &_flow, token, intervalMs, priority,
            LinkAppleMonotonicMilliseconds() + (uint64_t)intervalMs);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) return NO;
    [self driveDiagnosticFlow];
    return YES;
}

- (BOOL)setLiveManufacturerJobEnabled:(BOOL)enabled token:(uint32_t)token
{
    if (token == 0U) return NO;
    const LinkDiagnosticFlowResult result =
        link_diagnostic_flow_set_live_manufacturer_job_enabled(
            &_flow, token, enabled ? true : false);
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) return NO;
    if (enabled && self.active && !_flow.awaiting_response &&
        !_manufacturerExtensionActive) {
        [self driveDiagnosticFlow];
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
    if (!_manufacturerExtensionActive || !_sessionRunner.isInitialized) return;

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
        [_sessionRunner beginResynchronizationAt:LinkAppleMonotonicMilliseconds()
                                               timeout:UINT64_C(2500)];
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

- (void)beginLiveRecovery
{
    LinkElm327SessionOpResult sessionResult;

    if (_liveRecoveryActive || !_sessionRunner.isInitialized ||
        _flow.stage != LINK_DIAGNOSTIC_FLOW_READING_LIVE) {
        return;
    }

    ++_consecutiveLiveTimeouts;
    if (_consecutiveLiveTimeouts > 3U) {
        self.ready = NO;
        _flow.elm_failure = LINK_ELM327_RESULT_MORE_DATA;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:
            @"Repeated live-data timeouts; reconnect required"];
        return;
    }

    _liveRecoveryActive = YES;
    self.ready = NO;
    sessionResult = [_sessionRunner
        beginResynchronizationAt:LinkAppleMonotonicMilliseconds()
                             timeout:UINT64_C(2500)];
    if (sessionResult != LINK_ELM327_SESSION_OP_OK) {
        _liveRecoveryActive = NO;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:[NSString stringWithFormat:
            @"Live request timed out; adapter resynchronisation could not start: %@",
            LinkAppleStringFromCString(
                link_elm327_session_op_result_name(sessionResult))]];
        return;
    }
    [self setSharedStatus:
        @"Live request timed out; resynchronising adapter"];
}

- (void)finishLiveRecovery
{
    LinkDiagnosticFlowResult result;

    if (!_liveRecoveryActive) return;
    result = link_diagnostic_flow_recover_live_timeout(
        &_flow, LinkAppleMonotonicMilliseconds());
    _liveRecoveryActive = NO;
    if (result != LINK_DIAGNOSTIC_FLOW_RESULT_OK) {
        [self failWithStatus:
            @"Adapter resynchronised but live diagnostic flow could not resume"];
        return;
    }

    [self applyPollingPreferencesToScheduler];
    self.ready = YES;
    [self setSharedStatus:
        @"Live request recovered; continuing diagnostics"];
    [self driveDiagnosticFlow];
}

- (void)linkAppleSessionRunnerDidUpdate:(LinkAppleSessionRunner *)runner
{
    if (runner == nil) return;

    if (runner.status == LINK_ELM327_SESSION_COMPLETE) {
        dispatch_async(
            dispatch_get_main_queue(), ^{ [self processCompletedResponse]; });
        return;
    }

    if (runner.status == LINK_ELM327_SESSION_RESYNCHRONIZED) {
        if (_manufacturerRecoveryActive) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self finishManufacturerRecovery];
            });
        } else if (_liveRecoveryActive) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self finishLiveRecovery];
            });
        }
        return;
    }

    if (runner.status == LINK_ELM327_SESSION_TIMED_OUT) {
        if (_manufacturerExtensionActive) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self recoverManufacturerExtensionAfterFailure:
                    @"Manufacturer diagnostic request timed out"];
            });
            return;
        }
        if (_manufacturerRecoveryActive) {
            _manufacturerRecoveryActive = NO;
            _flow.elm_failure = runner.elmResult;
            link_diagnostic_flow_fail(
                &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
            [self setSharedStatus:
                @"Adapter resynchronisation timed out; reconnect required"];
            return;
        }
        if (_flow.stage == LINK_DIAGNOSTIC_FLOW_READING_LIVE) {
            dispatch_async(dispatch_get_main_queue(), ^{
                [self beginLiveRecovery];
            });
            return;
        }
        if (LinkAppleFlowIsFaultScan(&_flow))
            self.faultScanStatusText =
                @"Fault scan timed out; reconnect required";
        self.ready = NO;
        _flow.elm_failure = runner.elmResult;
        link_diagnostic_flow_fail(
            &_flow, LINK_DIAGNOSTIC_FLOW_RESULT_ELM_ERROR);
        [self setSharedStatus:
            @"Diagnostic request timed out; reconnect to resynchronise"];
        return;
    }

    if (runner.status == LINK_ELM327_SESSION_FAILED) {
        NSString *reason = LinkAppleStringFromCString(
            link_elm327_result_name(runner.elmResult));
        if (_manufacturerExtensionActive && runner.needsResync) {
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
        _flow.elm_failure = runner.elmResult;
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

    if (runner.status == LINK_ELM327_SESSION_CANCELLED) {
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
    const LinkElm327Response *response = [_sessionRunner response];
    if (response == NULL) {
        [self failWithStatus:@"Diagnostic response was unavailable"];
        return;
    }

    if (![_telemetryRecorder
            recordTranscriptCommand:[_sessionRunner currentCommand]
            resultCode:(uint32_t)response->result
            resultName:link_elm327_result_name(response->result)
            responseText:response->text]) {
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

    const LinkDiagnosticFlowStage completedStage = _flow.stage;
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

    /*
     * The live scheduler is built before the final ATH1 live-header command,
     * while product polling preferences can change throughout VIN/module
     * discovery. Re-apply the retained preferences at the exact live-entry
     * boundary so a real vehicle cannot arrive in LIVE with a freshly built
     * scheduler still carrying stale disabled flags. This is deliberately
     * before driveDiagnosticFlow(): the very next action must see the user's
     * current selection.
     */
    if (completedStage == LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS &&
        _flow.stage == LINK_DIAGNOSTIC_FLOW_LIVE) {
        [self applyPollingPreferencesToScheduler];
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
            [_telemetryRecorder setAdapterIdentifier:identifier];
        }
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_PROTOCOL_IDENTIFIED: {
        NSString *protocolText = self.obdProtocolText;
        [_telemetryRecorder setOBDProtocolText:protocolText];
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_STANDARD_VIN:
        if (event->vin_available && event->vin != NULL) {
            [self setVehicleIdentifier:event->vin];
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
            /*
             * The portable flow has constructed its capability-gated live
             * schedule but now continues through readiness/freeze-frame
             * investigation context before declaring the fault workflow done.
             */
            [self applyPollingPreferencesToScheduler];
            self.faultScanStatusText = [NSString stringWithFormat:
                @"Fault inventory complete · %lu stored · %lu pending · %lu permanent · collecting diagnostic context",
                (unsigned long)self.storedDTCs.count,
                (unsigned long)self.pendingDTCs.count,
                (unsigned long)self.permanentDTCs.count];
            break;
        }
        if (event->became_ready) self.ready = YES;
        break;
    }

    case LINK_DIAGNOSTIC_FLOW_EVENT_READINESS:
        self.faultScanStatusText = _flow.readiness_available
            ? @"Fault inventory complete · readiness captured"
            : @"Fault inventory complete · readiness unavailable";
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_FREEZE_FRAME_SAMPLE:
        self.faultScanStatusText = event->context_response_available
            ? @"Fault inventory complete · freeze-frame context captured"
            : @"Fault inventory complete · freeze-frame PID unavailable";
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_DIAGNOSTIC_CONTEXT_COMPLETE:
        self.faultScanStatusText = [NSString stringWithFormat:
            @"Complete · %lu stored · %lu pending · %lu permanent · readiness %@ · freeze-frame %@",
            (unsigned long)self.storedDTCs.count,
            (unsigned long)self.pendingDTCs.count,
            (unsigned long)self.permanentDTCs.count,
            _flow.readiness_available ? @"captured" : @"unavailable",
            _flow.freeze_frame_requested
                ? (_flow.freeze_frame_sample_count != 0U
                    ? @"captured" : @"unavailable")
                : @"not required"];
        if (event->became_ready) self.ready = YES;
        break;

    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE:
    case LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED: {
        _consecutiveLiveTimeouts = 0U;
        NSString *recordingError = [_telemetryRecorder recordFlowEvent:event];
        if (recordingError != nil) {
            [self failWithStatus:recordingError];
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
        ? _sessionRunner.isConnected
        : _provider.isReady;

    if (!_sessionRunner.isInitialized || !transportReady ||
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
                   LINK_DIAGNOSTIC_FLOW_READING_READINESS) {
            self.statusText =
                @"Reading emissions readiness diagnostic context";
        } else if (_flow.stage ==
                   LINK_DIAGNOSTIC_FLOW_READING_FREEZE_FRAME) {
            self.statusText =
                @"Reading stored-fault freeze-frame context";
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

    case LINK_DIAGNOSTIC_FLOW_ACTION_SCHEDULED_MANUFACTURER_JOB: {
        id<LinkDiagnosticsControllerDelegate> delegate = self.delegate;
        if (action.manufacturer_job_token == 0U ||
            ![delegate respondsToSelector:
                @selector(linkDiagnosticsController:beginScheduledManufacturerJob:)]) {
            [self failWithStatus:
                @"Scheduled manufacturer job has no product handler"];
            return;
        }
        ++_pollGeneration;
        _manufacturerExtensionActive = YES;
        [delegate linkDiagnosticsController:self
            beginScheduledManufacturerJob:action.manufacturer_job_token];
        return;
    }

    case LINK_DIAGNOSTIC_FLOW_ACTION_READY: {
        const size_t enabledPollingCount =
            link_scheduler_enabled_standard_count(&_flow.scheduler);
        self.ready = YES;
        [self setSharedStatus:
            _flow.scheduler.count == 0U
                ? @"Connected; no supported dashboard PIDs were advertised"
                : (enabledPollingCount == 0U
                    ? @"Connected · polling idle · no PIDs selected"
                    : (_simulated
                        ? _simulatedLiveStatusText
                        : _liveStatusText))];
        return;
    }

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
    _liveRecoveryActive = NO;
    self.ready = NO;
    [self setSharedStatus:status];
}

- (void)updateStatusText:(NSString *)status
{
    [self setSharedStatus:status];
}

- (void)setVehicleIdentifier:(const char *)vehicleIdentifier
{
    [_telemetryRecorder setVehicleIdentifier:vehicleIdentifier];
}

- (NSUInteger)recordedSampleCount
{
    return _telemetryRecorder.recordedSampleCount;
}

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                                      limit:(NSUInteger)limit
{
    return [_telemetryRecorder recentValuesForPID:pid limit:limit];
}

- (NSArray<NSNumber *> *)observedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                      extendedID:(BOOL)extendedID
{
    return [_telemetryRecorder
        observedPIDsForResponderCANIdentifier:responderCANIdentifier
        extendedID:extendedID];
}

- (NSArray<NSNumber *> *)supportedPIDsForResponderCANIdentifier:
    (uint32_t)responderCANIdentifier
                                                       extendedID:(BOOL)extendedID
{
    const uint32_t maximumIdentifier = extendedID
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (responderCANIdentifier > maximumIdentifier) return @[];

    const LinkObd2PidSet *set =
        link_diagnostic_flow_supported_pids_for_responder(
            &_flow, responderCANIdentifier, extendedID);
    if (set == NULL) return @[];

    NSMutableArray<NSNumber *> *pids = [[NSMutableArray alloc] init];
    for (NSUInteger pid = 1U; pid < 256U; ++pid) {
        if (link_obd2_pid_set_contains(set, (uint8_t)pid)) {
            [pids addObject:@(pid)];
        }
    }
    return [pids copy];
}

- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid
                     responderCANIdentifier:(uint32_t)responderCANIdentifier
                                  extendedID:(BOOL)extendedID
                                       limit:(NSUInteger)limit
{
    return [_telemetryRecorder recentValuesForPID:pid
                            responderCANIdentifier:responderCANIdentifier
                                         extendedID:extendedID
                                              limit:limit];
}

- (BOOL)supportsPID:(uint8_t)pid
{
    return link_obd2_pid_set_contains(&_flow.supported_pids, pid);
}

- (BOOL)favouriteForPID:(uint8_t)pid
{
    return [_telemetryRecorder favouriteForPID:pid];
}

- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid
{
    [_telemetryRecorder setFavourite:favourite forPID:pid];
    [self notifyDelegate];
}

- (BOOL)pollingEnabledForPID:(uint8_t)pid
{
    return [_pollingCoordinator isEnabledForPID:pid];
}

- (void)setPollingEnabled:(BOOL)enabled forPID:(uint8_t)pid
{
    LinkApplePollingUpdateDisposition disposition = [_pollingCoordinator
        setEnabled:enabled forPID:pid flow:&_flow active:self.active
        manufacturerExtensionActive:_manufacturerExtensionActive];
    [self notifyDelegate];

    if (disposition == LinkApplePollingUpdateDispositionRestartLiveFlow) {
        [self driveDiagnosticFlow];
    } else if (disposition == LinkApplePollingUpdateDispositionBecameIdle) {
        [self setSharedStatus:@"Connected · polling idle · no PIDs selected"];
    }
}

- (void)applyPollingPreferencesToScheduler
{
    [_pollingCoordinator applyToFlow:&_flow];
}

- (nullable NSData *)csvDataSnapshot
{
    return [_telemetryRecorder csvDataSnapshot];
}

- (nullable NSString *)csvSnapshot
{
    return [_telemetryRecorder csvSnapshot];
}

@end


#pragma mark - Shared branded-product controller facade

@interface LinkProductDiagnosticsController ()
@property(nonatomic, copy, nullable) NSString *simulatedAdapterIdentifier;
@property(nonatomic, copy, nullable) NSString *simulatedVIN;
@end

@implementation LinkProductDiagnosticsController

- (instancetype)initWithProductSlug:(NSString *)productSlug
                         flowConfig:(LinkDiagnosticFlowConfig)flowConfig
                     liveStatusText:(NSString *)liveStatusText
            simulatedLiveStatusText:(NSString *)simulatedLiveStatusText
              standardVINStatusText:(NSString *)standardVINStatusText
         simulatedAdapterIdentifier:(NSString * _Nullable)simulatedAdapterIdentifier
                       simulatedVIN:(NSString * _Nullable)simulatedVIN
{
    self = [super init];
    if (self == nil) return nil;

    _shared = [[LinkDiagnosticsController alloc]
        initWithProductSlug:productSlug
        flowConfig:flowConfig
        liveStatusText:liveStatusText
        simulatedLiveStatusText:simulatedLiveStatusText
        standardVINStatusText:standardVINStatusText];
    _shared.delegate = self;
    self.simulatedAdapterIdentifier = simulatedAdapterIdentifier;
    self.simulatedVIN = simulatedVIN;
    return self;
}

- (void)dealloc
{
    _shared.delegate = nil;
}

- (LinkDiagnosticsController *)sharedController { return _shared; }
- (NSString *)linkVersionText { return _shared.linkVersionText; }
- (NSString *)statusText { return _shared.statusText; }
- (nullable NSString *)peripheralName { return _shared.peripheralName; }
- (nullable NSString *)adapterIdentifier { return _shared.adapterIdentifier; }
- (NSString *)obdProtocolText { return _shared.obdProtocolText; }
- (NSString *)faultScanStatusText { return _shared.faultScanStatusText; }
- (NSArray<NSString *> *)storedDTCs { return _shared.storedDTCs; }
- (NSArray<NSString *> *)pendingDTCs { return _shared.pendingDTCs; }
- (NSArray<NSString *> *)permanentDTCs { return _shared.permanentDTCs; }
- (NSString *)readinessStatusText { return _shared.readinessStatusText; }
- (NSArray<NSString *> *)readinessMonitorStatus
{
    return _shared.readinessMonitorStatus;
}
- (NSArray<NSString *> *)freezeFrameContext { return _shared.freezeFrameContext; }
- (NSString *)diagnosticCapabilityText { return _shared.diagnosticCapabilityText; }
- (NSString *)diagnosticCapabilityDetailText
{
    return _shared.diagnosticCapabilityDetailText;
}
- (NSString *)standardResponderSummary { return _shared.standardResponderSummary; }
- (NSString *)supportedPIDSummary { return _shared.supportedPIDSummary; }
- (NSString *)standardVINText { return _shared.standardVINText; }
- (NSArray<NSString *> *)standardLiveValueRows
{
    return _shared.standardLiveValueRows;
}
- (BOOL)isActive { return _shared.isActive; }
- (BOOL)isReady { return _shared.isReady; }
- (NSUInteger)recordedSampleCount { return _shared.recordedSampleCount; }
- (NSArray<NSString *> *)availableLanguageTags { return _shared.availableLanguageTags; }
- (NSArray<NSString *> *)availableLanguageNames { return _shared.availableLanguageNames; }
- (NSString *)selectedLanguageTag { return _shared.selectedLanguageTag; }
- (NSArray<NSString *> *)availableMeasurementSystemKeys
{
    return _shared.availableMeasurementSystemKeys;
}
- (NSArray<NSString *> *)availableMeasurementSystemNames
{
    return _shared.availableMeasurementSystemNames;
}
- (NSString *)selectedMeasurementSystemKey
{
    return _shared.selectedMeasurementSystemKey;
}

- (void)start { [_shared start]; }
- (void)startWithPeripheralIdentifier:(NSString *)peripheralIdentifier
{
    [_shared startWithPeripheralIdentifier:peripheralIdentifier];
}
- (void)startSimulated
{
    if (self.simulatedAdapterIdentifier.length == 0U ||
        self.simulatedVIN.length == 0U) {
        [_shared failWithStatus:@"Simulated product identity is not configured"];
        return;
    }
    [_shared
        startSimulatedWithAdapterIdentifier:self.simulatedAdapterIdentifier.UTF8String
        vin:self.simulatedVIN.UTF8String
        customResponder:NULL
        context:NULL];
}
- (void)disconnect { [_shared disconnect]; }
- (NSString *)localizedTextForKey:(NSString *)key
{
    return [_shared localizedTextForKey:key];
}
- (void)setSelectedLanguageTag:(NSString *)tag
{
    [_shared setSelectedLanguageTag:tag];
}
- (void)setSelectedMeasurementSystemKey:(NSString *)key
{
    [_shared setSelectedMeasurementSystemKey:key];
}
- (NSArray<NSNumber *> *)recentValuesForPID:(uint8_t)pid limit:(NSUInteger)limit
{
    return [_shared recentValuesForPID:pid limit:limit];
}
- (double)displayValueForPID:(uint8_t)pid canonicalValue:(double)value
{
    return [_shared displayValueForPID:pid canonicalValue:value];
}
- (double)displayTemperatureCelsius:(double)celsius
{
    return [_shared displayTemperatureCelsius:celsius];
}
- (NSString *)displayTemperatureUnit { return _shared.displayTemperatureUnit; }
- (NSArray<NSNumber *> *)displayRecentValuesForPID:(uint8_t)pid
                                              limit:(NSUInteger)limit
{
    return [_shared displayRecentValuesForPID:pid limit:limit];
}
- (NSString *)displayUnitForPID:(uint8_t)pid
{
    return [_shared displayUnitForPID:pid];
}
- (NSArray<NSNumber *> *)displayRangeForPID:(uint8_t)pid
{
    return [_shared displayRangeForPID:pid];
}
- (nullable NSString *)structuredDisplayValueForPID:(uint8_t)pid
{
    return [_shared structuredDisplayValueForPID:pid];
}
- (nullable NSString *)structuredRawHexForPID:(uint8_t)pid
{
    return [_shared structuredRawHexForPID:pid];
}
- (NSString *)dtcDisplayTextForCode:(NSString *)code
{
    return [_shared dtcDisplayTextForCode:code];
}
- (BOOL)supportsPID:(uint8_t)pid { return [_shared supportsPID:pid]; }
- (BOOL)favouriteForPID:(uint8_t)pid { return [_shared favouriteForPID:pid]; }
- (void)setFavourite:(BOOL)favourite forPID:(uint8_t)pid
{
    [_shared setFavourite:favourite forPID:pid];
}
- (BOOL)pollingEnabledForPID:(uint8_t)pid
{
    return [_shared pollingEnabledForPID:pid];
}
- (void)setPollingEnabled:(BOOL)enabled forPID:(uint8_t)pid
{
    [_shared setPollingEnabled:enabled forPID:pid];
}
- (nullable NSData *)csvDataSnapshot { return [_shared csvDataSnapshot]; }
- (nullable NSString *)csvSnapshot { return [_shared csvSnapshot]; }
- (const LinkDiagnosticFlow * _Nullable)diagnosticFlow
{
    return [_shared diagnosticFlow];
}

- (void)linkDiagnosticsControllerDidUpdate:(LinkDiagnosticsController *)controller
{
    (void)controller;
    [self productDiagnosticsDidUpdate];
}

- (void)productDiagnosticsDidUpdate
{
    /* Manufacturer subclasses notify their own typed delegates here. */
}

@end


#pragma mark - Shared vehicle-profile/session persistence

#include "LinkAppleSessionRunner.inc"
#include "LinkApplePollingCoordinator.inc"
#include "LinkAppleTelemetryRecorder.inc"
#include "LinkAppleSettings.inc"
#include "LinkVehicleProfileStore.inc"
