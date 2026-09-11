from pathlib import Path

controller = Path('platform/apple/LinkDiagnosticsController.m')
text = controller.read_text()
old = '''    LinkDiagnosticFlowEvent event;
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
'''
new = '''    const LinkDiagnosticFlowStage completedStage = _flow.stage;
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
'''
if text.count(old) != 1:
    raise SystemExit(f'live-entry anchor count={text.count(old)}')
controller.write_text(text.replace(old, new))

reg = Path('tests/apple/run-regressions.sh')
r = reg.read_text()
anchor = '''grep -Fq 'if (item->pid_valid && item->enabled) ++enabledPollingCount;' "$controller"
'''
addition = anchor + '''grep -Fq 'completedStage == LINK_DIAGNOSTIC_FLOW_CONFIGURING_LIVE_HEADERS' "$controller"
grep -Fq '[self applyPollingPreferencesToScheduler];' "$controller"
'''
if r.count(anchor) != 1:
    raise SystemExit('Apple regression anchor mismatch')
reg.write_text(r.replace(anchor, addition))

version = Path('VERSION')
if version.read_text().strip() != '0.15.15':
    raise SystemExit('unexpected LINK version')
version.write_text('0.15.16\n')

header = Path('include/link/version.h')
h = header.read_text()
needle = '#define LINK_VERSION_STRING "0.15.15"'
if h.count(needle) != 1:
    raise SystemExit('unexpected LINK version header')
header.write_text(h.replace(needle, '#define LINK_VERSION_STRING "0.15.16"'))
