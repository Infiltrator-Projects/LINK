// SPDX-License-Identifier: GPL-3.0-or-later
/* Internal implementation include; compiled through LinkDiagnosticsController.m. */

static NSString * const LinkVehicleKnownPeripheralDefaultsKey =
    @"link.ble.knownPeripheralIdentifier.v1";

static BOOL LinkVehicleSessionValidVIN(NSString *vin)
{
    return vin.length == 17U;
}

static NSString * _Nullable LinkVehicleSessionValidAdapterIdentifier(
    NSString *identifier)
{
    if (identifier.length == 0U) return nil;
    return [[NSUUID alloc] initWithUUIDString:identifier] != nil
        ? identifier : nil;
}

@interface LinkVehicleProfileStore ()
@property(nonatomic, copy, readwrite) NSString *productNamespace;
@property(nonatomic, copy) NSString *profilesKey;
@property(nonatomic, copy) NSString *selectedVINKey;
@property(nonatomic, copy) NSString *adapterMappingKey;
@property(nonatomic, copy, nullable) NSString *legacyProfileKey;
@property(nonatomic, copy, nullable) NSString *legacySelectedVINKey;
@property(nonatomic, copy, nullable) NSString *legacyAdapterMappingKey;
@end


static NSMutableArray<NSMutableDictionary *> *
LinkVehicleMutableResponders(NSDictionary *profile)
{
    NSMutableArray<NSMutableDictionary *> *responders =
        [[NSMutableArray alloc] init];
    NSArray *stored = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    for (id value in stored) {
        if ([value isKindOfClass:[NSDictionary class]])
            [responders addObject:[(NSDictionary *)value mutableCopy]];
    }
    return responders;
}

static NSMutableDictionary *LinkVehicleResponderEntry(
    NSMutableArray<NSMutableDictionary *> *responders,
    uint32_t responder,
    BOOL extended,
    BOOL *created)
{
    for (NSMutableDictionary *candidate in responders) {
        NSNumber *rx = candidate[@"rx"];
        NSNumber *isExtended = candidate[@"extended"];
        if ([rx isKindOfClass:[NSNumber class]] &&
            [isExtended isKindOfClass:[NSNumber class]] &&
            rx.unsignedIntValue == responder &&
            isExtended.boolValue == extended) {
            return candidate;
        }
    }
    NSMutableDictionary *entry = [@{
        @"rx": @(responder), @"extended": @(extended), @"pids": @[]
    } mutableCopy];
    [responders addObject:entry];
    if (created != NULL) *created = YES;
    return entry;
}

NSArray<NSNumber *> *LinkVehicleProfileCachedPIDs(
    NSDictionary *profile,
    uint32_t responderCANIdentifier,
    BOOL extendedID)
{
    if (![profile isKindOfClass:[NSDictionary class]]) return @[];
    const uint32_t maximum = extendedID
        ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
    if (responderCANIdentifier > maximum) return @[];

    NSArray *responders = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    NSMutableOrderedSet<NSNumber *> *pids =
        [[NSMutableOrderedSet alloc] init];
    for (id value in responders) {
        if (![value isKindOfClass:[NSDictionary class]]) continue;
        NSDictionary *responder = (NSDictionary *)value;
        NSNumber *rx = responder[@"rx"];
        NSNumber *extended = responder[@"extended"];
        if (![rx isKindOfClass:[NSNumber class]] ||
            ![extended isKindOfClass:[NSNumber class]] ||
            rx.unsignedIntValue != responderCANIdentifier ||
            extended.boolValue != extendedID) {
            continue;
        }
        NSArray *storedPIDs = [responder[@"pids"] isKindOfClass:[NSArray class]]
            ? responder[@"pids"] : @[];
        for (id pid in storedPIDs) {
            if (![pid isKindOfClass:[NSNumber class]]) continue;
            const unsigned int pidValue = [pid unsignedIntValue];
            if (pidValue <= UINT8_MAX) [pids addObject:@(pidValue)];
        }
    }
    return [[pids array] sortedArrayUsingSelector:@selector(compare:)];
}


@interface LinkVehicleProfileStandardResponder ()
@property(nonatomic) uint32_t responderCANIdentifier;
@property(nonatomic, getter=isExtendedID) BOOL extendedID;
@property(nonatomic, copy) NSArray<NSNumber *> *pids;
@end

@implementation LinkVehicleProfileStandardResponder
@end

NSArray<LinkVehicleProfileStandardResponder *> *
LinkVehicleProfileStandardResponders(NSDictionary *profile)
{
    if (![profile isKindOfClass:[NSDictionary class]]) return @[];
    NSArray *responders = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    NSMutableArray<LinkVehicleProfileStandardResponder *> *result =
        [[NSMutableArray alloc] initWithCapacity:responders.count];
    for (id raw in responders) {
        if (![raw isKindOfClass:[NSDictionary class]]) continue;
        NSDictionary *entry = (NSDictionary *)raw;
        NSNumber *rx = [entry[@"rx"] isKindOfClass:[NSNumber class]]
            ? entry[@"rx"] : nil;
        if (rx == nil) continue;
        BOOL extended = [entry[@"extended"] isKindOfClass:[NSNumber class]]
            ? [entry[@"extended"] boolValue] : NO;
        uint32_t responderID = rx.unsignedIntValue;
        uint32_t maximum = extended ? UINT32_C(0x1fffffff) : UINT32_C(0x7ff);
        if (responderID > maximum) continue;

        LinkVehicleProfileStandardResponder *value =
            [[LinkVehicleProfileStandardResponder alloc] init];
        value.responderCANIdentifier = responderID;
        value.extendedID = extended;
        value.pids = LinkVehicleProfileCachedPIDs(profile, responderID, extended);
        [result addObject:value];
    }
    return [result copy];
}

NSUInteger LinkVehicleProfileStandardResponderCount(NSDictionary *profile)
{
    if (![profile isKindOfClass:[NSDictionary class]]) return 0U;
    NSArray *responders = [profile[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? profile[@"liveResponders"] : @[];
    NSUInteger count = 0U;
    for (id value in responders) {
        if ([value isKindOfClass:[NSDictionary class]]) ++count;
    }
    return count;
}

@implementation LinkVehicleProfileStore

- (BOOL)mergeStandardCapabilitiesFromDiagnosticFlow:
    (const LinkDiagnosticFlow *)flow
                                             forVIN:(NSString *)vin
{
    if (flow == NULL || flow->supported_pid_responders.count == 0U ||
        !LinkVehicleSessionValidVIN(vin)) {
        return NO;
    }
    NSDictionary *existing = [self profileForVIN:vin];
    if (existing == nil) return NO;

    NSMutableDictionary *profile = [existing mutableCopy];
    NSMutableArray<NSMutableDictionary *> *responders =
        LinkVehicleMutableResponders(profile);
    BOOL changed = NO;

    for (size_t index = 0U;
         index < flow->supported_pid_responders.count; ++index) {
        const LinkObd2ResponderPidSet *set =
            &flow->supported_pid_responders.entries[index];
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, set->responder_id, set->extended_id, &created);
        if (created) changed = YES;

        NSMutableArray<NSNumber *> *pids = [[NSMutableArray alloc] init];
        for (unsigned int pid = 0U; pid <= UINT8_MAX; ++pid) {
            if (link_obd2_pid_set_contains(
                    &set->supported_pids, (uint8_t)pid)) {
                [pids addObject:@(pid)];
            }
        }
        NSArray *previous = [match[@"pids"] isKindOfClass:[NSArray class]]
            ? match[@"pids"] : @[];
        if (![previous isEqualToArray:pids]) {
            match[@"pids"] = [pids copy];
            changed = YES;
        }
    }

    if (!changed) return NO;
    profile[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    profile[@"liveResponders"] = [responders copy];
    [self saveProfile:[profile copy] forVIN:vin];
    return YES;
}

- (BOOL)mergeStandardCapabilitiesFromFlowEvent:
    (const LinkDiagnosticFlowEvent *)event
                                        forVIN:(NSString *)vin
{
    if (event == NULL ||
        (event->kind != LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_SAMPLE &&
         event->kind != LINK_DIAGNOSTIC_FLOW_EVENT_LIVE_STRUCTURED) ||
        (event->responder_samples.count == 0U &&
         event->responder_decoded.count == 0U) ||
        !LinkVehicleSessionValidVIN(vin)) {
        return NO;
    }
    NSDictionary *existing = [self profileForVIN:vin];
    if (existing == nil) return NO;

    NSMutableDictionary *profile = [existing mutableCopy];
    NSMutableArray<NSMutableDictionary *> *responders =
        LinkVehicleMutableResponders(profile);
    BOOL changed = NO;

    for (size_t index = 0U; index < event->responder_samples.count; ++index) {
        const LinkObd2ResponderSample *sample =
            &event->responder_samples.samples[index];
        if (!sample->responder_id_available) continue;
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, sample->responder_id, sample->extended_id, &created);
        if (created) changed = YES;
        NSMutableOrderedSet<NSNumber *> *pids =
            [[NSMutableOrderedSet alloc] initWithArray:
                [match[@"pids"] isKindOfClass:[NSArray class]]
                    ? match[@"pids"] : @[]];
        NSNumber *pid = @(sample->sample.pid);
        if (![pids containsObject:pid]) {
            [pids addObject:pid];
            match[@"pids"] = [[pids array]
                sortedArrayUsingSelector:@selector(compare:)];
            changed = YES;
        }
    }

    for (size_t index = 0U; index < event->responder_decoded.count; ++index) {
        const LinkObd2ResponderDecodedPid *entry =
            &event->responder_decoded.entries[index];
        if (!entry->responder_id_available || entry->decoded.definition == NULL)
            continue;
        BOOL created = NO;
        NSMutableDictionary *match = LinkVehicleResponderEntry(
            responders, entry->responder_id, entry->extended_id, &created);
        if (created) changed = YES;
        NSMutableOrderedSet<NSNumber *> *pids =
            [[NSMutableOrderedSet alloc] initWithArray:
                [match[@"pids"] isKindOfClass:[NSArray class]]
                    ? match[@"pids"] : @[]];
        NSNumber *pid = @(entry->decoded.definition->pid);
        if (![pids containsObject:pid]) {
            [pids addObject:pid];
            match[@"pids"] = [[pids array]
                sortedArrayUsingSelector:@selector(compare:)];
            changed = YES;
        }
    }

    if (!changed) return NO;
    profile[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    profile[@"liveResponders"] = [responders copy];
    [self saveProfile:[profile copy] forVIN:vin];
    return YES;
}


- (instancetype)initWithProductNamespace:(NSString *)productNamespace
                         legacyProfileKey:(NSString *)legacyProfileKey
                    legacySelectedVINKey:(NSString *)legacySelectedVINKey
                 legacyAdapterMappingKey:(NSString *)legacyAdapterMappingKey
{
    self = [super init];
    if (self == nil) return nil;

    NSString *namespace = [productNamespace
        stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (namespace.length == 0U) namespace = @"default";
    self.productNamespace = namespace.lowercaseString;
    self.profilesKey = [NSString stringWithFormat:
        @"link.vehicleProfiles.%@.v1", self.productNamespace];
    self.selectedVINKey = [NSString stringWithFormat:
        @"link.selectedVehicleVIN.%@.v1", self.productNamespace];
    self.adapterMappingKey = [NSString stringWithFormat:
        @"link.adapterPeripheralByVehicle.%@.v1", self.productNamespace];
    self.legacyProfileKey = [legacyProfileKey copy];
    self.legacySelectedVINKey = [legacySelectedVINKey copy];
    self.legacyAdapterMappingKey = [legacyAdapterMappingKey copy];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:self.profilesKey] == nil &&
        self.legacyProfileKey.length != 0U) {
        NSDictionary *legacy = [defaults dictionaryForKey:self.legacyProfileKey];
        if (legacy != nil) [defaults setObject:legacy forKey:self.profilesKey];
    }
    if ([defaults objectForKey:self.selectedVINKey] == nil &&
        self.legacySelectedVINKey.length != 0U) {
        NSString *legacy = [defaults stringForKey:self.legacySelectedVINKey];
        if (legacy != nil) [defaults setObject:legacy forKey:self.selectedVINKey];
    }
    if ([defaults objectForKey:self.adapterMappingKey] == nil &&
        self.legacyAdapterMappingKey.length != 0U) {
        NSDictionary *legacy =
            [defaults dictionaryForKey:self.legacyAdapterMappingKey];
        if (legacy != nil) [defaults setObject:legacy forKey:self.adapterMappingKey];
    }
    return self;
}

- (NSDictionary *)storedProfiles
{
    NSDictionary *profiles = [[NSUserDefaults standardUserDefaults]
        dictionaryForKey:self.profilesKey];
    return profiles != nil ? profiles : @{};
}

- (void)storeProfiles:(NSDictionary *)profiles
{
    [[NSUserDefaults standardUserDefaults] setObject:[profiles copy]
                                               forKey:self.profilesKey];
}

- (NSDictionary *)storedAdapterMapping
{
    NSDictionary *mapping = [[NSUserDefaults standardUserDefaults]
        dictionaryForKey:self.adapterMappingKey];
    return mapping != nil ? mapping : @{};
}

- (void)storeAdapterMapping:(NSDictionary *)mapping
{
    [[NSUserDefaults standardUserDefaults] setObject:[mapping copy]
                                               forKey:self.adapterMappingKey];
}

- (NSArray<NSDictionary *> *)savedProfiles
{
    NSMutableArray<NSDictionary *> *result = [[NSMutableArray alloc] init];
    for (id key in self.storedProfiles) {
        id value = self.storedProfiles[key];
        if (![value isKindOfClass:[NSDictionary class]]) continue;
        NSDictionary *profile = (NSDictionary *)value;
        NSString *vin = [profile[@"vin"] isKindOfClass:[NSString class]]
            ? profile[@"vin"] : ([key isKindOfClass:[NSString class]] ? key : nil);
        if (!LinkVehicleSessionValidVIN(vin)) continue;

        NSMutableDictionary *copy = [profile mutableCopy];
        copy[@"vin"] = vin;
        [result addObject:[copy copy]];
    }
    [result sortUsingComparator:^NSComparisonResult(
        NSDictionary *left, NSDictionary *right) {
        NSNumber *leftDate = [left[@"updatedAt"] isKindOfClass:[NSNumber class]]
            ? left[@"updatedAt"] : @0;
        NSNumber *rightDate = [right[@"updatedAt"] isKindOfClass:[NSNumber class]]
            ? right[@"updatedAt"] : @0;
        if (leftDate.doubleValue != rightDate.doubleValue) {
            return leftDate.doubleValue > rightDate.doubleValue
                ? NSOrderedAscending : NSOrderedDescending;
        }
        return [left[@"vin"] compare:right[@"vin"]];
    }];
    return [result copy];
}

- (NSString *)selectedVehicleVIN
{
    NSString *vin = [[NSUserDefaults standardUserDefaults]
        stringForKey:self.selectedVINKey];
    return LinkVehicleSessionValidVIN(vin) ? vin : nil;
}

- (NSDictionary *)profileForVIN:(NSString *)vin
{
    if (!LinkVehicleSessionValidVIN(vin)) return nil;
    NSDictionary *profile = self.storedProfiles[vin];
    if (![profile isKindOfClass:[NSDictionary class]]) return nil;
    return [profile copy];
}

- (BOOL)selectOfflineVehicleWithVIN:(NSString *)vin
{
    if ([self profileForVIN:vin] == nil) return NO;
    [[NSUserDefaults standardUserDefaults] setObject:vin
                                               forKey:self.selectedVINKey];
    return YES;
}

- (void)clearSelectedVehicle
{
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:self.selectedVINKey];
}

- (NSString *)associatedAdapterIdentifierForVIN:(NSString *)vin
{
    if (!LinkVehicleSessionValidVIN(vin)) return nil;

    NSString *identifier = [self.storedAdapterMapping[vin]
        isKindOfClass:[NSString class]] ? self.storedAdapterMapping[vin] : nil;
    identifier = LinkVehicleSessionValidAdapterIdentifier(identifier);
    if (identifier != nil) return identifier;

    NSDictionary *profile = [self profileForVIN:vin];
    identifier = [profile[@"adapterIdentifier"] isKindOfClass:[NSString class]]
        ? profile[@"adapterIdentifier"] : nil;
    return LinkVehicleSessionValidAdapterIdentifier(identifier);
}

- (void)recordLiveVIN:(NSString *)vin
{
    if (!LinkVehicleSessionValidVIN(vin)) return;

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    [defaults setObject:vin forKey:self.selectedVINKey];

    NSString *identifier = LinkVehicleSessionValidAdapterIdentifier(
        [defaults stringForKey:LinkVehicleKnownPeripheralDefaultsKey]);
    if (identifier == nil) return;

    NSMutableDictionary *mapping = [self.storedAdapterMapping mutableCopy];
    mapping[vin] = identifier;
    [self storeAdapterMapping:mapping];

    NSDictionary *existing = [self profileForVIN:vin];
    if (existing == nil) return;
    NSMutableDictionary *profile = [existing mutableCopy];
    profile[@"adapterIdentifier"] = identifier;
    NSMutableDictionary *profiles = [self.storedProfiles mutableCopy];
    profiles[vin] = [profile copy];
    [self storeProfiles:profiles];
}

- (void)mergeProfileFields:(NSDictionary *)fields forVIN:(NSString *)vin
{
    if (![fields isKindOfClass:[NSDictionary class]] ||
        !LinkVehicleSessionValidVIN(vin)) return;
    NSMutableDictionary *profile = [[self profileForVIN:vin] mutableCopy];
    if (profile == nil) profile = [[NSMutableDictionary alloc] init];
    [profile addEntriesFromDictionary:fields];
    profile[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    [self saveProfile:profile forVIN:vin];
}

- (void)saveProfile:(NSDictionary *)profile forVIN:(NSString *)vin
{
    if (![profile isKindOfClass:[NSDictionary class]] ||
        !LinkVehicleSessionValidVIN(vin)) return;

    NSMutableDictionary *stored = [profile mutableCopy];
    stored[@"vin"] = vin;
    if (![stored[@"updatedAt"] isKindOfClass:[NSNumber class]]) {
        stored[@"updatedAt"] = @([[NSDate date] timeIntervalSince1970]);
    }

    /* Preserve LINK-owned standard responder capability across product saves. */
    NSDictionary *existing = [self profileForVIN:vin];
    NSArray *responders = [existing[@"liveResponders"] isKindOfClass:[NSArray class]]
        ? existing[@"liveResponders"] : nil;
    if (stored[@"liveResponders"] == nil && responders != nil)
        stored[@"liveResponders"] = responders;

    NSString *identifier = [self associatedAdapterIdentifierForVIN:vin];
    if (identifier != nil) {
        stored[@"adapterIdentifier"] = identifier;
    } else {
        identifier = LinkVehicleSessionValidAdapterIdentifier(
            [stored[@"adapterIdentifier"] isKindOfClass:[NSString class]]
                ? stored[@"adapterIdentifier"] : nil);
        if (identifier != nil) {
            NSMutableDictionary *mapping = [self.storedAdapterMapping mutableCopy];
            mapping[vin] = identifier;
            [self storeAdapterMapping:mapping];
            stored[@"adapterIdentifier"] = identifier;
        }
    }

    NSMutableDictionary *profiles = [self.storedProfiles mutableCopy];
    profiles[vin] = [stored copy];
    [self storeProfiles:profiles];
}

- (void)removeProfileForVIN:(NSString *)vin
{
    if (!LinkVehicleSessionValidVIN(vin)) return;

    NSMutableDictionary *profiles = [self.storedProfiles mutableCopy];
    [profiles removeObjectForKey:vin];
    [self storeProfiles:profiles];

    NSMutableDictionary *mapping = [self.storedAdapterMapping mutableCopy];
    [mapping removeObjectForKey:vin];
    [self storeAdapterMapping:mapping];

    if ([self.selectedVehicleVIN isEqualToString:vin])
        [self clearSelectedVehicle];
}

@end
#pragma mark - Shared standard PID selection persistence

static NSArray<NSString *> *LinkSortedUniqueStableKeys(id raw)
{
    if (![raw isKindOfClass:[NSArray class]]) return @[];
    NSMutableOrderedSet<NSString *> *values = [[NSMutableOrderedSet alloc] init];
    for (id value in (NSArray *)raw) {
        if (![value isKindOfClass:[NSString class]]) continue;
        NSString *key = [(NSString *)value
            stringByTrimmingCharactersInSet:
                [NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (key.length != 0U) [values addObject:key];
    }
    return [[values array] sortedArrayUsingSelector:@selector(compare:)];
}

@interface LinkPIDSelectionStore ()
@property(nonatomic, copy) NSString *globalKey;
@property(nonatomic, copy) NSString *vehicleKey;
@end

@implementation LinkPIDSelectionStore

- (instancetype)initWithProductNamespace:(NSString *)productNamespace
                         legacyGlobalKey:(NSString *)legacyGlobalKey
                        legacyVehicleKey:(NSString *)legacyVehicleKey
{
    self = [super init];
    if (self == nil) return nil;
    NSString *name = [productNamespace
        stringByTrimmingCharactersInSet:
            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (name.length == 0U) name = @"default";
    name = name.lowercaseString;
    self.globalKey = [NSString stringWithFormat:
        @"link.pidSelections.global.%@.v1", name];
    self.vehicleKey = [NSString stringWithFormat:
        @"link.pidSelections.byVehicle.%@.v1", name];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    if ([defaults objectForKey:self.globalKey] == nil && legacyGlobalKey.length != 0U) {
        id legacy = [defaults objectForKey:legacyGlobalKey];
        if ([legacy isKindOfClass:[NSArray class]])
            [defaults setObject:LinkSortedUniqueStableKeys(legacy) forKey:self.globalKey];
    }
    if ([defaults objectForKey:self.vehicleKey] == nil && legacyVehicleKey.length != 0U) {
        NSDictionary *legacy = [defaults dictionaryForKey:legacyVehicleKey];
        if (legacy != nil) [defaults setObject:legacy forKey:self.vehicleKey];
    }
    return self;
}

- (BOOL)hasGlobalSelection
{
    return [[NSUserDefaults standardUserDefaults] objectForKey:self.globalKey] != nil;
}

- (NSArray<NSString *> *)globalStableKeys
{
    return LinkSortedUniqueStableKeys(
        [[NSUserDefaults standardUserDefaults] objectForKey:self.globalKey]);
}

- (void)setGlobalStableKeys:(NSArray<NSString *> *)stableKeys
{
    [[NSUserDefaults standardUserDefaults]
        setObject:LinkSortedUniqueStableKeys(stableKeys) forKey:self.globalKey];
}

- (NSDictionary *)storedVehicleSelections
{
    NSDictionary *stored = [[NSUserDefaults standardUserDefaults]
        dictionaryForKey:self.vehicleKey];
    return stored != nil ? stored : @{};
}

- (BOOL)hasSelectionForVIN:(NSString *)vin
      controllerIdentifier:(NSString *)controllerIdentifier
{
    if (vin.length != 17U || controllerIdentifier.length == 0U) return NO;
    NSDictionary *vehicle = [self.storedVehicleSelections[vin]
        isKindOfClass:[NSDictionary class]]
        ? self.storedVehicleSelections[vin] : nil;
    return vehicle != nil && vehicle[controllerIdentifier] != nil;
}

- (NSArray<NSString *> *)stableKeysForVIN:(NSString *)vin
                     controllerIdentifier:(NSString *)controllerIdentifier
{
    if (vin.length != 17U || controllerIdentifier.length == 0U) return @[];
    NSDictionary *vehicle = [self.storedVehicleSelections[vin]
        isKindOfClass:[NSDictionary class]]
        ? self.storedVehicleSelections[vin] : @{};
    return LinkSortedUniqueStableKeys(vehicle[controllerIdentifier]);
}

- (void)setStableKeys:(NSArray<NSString *> *)stableKeys
               forVIN:(NSString *)vin
 controllerIdentifier:(NSString *)controllerIdentifier
{
    if (vin.length != 17U || controllerIdentifier.length == 0U) return;
    NSMutableDictionary *root = [self.storedVehicleSelections mutableCopy];
    NSDictionary *existing = [root[vin] isKindOfClass:[NSDictionary class]]
        ? root[vin] : @{};
    NSMutableDictionary *vehicle = [existing mutableCopy];
    vehicle[controllerIdentifier] = LinkSortedUniqueStableKeys(stableKeys);
    root[vin] = [vehicle copy];
    [[NSUserDefaults standardUserDefaults] setObject:[root copy]
                                               forKey:self.vehicleKey];
}

@end
