from pathlib import Path


def replace(path, old, new, count=1):
    p = Path(path)
    text = p.read_text()
    if old not in text:
        raise SystemExit(f"expected text not found in {path}: {old[:120]!r}")
    p.write_text(text.replace(old, new, count))


Path('VERSION').write_text('0.15.4\n')
replace('include/link/version.h', '#define LINK_VERSION_STRING "0.15.3"', '#define LINK_VERSION_STRING "0.15.4"')

Path('include/link/dashboard.h').write_text('''// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file dashboard.h
 * @brief Shared dashboard presentation policy for every LINK product face.
 *
 * The core owns mode names and gauge-range mathematics. Native platform faces
 * own drawing, while manufacturer products supply only branding and any
 * evidence-backed parameter metadata not already present in LINK.
 */
#ifndef LINK_DASHBOARD_H
#define LINK_DASHBOARD_H

#include "link/parameter.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum LinkDashboardPresentationMode {
    LINK_DASHBOARD_PRESENTATION_NUMBERS = 0,
    LINK_DASHBOARD_PRESENTATION_DIALS,
    LINK_DASHBOARD_PRESENTATION_COMBINED
} LinkDashboardPresentationMode;

typedef struct LinkDashboardGaugeRange {
    double minimum;
    double maximum;
} LinkDashboardGaugeRange;

/** Stable persistence key: numbers, dials or combined. */
const char *link_dashboard_presentation_mode_key(
    LinkDashboardPresentationMode mode);

/** Parses a stable persistence key without accepting aliases. */
bool link_dashboard_presentation_mode_from_key(
    const char *key,
    LinkDashboardPresentationMode *mode);

/** Returns a finite numeric range; otherwise the parameter stays text-only. */
bool link_dashboard_gauge_range_for_parameter(
    const LinkParameterDefinition *definition,
    LinkDashboardGaugeRange *range);

/** Clamps a measured value to a 0..1 position in a validated range. */
bool link_dashboard_gauge_fraction(
    const LinkDashboardGaugeRange *range,
    double value,
    double *fraction);

#ifdef __cplusplus
}
#endif

#endif
''')

Path('src/core/dashboard.c').write_text('''// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dashboard.h"

#include <math.h>
#include <string.h>

const char *link_dashboard_presentation_mode_key(
    LinkDashboardPresentationMode mode)
{
    switch (mode) {
    case LINK_DASHBOARD_PRESENTATION_NUMBERS: return "numbers";
    case LINK_DASHBOARD_PRESENTATION_DIALS: return "dials";
    case LINK_DASHBOARD_PRESENTATION_COMBINED: return "combined";
    }
    return "numbers";
}

bool link_dashboard_presentation_mode_from_key(
    const char *key,
    LinkDashboardPresentationMode *mode)
{
    if (key == NULL || mode == NULL) return false;
    if (strcmp(key, "numbers") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_NUMBERS;
        return true;
    }
    if (strcmp(key, "dials") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_DIALS;
        return true;
    }
    if (strcmp(key, "combined") == 0) {
        *mode = LINK_DASHBOARD_PRESENTATION_COMBINED;
        return true;
    }
    return false;
}

bool link_dashboard_gauge_range_for_parameter(
    const LinkParameterDefinition *definition,
    LinkDashboardGaugeRange *range)
{
    if (definition == NULL || range == NULL ||
        !link_parameter_definition_is_valid(definition) ||
        !isfinite(definition->minimum) || !isfinite(definition->maximum) ||
        definition->maximum <= definition->minimum) {
        return false;
    }
    range->minimum = definition->minimum;
    range->maximum = definition->maximum;
    return true;
}

bool link_dashboard_gauge_fraction(
    const LinkDashboardGaugeRange *range,
    double value,
    double *fraction)
{
    double result;
    if (range == NULL || fraction == NULL || !isfinite(value) ||
        !isfinite(range->minimum) || !isfinite(range->maximum) ||
        range->maximum <= range->minimum) {
        return false;
    }
    result = (value - range->minimum) / (range->maximum - range->minimum);
    if (result < 0.0) result = 0.0;
    if (result > 1.0) result = 1.0;
    *fraction = result;
    return true;
}
''')

Path('tests/test_dashboard.c').write_text('''// SPDX-License-Identifier: GPL-3.0-or-later
#include "link/dashboard.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \\
    if (!(condition)) { \\
        fprintf(stderr, "CHECK failed at %s:%d: %s\\n", __FILE__, __LINE__, #condition); \\
        return 1; \\
    } \\
} while (0)

int main(void)
{
    LinkDashboardPresentationMode mode;
    LinkDashboardGaugeRange range;
    double fraction = -1.0;
    const LinkParameterDefinition *rpm =
        link_parameter_obd2_definition(UINT8_C(0x0c));

    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_NUMBERS), "numbers") == 0);
    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_DIALS), "dials") == 0);
    CHECK(strcmp(link_dashboard_presentation_mode_key(
        LINK_DASHBOARD_PRESENTATION_COMBINED), "combined") == 0);
    CHECK(link_dashboard_presentation_mode_from_key("combined", &mode));
    CHECK(mode == LINK_DASHBOARD_PRESENTATION_COMBINED);
    CHECK(!link_dashboard_presentation_mode_from_key("gauge", &mode));

    CHECK(rpm != NULL);
    CHECK(link_dashboard_gauge_range_for_parameter(rpm, &range));
    CHECK(range.maximum > range.minimum);
    CHECK(link_dashboard_gauge_fraction(&range, range.minimum, &fraction));
    CHECK(fabs(fraction) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(&range, range.maximum, &fraction));
    CHECK(fabs(fraction - 1.0) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(
        &range, (range.minimum + range.maximum) / 2.0, &fraction));
    CHECK(fabs(fraction - 0.5) < 1e-12);
    CHECK(link_dashboard_gauge_fraction(&range, range.maximum * 10.0, &fraction));
    CHECK(fabs(fraction - 1.0) < 1e-12);
    return 0;
}
''')

replace('CMakeLists.txt',
        '    src/core/diagnostic_capability.c\n    src/core/isotp.c\n    src/core/parameter.c\n',
        '    src/core/diagnostic_capability.c\n    src/core/isotp.c\n    src/core/parameter.c\n    src/core/dashboard.c\n')
replace('CMakeLists.txt',
        '    link_add_test(link-test-parameter-observation tests/test_parameter_observation.c link-parameter-observation)\n',
        '    link_add_test(link-test-parameter-observation tests/test_parameter_observation.c link-parameter-observation)\n    link_add_test(link-test-dashboard tests/test_dashboard.c link-dashboard)\n')

apple = Path('platform/apple/LinkDiagnosticUI.swift')
text = apple.read_text()
old_fields = '''    let history: [Double]\n    let sourceLabel: String?\n    let qualityNote: String?\n\n    var isAvailable: Bool { value != nil || !(structuredValue ?? "").isEmpty }'''
new_fields = '''    let history: [Double]\n    let sourceLabel: String?\n    let qualityNote: String?\n    let dashboardMinimum: Double?\n    let dashboardMaximum: Double?\n\n    init(\n        id: String,\n        protocolName: String,\n        moduleIdentifier: UInt32,\n        parameterIdentifier: UInt32,\n        shortName: String,\n        title: String,\n        suffix: String,\n        formattedValue: String,\n        value: Double?,\n        structuredValue: String?,\n        rawHex: String?,\n        vehicleSupported: Bool,\n        favourite: Bool,\n        pollingEnabled: Bool,\n        history: [Double],\n        sourceLabel: String?,\n        qualityNote: String?,\n        dashboardMinimum: Double? = nil,\n        dashboardMaximum: Double? = nil\n    ) {\n        self.id = id\n        self.protocolName = protocolName\n        self.moduleIdentifier = moduleIdentifier\n        self.parameterIdentifier = parameterIdentifier\n        self.shortName = shortName\n        self.title = title\n        self.suffix = suffix\n        self.formattedValue = formattedValue\n        self.value = value\n        self.structuredValue = structuredValue\n        self.rawHex = rawHex\n        self.vehicleSupported = vehicleSupported\n        self.favourite = favourite\n        self.pollingEnabled = pollingEnabled\n        self.history = history\n        self.sourceLabel = sourceLabel\n        self.qualityNote = qualityNote\n        self.dashboardMinimum = dashboardMinimum\n        self.dashboardMaximum = dashboardMaximum\n    }\n\n    var isAvailable: Bool { value != nil || !(structuredValue ?? "").isEmpty }'''
if old_fields not in text:
    raise SystemExit('Apple parameter field anchor not found')
text = text.replace(old_fields, new_fields, 1)

old_props = '''    var hasLiveValue: Bool { pollingEnabled && isAvailable }\n    var pidText: String {'''
new_props = '''    var hasLiveValue: Bool { pollingEnabled && isAvailable }\n    var dashboardDialSupported: Bool {\n        guard structuredValue == nil,\n              let minimum = dashboardMinimum,\n              let maximum = dashboardMaximum,\n              minimum.isFinite, maximum.isFinite, maximum > minimum else {\n            return false\n        }\n        return true\n    }\n    var dashboardFraction: Double? {\n        guard let value, dashboardDialSupported,\n              let minimum = dashboardMinimum,\n              let maximum = dashboardMaximum else { return nil }\n        return min(1.0, max(0.0, (value - minimum) / (maximum - minimum)))\n    }\n    var pidText: String {'''
if old_props not in text:
    raise SystemExit('Apple parameter property anchor not found')
text = text.replace(old_props, new_props, 1)

metric_anchor = '''struct LinkMetricTile: View {\n    @Environment(\\.linkDiagnosticTheme) private var theme\n    let parameter: LinkDiagnosticParameter\n'''
if metric_anchor not in text:
    raise SystemExit('LinkMetricTile anchor not found')

dashboard_ui = r'''enum LinkDashboardPresentationMode: String, CaseIterable, Identifiable {
    case numbers
    case dials
    case combined

    var id: String { rawValue }
    var title: String {
        switch self {
        case .numbers: return "Numbers"
        case .dials: return "Dials"
        case .combined: return "Combined"
        }
    }
}

struct LinkDashboardModePicker: View {
    @Binding var selection: LinkDashboardPresentationMode

    var body: some View {
        Picker("Dashboard display", selection: $selection) {
            ForEach(LinkDashboardPresentationMode.allCases) { mode in
                Text(LocalizedStringKey(mode.title)).tag(mode)
            }
        }
        .pickerStyle(.segmented)
        .accessibilityLabel("Dashboard display")
    }
}

private struct LinkDashboardGaugeTile: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let parameter: LinkDiagnosticParameter
    let showDetails: Bool

    private var fraction: Double { parameter.dashboardFraction ?? 0.0 }

    var body: some View {
        VStack(spacing: 8) {
            ZStack {
                Circle()
                    .fill(
                        RadialGradient(
                            colors: [theme.panelRaised, theme.panel, Color.black.opacity(0.78)],
                            center: .topLeading,
                            startRadius: 4,
                            endRadius: 86))
                Circle()
                    .stroke(theme.border.opacity(0.82), lineWidth: 1)
                    .padding(10)
                Circle()
                    .trim(from: 0.0, to: 0.75)
                    .stroke(
                        theme.border.opacity(0.9),
                        style: StrokeStyle(lineWidth: 7, lineCap: .round))
                    .rotationEffect(.degrees(135))
                    .padding(4)
                Circle()
                    .trim(from: 0.0, to: 0.75 * fraction)
                    .stroke(
                        parameter.hasLiveValue ? theme.accent : theme.mutedText,
                        style: StrokeStyle(lineWidth: 7, lineCap: .round))
                    .rotationEffect(.degrees(135))
                    .padding(4)

                VStack(spacing: 2) {
                    Text(parameter.presentationValue)
                        .font(theme.typography.title2)
                        .monospacedDigit()
                        .foregroundStyle(
                            parameter.hasLiveValue
                                ? theme.primaryText : theme.mutedText)
                        .minimumScaleFactor(0.55)
                        .lineLimit(1)
                    Text(parameter.pidText)
                        .font(theme.typography.caption2)
                        .foregroundStyle(theme.mutedText)
                }
                .padding(.horizontal, 18)
            }
            .aspectRatio(1, contentMode: .fit)
            .frame(maxWidth: 164)

            Text(LocalizedStringKey(parameter.title))
                .font(theme.typography.captionBold)
                .foregroundStyle(theme.secondaryText)
                .multilineTextAlignment(.center)
                .lineLimit(2)

            if showDetails {
                if let source = parameter.sourceLabel {
                    Label(source, systemImage: "cpu")
                        .font(theme.typography.caption2)
                        .foregroundStyle(theme.mutedText)
                        .lineLimit(2)
                }
                if let qualityNote = parameter.qualityNote {
                    Text(qualityNote)
                        .font(theme.typography.caption2)
                        .foregroundStyle(theme.warning)
                        .lineLimit(2)
                }
            }
        }
        .frame(maxWidth: .infinity, minHeight: showDetails ? 218 : 194)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(theme.panelRaised))
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(theme.border, lineWidth: 1))
    }
}

struct LinkDashboardMetric: View {
    let parameter: LinkDiagnosticParameter
    let mode: LinkDashboardPresentationMode

    @ViewBuilder
    var body: some View {
        switch mode {
        case .numbers:
            LinkMetricTile(parameter: parameter)
        case .dials:
            if parameter.dashboardDialSupported {
                LinkDashboardGaugeTile(parameter: parameter, showDetails: false)
            } else {
                LinkMetricTile(parameter: parameter)
            }
        case .combined:
            if parameter.dashboardDialSupported {
                LinkDashboardGaugeTile(parameter: parameter, showDetails: true)
            } else {
                LinkMetricTile(parameter: parameter)
            }
        }
    }
}

'''
text = text.replace(metric_anchor, dashboard_ui + metric_anchor, 1)
apple.write_text(text)

docs = Path('docs/PRODUCT_FACES.md')
doc = docs.read_text()
anchor = '''An iPhone layout change intended for every vehicle product must therefore be
made in LINK first and consumed by each pinned product face. This makes visual
and structural drift between MBLINK, JAGLINK, BMWLINK, AUDILINK, FORDLINK and future LINK products an
explicit architectural regression rather than normal parallel development.
'''
addition = anchor + '''
### Shared Dashboard presentation

Dashboard presentation is LINK behaviour. LINK exposes the three persistent
operator modes `numbers`, `dials` and `combined` and owns gauge range/fraction
semantics. Native faces draw the same instrument design using their platform
toolkit; product repositories provide colours/fonts and manufacturer-specific
parameter metadata only. A structured/text state without a meaningful numeric
range remains a text tile even when Dials or Combined is selected.

The existing Linux circular cockpit instrument is the visual reference: a
270-degree sweep, dark radial face, restrained accent arc, exact live value in
the centre and the parameter title beneath. Apple reproduces that geometry in
SwiftUI. Windows and Android main diagnostic faces must consume the same core
mode/range contract when those faces are present; they must not invent another
mode enum or independent scaling rules.
'''
if anchor not in doc:
    raise SystemExit('product face doc anchor not found')
docs.write_text(doc.replace(anchor, addition, 1))

Path('.github/workflows/hal-dashboard-modes.yml').unlink()
Path('.github/hal_dashboard_migrate.py').unlink()
