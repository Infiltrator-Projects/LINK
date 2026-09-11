#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one guarded match, found {count}")
    p.write_text(text.replace(old, new, 1))


# Portable ownership for retained per-PID polling choices. This deliberately
# lives beside the scheduler data model while remaining a separate abstraction:
# the scheduler owns what can run; the policy owns what the user wants enabled.
replace_once(
    "include/link/scheduler.h",
    "#define LINK_SCHEDULER_MAX_ITEMS 256U\n#define LINK_OBD2_PID_SET_BYTES 32U\n",
    "#define LINK_SCHEDULER_MAX_ITEMS 256U\n#define LINK_OBD2_PID_SET_BYTES 32U\n#define LINK_OBD2_PID_COUNT 256U\n",
)
replace_once(
    "include/link/scheduler.h",
    "typedef struct {\n    LinkSchedulerItem items[LINK_SCHEDULER_MAX_ITEMS];\n    size_t count;\n    bool paused;\n    uint64_t pause_started_ms;\n} LinkScheduler;\n\ntypedef struct {\n    size_t index;\n",
    "typedef struct {\n    LinkSchedulerItem items[LINK_SCHEDULER_MAX_ITEMS];\n    size_t count;\n    bool paused;\n    uint64_t pause_started_ms;\n} LinkScheduler;\n\n/** Retained user policy for standard OBD-II PID polling. */\ntypedef struct {\n    bool pid_enabled[LINK_OBD2_PID_COUNT];\n} LinkPollingPolicy;\n\ntypedef struct {\n    size_t index;\n",
)
replace_once(
    "include/link/scheduler.h",
    "LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled);\n\n/**\n * Register one opaque manufacturer/product live transaction with LINK's single\n",
    "LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled);\n\n/**\n * Retain standard PID choices independently of scheduler construction. A\n * product may set choices before discovery; applying the policy later changes\n * only standard OBD-II scheduler items and never touches external/OEM jobs.\n */\nvoid link_polling_policy_init(LinkPollingPolicy *policy, bool enabled_by_default);\nbool link_polling_policy_is_enabled(const LinkPollingPolicy *policy, uint8_t pid);\nvoid link_polling_policy_set_enabled(LinkPollingPolicy *policy, uint8_t pid, bool enabled);\nsize_t link_polling_policy_apply_to_scheduler(const LinkPollingPolicy *policy, LinkScheduler *scheduler);\nsize_t link_scheduler_enabled_standard_count(const LinkScheduler *scheduler);\n\n/**\n * Register one opaque manufacturer/product live transaction with LINK's single\n",
)

replace_once(
    "src/core/scheduler.c",
    "LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled)\n{\n    const LinkParameterKey key = obd2_key(pid);\n    return link_scheduler_set_parameter_enabled(scheduler, &key, enabled);\n}\n\nLinkSchedulerResult link_scheduler_add_external(\n",
    "LinkSchedulerResult link_scheduler_set_enabled(LinkScheduler *scheduler, uint8_t pid, bool enabled)\n{\n    const LinkParameterKey key = obd2_key(pid);\n    return link_scheduler_set_parameter_enabled(scheduler, &key, enabled);\n}\n\nvoid link_polling_policy_init(LinkPollingPolicy *policy, bool enabled_by_default)\n{\n    size_t pid;\n    if (policy == NULL) return;\n    for (pid = 0U; pid < LINK_OBD2_PID_COUNT; ++pid)\n        policy->pid_enabled[pid] = enabled_by_default;\n}\n\nbool link_polling_policy_is_enabled(const LinkPollingPolicy *policy, uint8_t pid)\n{\n    return policy != NULL && policy->pid_enabled[pid];\n}\n\nvoid link_polling_policy_set_enabled(\n    LinkPollingPolicy *policy, uint8_t pid, bool enabled)\n{\n    if (policy == NULL) return;\n    policy->pid_enabled[pid] = enabled;\n}\n\nsize_t link_polling_policy_apply_to_scheduler(\n    const LinkPollingPolicy *policy, LinkScheduler *scheduler)\n{\n    size_t index;\n    size_t enabled_count = 0U;\n    if (policy == NULL || scheduler == NULL) return 0U;\n\n    for (index = 0U; index < scheduler->count; ++index) {\n        LinkSchedulerItem *item = &scheduler->items[index];\n        if (item->kind != LINK_SCHEDULER_ITEM_PARAMETER || !item->pid_valid)\n            continue;\n        item->enabled = policy->pid_enabled[item->pid];\n        if (item->enabled) ++enabled_count;\n    }\n    return enabled_count;\n}\n\nsize_t link_scheduler_enabled_standard_count(const LinkScheduler *scheduler)\n{\n    size_t index;\n    size_t enabled_count = 0U;\n    if (scheduler == NULL) return 0U;\n    for (index = 0U; index < scheduler->count; ++index) {\n        const LinkSchedulerItem *item = &scheduler->items[index];\n        if (item->kind == LINK_SCHEDULER_ITEM_PARAMETER &&\n            item->pid_valid && item->enabled) {\n            ++enabled_count;\n        }\n    }\n    return enabled_count;\n}\n\nLinkSchedulerResult link_scheduler_add_external(\n",
)

# Apple facade now delegates retained PID policy to the portable owner. Public
# API and live-flow semantics remain byte-for-byte equivalent at the boundary.
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "    BOOL _pidPollingEnabled[256];\n",
    "    LinkPollingPolicy _pollingPolicy;\n",
)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "    for (NSUInteger pid = 0U; pid < 256U; ++pid)\n        _pidPollingEnabled[pid] = YES;\n",
    "    link_polling_policy_init(&_pollingPolicy, true);\n",
)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "- (BOOL)pollingEnabledForPID:(uint8_t)pid\n{\n    return _pidPollingEnabled[pid];\n}\n\n- (void)setPollingEnabled:(BOOL)enabled forPID:(uint8_t)pid\n{\n    _pidPollingEnabled[pid] = enabled;\n",
    "- (BOOL)pollingEnabledForPID:(uint8_t)pid\n{\n    return link_polling_policy_is_enabled(&_pollingPolicy, pid) ? YES : NO;\n}\n\n- (void)setPollingEnabled:(BOOL)enabled forPID:(uint8_t)pid\n{\n    link_polling_policy_set_enabled(\n        &_pollingPolicy, pid, enabled ? true : false);\n",
)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "            size_t enabledPollingCount = 0U;\n            for (size_t index = 0U; index < _flow.scheduler.count; ++index) {\n                const LinkSchedulerItem *item = &_flow.scheduler.items[index];\n                if (item->pid_valid && item->enabled) ++enabledPollingCount;\n            }\n            if (enabledPollingCount == 0U) {\n",
    "            const size_t enabledPollingCount =\n                link_scheduler_enabled_standard_count(&_flow.scheduler);\n            if (enabledPollingCount == 0U) {\n",
)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "- (void)applyPollingPreferencesToScheduler\n{\n    for (size_t index = 0U; index < _flow.scheduler.count; ++index) {\n        const LinkSchedulerItem *item = &_flow.scheduler.items[index];\n        if (!item->pid_valid) continue;\n        (void)link_scheduler_set_enabled(\n            &_flow.scheduler, item->pid, _pidPollingEnabled[item->pid]);\n    }\n}\n",
    "- (void)applyPollingPreferencesToScheduler\n{\n    (void)link_polling_policy_apply_to_scheduler(\n        &_pollingPolicy, &_flow.scheduler);\n}\n",
)
replace_once(
    "platform/apple/LinkDiagnosticsController.m",
    "    case LINK_DIAGNOSTIC_FLOW_ACTION_READY: {\n        size_t enabledPollingCount = 0U;\n        for (size_t index = 0U; index < _flow.scheduler.count; ++index) {\n            const LinkSchedulerItem *item = &_flow.scheduler.items[index];\n            if (item->pid_valid && item->enabled) ++enabledPollingCount;\n        }\n        self.ready = YES;\n",
    "    case LINK_DIAGNOSTIC_FLOW_ACTION_READY: {\n        const size_t enabledPollingCount =\n            link_scheduler_enabled_standard_count(&_flow.scheduler);\n        self.ready = YES;\n",
)

# Characterise the public Apple facade before future extractions go further.
replace_once(
    "tests/apple/ProfileRegression.swift",
    "        precondition(saved[\"modules\"] != nil)\n        print(\"LINK Apple simulation isolation and profile-patch regressions passed\")\n",
    "        precondition(saved[\"modules\"] != nil)\n\n        // Retained polling choices are controller policy, not scheduler lifetime.\n        let pollingController = LinkDiagnosticsController(\n            productSlug: namespace + \"-polling\", flowConfig: flow,\n            liveStatusText: \"live\", simulatedLiveStatusText: \"simulated\",\n            standardVINStatusText: \"VIN\")\n        precondition(pollingController.pollingEnabled(forPID: 0x0C))\n        precondition(pollingController.pollingEnabled(forPID: 0x0D))\n        pollingController.setPollingEnabled(false, forPID: 0x0C)\n        precondition(!pollingController.pollingEnabled(forPID: 0x0C))\n        precondition(pollingController.pollingEnabled(forPID: 0x0D),\n                     \"Changing one PID must not mutate another PID choice\")\n        pollingController.setPollingEnabled(true, forPID: 0x0C)\n        precondition(pollingController.pollingEnabled(forPID: 0x0C))\n\n        print(\"LINK Apple simulation, profile-patch and polling-policy regressions passed\")\n",
)

replace_once(
    "tests/apple/run-regressions.sh",
    "grep -Fq 'if (item->pid_valid && item->enabled) ++enabledPollingCount;' \"$controller\"\n",
    "grep -Fq 'link_scheduler_enabled_standard_count(&_flow.scheduler)' \"$controller\"\n",
)
replace_once(
    "tests/apple/run-regressions.sh",
    "controller=platform/apple/LinkDiagnosticsController.m\ngrep -Fq 'Connected · polling idle · no PIDs selected' \"$controller\"\n",
    "controller=platform/apple/LinkDiagnosticsController.m\ngrep -Fq 'Connected · polling idle · no PIDs selected' \"$controller\"\ngrep -Fq 'LinkPollingPolicy _pollingPolicy;' \"$controller\"\ngrep -Fq 'link_polling_policy_apply_to_scheduler(' \"$controller\"\nif grep -Fq '_pidPollingEnabled' \"$controller\"; then\n    echo 'Apple controller must not own a second PID polling-policy array.' >&2\n    exit 1\nfi\n",
)

# Prepare the next validated release without publishing from the helper commit.
replace_once("VERSION", "0.15.16\n", "0.15.17\n")
replace_once(
    "include/link/version.h",
    '#define LINK_VERSION_STRING "0.15.16"\n',
    '#define LINK_VERSION_STRING "0.15.17"\n',
)

print("Polling policy ownership refactor applied successfully")
