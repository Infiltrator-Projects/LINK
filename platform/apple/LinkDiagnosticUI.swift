// SPDX-License-Identifier: GPL-3.0-or-later
#if canImport(SwiftUI)
import SwiftUI

/*
 * LINK-owned SwiftUI presentation primitives.
 *
 * Product faces provide colours, typography and brand content. LINK owns the
 * geometry, spacing and diagnostic information architecture so MBLINK,
 * JAGLINK and future product faces cannot silently drift apart.
 */
struct LinkDiagnosticTypography {
    let display: Font
    let body: Font
    let bodyBold: Font
    let subheadline: Font
    let subheadlineBold: Font
    let headline: Font
    let caption: Font
    let captionBold: Font
    let caption2: Font
    let caption2Bold: Font
    let title3: Font
    let title2: Font
}

struct LinkDiagnosticTheme {
    let backgroundTop: Color
    let backgroundMiddle: Color
    let backgroundBottom: Color
    let panel: Color
    let panelRaised: Color
    let primaryText: Color
    let secondaryText: Color
    let mutedText: Color
    let border: Color
    let accent: Color
    let success: Color
    let warning: Color
    let fault: Color
    let typography: LinkDiagnosticTypography

    static let neutral = LinkDiagnosticTheme(
        backgroundTop: Color(red: 0.02, green: 0.02, blue: 0.025),
        backgroundMiddle: Color(red: 0.035, green: 0.035, blue: 0.045),
        backgroundBottom: Color(red: 0.055, green: 0.055, blue: 0.065),
        panel: Color(red: 0.075, green: 0.075, blue: 0.085),
        panelRaised: Color(red: 0.10, green: 0.10, blue: 0.115),
        primaryText: Color.white,
        secondaryText: Color.white.opacity(0.78),
        mutedText: Color.white.opacity(0.55),
        border: Color.white.opacity(0.18),
        accent: Color.white,
        success: Color.green,
        warning: Color.orange,
        fault: Color.red,
        typography: LinkDiagnosticTypography(
            display: .system(size: 29, weight: .semibold),
            body: .body,
            bodyBold: .body.bold(),
            subheadline: .subheadline,
            subheadlineBold: .subheadline.bold(),
            headline: .headline,
            caption: .caption,
            captionBold: .caption.bold(),
            caption2: .caption2,
            caption2Bold: .caption2.bold(),
            title3: .title3,
            title2: .title2.bold()))
}

private struct LinkDiagnosticThemeKey: EnvironmentKey {
    static let defaultValue = LinkDiagnosticTheme.neutral
}

struct LinkDiagnosticLocalizer {
    let resolve: (String) -> String

    func text(_ key: String, fallback: String) -> String {
        let value = resolve(key)
        return value.isEmpty || value == key ? fallback : value
    }

    static let fallback = LinkDiagnosticLocalizer(resolve: { $0 })
}

private struct LinkDiagnosticLocalizerKey: EnvironmentKey {
    static let defaultValue = LinkDiagnosticLocalizer.fallback
}

extension EnvironmentValues {
    var linkDiagnosticTheme: LinkDiagnosticTheme {
        get { self[LinkDiagnosticThemeKey.self] }
        set { self[LinkDiagnosticThemeKey.self] = newValue }
    }

    var linkDiagnosticLocalizer: LinkDiagnosticLocalizer {
        get { self[LinkDiagnosticLocalizerKey.self] }
        set { self[LinkDiagnosticLocalizerKey.self] = newValue }
    }
}

extension View {
    func linkDiagnosticTheme(_ theme: LinkDiagnosticTheme) -> some View {
        environment(\.linkDiagnosticTheme, theme)
    }

    func linkDiagnosticLocalization(
        _ resolve: @escaping (String) -> String
    ) -> some View {
        environment(
            \.linkDiagnosticLocalizer,
            LinkDiagnosticLocalizer(resolve: resolve))
    }

    func linkDiagnosticScreen(_ title: String) -> some View {
        modifier(LinkDiagnosticScreenModifier(title: title))
    }
}

enum LinkDiagnosticLayout {
    static let screenHorizontalPadding: CGFloat = 20
    static let screenTopPadding: CGFloat = 18
    static let screenBottomPadding: CGFloat = 30
    static let sectionSpacing: CGFloat = 18
    static let gridSpacing: CGFloat = 14
    static let panelPadding: CGFloat = 16
    static let panelCornerRadius: CGFloat = 18
    static let tilePadding: CGFloat = 16
    static let tileCornerRadius: CGFloat = 19
    static let tileMinimumHeight: CGFloat = 118
    static let compactRowVerticalPadding: CGFloat = 6
    static let headerSpacing: CGFloat = 14

    static var dashboardColumns: [GridItem] {
        [
            GridItem(.flexible(), spacing: gridSpacing),
            GridItem(.flexible(), spacing: gridSpacing)
        ]
    }
}

struct LinkDiagnosticBackground: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    var body: some View {
        LinearGradient(
            stops: [
                .init(color: theme.backgroundTop, location: 0.0),
                .init(color: theme.backgroundMiddle, location: 0.55),
                .init(color: theme.backgroundBottom, location: 1.0)
            ],
            startPoint: .top,
            endPoint: .bottomTrailing)
            .ignoresSafeArea()
    }
}

struct LinkStatusPill: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let text: String
    let active: Bool

    var body: some View {
        HStack(spacing: 7) {
            Circle()
                .fill(active ? theme.success : theme.mutedText)
                .frame(width: 7, height: 7)
            Text(LocalizedStringKey(text))
                .textCase(.uppercase)
                .font(theme.typography.caption2Bold)
                .tracking(0.8)
                .lineLimit(1)
        }
        .foregroundStyle(theme.primaryText)
        .padding(.horizontal, 10)
        .padding(.vertical, 7)
        .background(Capsule().fill(theme.panelRaised))
        .overlay(Capsule().stroke(theme.border, lineWidth: 1))
    }
}

struct LinkPanel<Content: View>: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        content
            .padding(LinkDiagnosticLayout.panelPadding)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(
                RoundedRectangle(
                    cornerRadius: LinkDiagnosticLayout.panelCornerRadius,
                    style: .continuous)
                    .fill(theme.panel))
            .overlay(
                RoundedRectangle(
                    cornerRadius: LinkDiagnosticLayout.panelCornerRadius,
                    style: .continuous)
                    .stroke(theme.border, lineWidth: 1))
    }
}

struct LinkLabeledPanel<Content: View>: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let title: String
    let systemImage: String
    let content: Content

    init(
        title: String,
        systemImage: String,
        @ViewBuilder content: () -> Content
    ) {
        self.title = title
        self.systemImage = systemImage
        self.content = content()
    }

    var body: some View {
        LinkPanel {
            VStack(alignment: .leading, spacing: 14) {
                Label(LocalizedStringKey(title), systemImage: systemImage)
                    .font(theme.typography.headline)
                    .foregroundStyle(theme.primaryText)
                content
            }
        }
    }
}

struct LinkSectionHeader: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let title: String
    var kicker: String? = nil

    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            if let kicker {
                Text(LocalizedStringKey(kicker))
                    .textCase(.uppercase)
                    .font(theme.typography.caption2Bold)
                    .tracking(1.4)
                    .foregroundStyle(theme.mutedText)
            }
            Text(LocalizedStringKey(title))
                .font(theme.typography.title3)
                .foregroundStyle(theme.primaryText)
        }
    }
}

struct LinkBrandHeader<Brand: View, Status: View>: View {
    let brand: Brand
    let status: Status

    init(
        @ViewBuilder brand: () -> Brand,
        @ViewBuilder status: () -> Status
    ) {
        self.brand = brand()
        self.status = status()
    }

    var body: some View {
        ViewThatFits(in: .horizontal) {
            HStack(alignment: .center, spacing: LinkDiagnosticLayout.headerSpacing) {
                brand
                Spacer(minLength: 8)
                status
            }
            VStack(alignment: .leading, spacing: 11) {
                brand
                status
            }
        }
    }
}

struct LinkDiagnosticGrid<Content: View>: View {
    let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        LazyVGrid(
            columns: LinkDiagnosticLayout.dashboardColumns,
            spacing: LinkDiagnosticLayout.gridSpacing) {
                content
            }
    }
}

enum LinkDiagnosticTask: CaseIterable {
    case vehicle
    case log
    case errors
    case dashboard
    case table
    case graph
    case tests
    case services
    case settings

    var title: String {
        switch self {
        case .vehicle: return "Vehicle"
        case .log: return "Log"
        case .errors: return "Errors"
        case .dashboard: return "Dashboard"
        case .table: return "Table"
        case .graph: return "Graph"
        case .tests: return "Tests"
        case .services: return "Services"
        case .settings: return "Settings"
        }
    }

    var subtitle: String {
        switch self {
        case .vehicle: return "Identity, connection, networks and modules"
        case .log: return "Chronological diagnostic session activity"
        case .errors: return "Standard and manufacturer fault memory"
        case .dashboard: return "Selected live measurements at a glance"
        case .table: return "Search and view live diagnostic parameters"
        case .graph: return "Selected parameters over time"
        case .tests: return "Readiness, monitor results and self-tests"
        case .services: return "Supported service procedures"
        case .settings: return "Display, adapter, units, logging and application preferences"
        }
    }

    var titleKey: String {
        switch self {
        case .vehicle: return "nav.vehicle"
        case .log: return "nav.log"
        case .errors: return "nav.errors"
        case .dashboard: return "nav.dashboard"
        case .table: return "nav.table"
        case .graph: return "nav.graph"
        case .tests: return "nav.tests"
        case .services: return "nav.services"
        case .settings: return "nav.settings"
        }
    }

    var subtitleKey: String {
        titleKey + ".summary"
    }

    var symbol: String {
        switch self {
        case .vehicle: return "car.side.fill"
        case .log: return "list.bullet.rectangle"
        case .errors: return "exclamationmark.triangle.fill"
        case .dashboard: return "gauge.with.dots.needle.67percent"
        case .table: return "tablecells"
        case .graph: return "chart.xyaxis.line"
        case .tests: return "checkmark.square.fill"
        case .services: return "wrench.and.screwdriver.fill"
        case .settings: return "gearshape.fill"
        }
    }
}

struct LinkTaskTile<Destination: View>: View {
    @Environment(\.linkDiagnosticLocalizer) private var localizer
    let task: LinkDiagnosticTask
    let destination: () -> Destination

    init(
        _ task: LinkDiagnosticTask,
        @ViewBuilder destination: @escaping () -> Destination
    ) {
        self.task = task
        self.destination = destination
    }

    var body: some View {
        LinkHomeTile(
            localizer.text(task.titleKey, fallback: task.title),
            localizer.text(task.subtitleKey, fallback: task.subtitle),
            task.symbol,
            destination: destination)
    }
}

struct LinkHomeTile<Destination: View>: View {
    let title: String
    let subtitle: String
    let symbol: String
    let destination: () -> Destination

    init(
        _ title: String,
        _ subtitle: String,
        _ symbol: String,
        @ViewBuilder destination: @escaping () -> Destination
    ) {
        self.title = title
        self.subtitle = subtitle
        self.symbol = symbol
        self.destination = destination
    }

    var body: some View {
        NavigationLink {
            destination()
        } label: {
            LinkTileFace(title: title, subtitle: subtitle, symbol: symbol)
        }
        .buttonStyle(.plain)
    }
}

struct LinkActionTile: View {
    let title: String
    let subtitle: String
    let symbol: String
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            LinkTileFace(title: title, subtitle: subtitle, symbol: symbol)
        }
        .buttonStyle(.plain)
    }
}

struct LinkTileFace: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let title: String
    let subtitle: String
    let symbol: String

    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            Image(systemName: symbol)
                .font(theme.typography.title2)
                .foregroundStyle(theme.accent)
                .frame(width: 30, height: 30, alignment: .leading)

            Text(LocalizedStringKey(title))
                .font(theme.typography.headline)
                .foregroundStyle(theme.primaryText)
                .lineLimit(1)

            Text(LocalizedStringKey(subtitle))
                .font(theme.typography.caption)
                .foregroundStyle(theme.mutedText)
                .lineLimit(2)
                .fixedSize(horizontal: false, vertical: true)

            Spacer(minLength: 0)

            HStack {
                Spacer()
                Image(systemName: "chevron.right")
                    .font(theme.typography.captionBold)
                    .foregroundStyle(theme.secondaryText.opacity(0.72))
            }
        }
        .frame(
            maxWidth: .infinity,
            minHeight: LinkDiagnosticLayout.tileMinimumHeight,
            alignment: .leading)
        .padding(LinkDiagnosticLayout.tilePadding)
        .background(
            RoundedRectangle(
                cornerRadius: LinkDiagnosticLayout.tileCornerRadius,
                style: .continuous)
                .fill(
                    LinearGradient(
                        colors: [theme.panelRaised, theme.panel],
                        startPoint: .topLeading,
                        endPoint: .bottomTrailing)))
        .overlay(
            RoundedRectangle(
                cornerRadius: LinkDiagnosticLayout.tileCornerRadius,
                style: .continuous)
                .stroke(theme.border, lineWidth: 1))
    }
}

struct LinkCompactLink<Destination: View>: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let title: String
    let subtitle: String
    let symbol: String
    let destination: () -> Destination

    init(
        _ title: String,
        _ subtitle: String,
        _ symbol: String,
        @ViewBuilder destination: @escaping () -> Destination
    ) {
        self.title = title
        self.subtitle = subtitle
        self.symbol = symbol
        self.destination = destination
    }

    var body: some View {
        NavigationLink {
            destination()
        } label: {
            HStack(spacing: 12) {
                Image(systemName: symbol)
                    .font(theme.typography.title3)
                    .foregroundStyle(theme.accent)
                    .frame(width: 28)
                VStack(alignment: .leading, spacing: 2) {
                    Text(LocalizedStringKey(title))
                        .font(theme.typography.subheadlineBold)
                        .foregroundStyle(theme.primaryText)
                    Text(LocalizedStringKey(subtitle))
                        .font(theme.typography.caption)
                        .foregroundStyle(theme.mutedText)
                        .lineLimit(1)
                }
                Spacer(minLength: 10)
                Image(systemName: "chevron.right")
                    .font(theme.typography.captionBold)
                    .foregroundStyle(theme.mutedText)
            }
            .contentShape(Rectangle())
            .padding(.vertical, LinkDiagnosticLayout.compactRowVerticalPadding)
        }
        .buttonStyle(.plain)
    }
}

struct LinkCommandCentreShell<
    Header: View,
    Progress: View,
    Connection: View,
    Primary: View,
    Tools: View
>: View {
    @Environment(\.linkDiagnosticTheme) private var theme

    let showProgress: Bool
    let diagnosticsTitle: String
    let diagnosticsKicker: String?
    let header: Header
    let progress: Progress
    let connection: Connection
    let primary: Primary
    let tools: Tools

    init(
        showProgress: Bool,
        diagnosticsTitle: String = "Diagnostics",
        diagnosticsKicker: String? = "Vehicle",
        @ViewBuilder header: () -> Header,
        @ViewBuilder progress: () -> Progress,
        @ViewBuilder connection: () -> Connection,
        @ViewBuilder primary: () -> Primary,
        @ViewBuilder tools: () -> Tools
    ) {
        self.showProgress = showProgress
        self.diagnosticsTitle = diagnosticsTitle
        self.diagnosticsKicker = diagnosticsKicker
        self.header = header()
        self.progress = progress()
        self.connection = connection()
        self.primary = primary()
        self.tools = tools()
    }

    var body: some View {
        NavigationStack {
            ZStack {
                LinkDiagnosticBackground()
                ScrollView {
                    VStack(alignment: .leading, spacing: LinkDiagnosticLayout.sectionSpacing) {
                        header
                        if showProgress {
                            progress
                        }
                        connection
                        LinkSectionHeader(
                            title: diagnosticsTitle,
                            kicker: diagnosticsKicker)
                        primary
                        tools
                    }
                    .padding(.horizontal, LinkDiagnosticLayout.screenHorizontalPadding)
                    .padding(.top, LinkDiagnosticLayout.screenTopPadding)
                    .padding(.bottom, LinkDiagnosticLayout.screenBottomPadding)
                }
            }
            .toolbar(.hidden, for: .navigationBar)
            .tint(theme.accent)
        }
    }
}

private struct LinkDiagnosticScreenModifier: ViewModifier {
    @Environment(\.linkDiagnosticTheme) private var theme
    let title: String

    func body(content: Content) -> some View {
        content
            .background(theme.backgroundMiddle.ignoresSafeArea())
            .navigationTitle(LocalizedStringKey(title))
            .navigationBarTitleDisplayMode(.inline)
            .toolbarBackground(theme.backgroundTop, for: .navigationBar)
            .toolbarBackground(.visible, for: .navigationBar)
            .toolbarColorScheme(.dark, for: .navigationBar)
    }
}
struct LinkSettingOption: Identifiable, Hashable {
    let id: String
    let title: String
}

struct LinkDiagnosticSettingsView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @Environment(\.linkDiagnosticLocalizer) private var localizer

    let languageOptions: [LinkSettingOption]
    @Binding var selectedLanguageID: String
    let measurementOptions: [LinkSettingOption]
    @Binding var selectedMeasurementID: String
    let productName: String
    let productVersion: String
    let adapterName: String
    let coreSummary: String

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: LinkDiagnosticLayout.sectionSpacing) {
                LinkLabeledPanel(
                    title: localizer.text("nav.settings", fallback: "Settings"),
                    systemImage: "gearshape.fill"
                ) {
                    VStack(alignment: .leading, spacing: 8) {
                        Text(localizer.text("language.label", fallback: "Language"))
                            .font(theme.typography.subheadlineBold)
                            .foregroundStyle(theme.primaryText)
                        Picker(
                            localizer.text("language.label", fallback: "Language"),
                            selection: $selectedLanguageID
                        ) {
                            ForEach(languageOptions) { option in
                                Text(option.title).tag(option.id)
                            }
                        }
                        .pickerStyle(.menu)
                    }

                    Divider().overlay(theme.border)

                    VStack(alignment: .leading, spacing: 8) {
                        Text(localizer.text("units.label", fallback: "Measurement units"))
                            .font(theme.typography.subheadlineBold)
                            .foregroundStyle(theme.primaryText)
                        Picker(
                            localizer.text("units.label", fallback: "Measurement units"),
                            selection: $selectedMeasurementID
                        ) {
                            ForEach(measurementOptions) { option in
                                Text(option.title).tag(option.id)
                            }
                        }
                        .pickerStyle(.segmented)
                    }
                }

                LinkLabeledPanel(
                    title: localizer.text("common.about", fallback: "About"),
                    systemImage: "info.circle.fill"
                ) {
                    settingsRow(productName, productVersion)
                    Divider().overlay(theme.border)
                    settingsRow(
                        localizer.text("common.adapter", fallback: "Adapter"),
                        adapterName)
                    Divider().overlay(theme.border)
                    settingsRow("LINK", coreSummary)
                }
            }
            .padding(.horizontal, LinkDiagnosticLayout.screenHorizontalPadding)
            .padding(.top, LinkDiagnosticLayout.screenTopPadding)
            .padding(.bottom, LinkDiagnosticLayout.screenBottomPadding)
        }
        .linkDiagnosticScreen(
            localizer.text("nav.settings", fallback: "Settings"))
    }

    @ViewBuilder
    private func settingsRow(_ label: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(label)
                .font(theme.typography.captionBold)
                .foregroundStyle(theme.secondaryText)
            Spacer(minLength: 12)
            Text(value)
                .font(theme.typography.caption)
                .foregroundStyle(theme.primaryText)
                .multilineTextAlignment(.trailing)
        }
    }
}

struct LinkStandardObdSnapshot {
    let capability: String
    let capabilityDetail: String
    let vin: String
    let responderSummary: String
    let pidSummary: String
    let readiness: String
    let readinessMonitors: [String]
    let freezeFrame: [String]
    let storedDTCs: [String]
    let pendingDTCs: [String]
    let permanentDTCs: [String]
    let liveRows: [String]

    init(
        capability: String,
        capabilityDetail: String,
        vin: String,
        responderSummary: String,
        pidSummary: String,
        readiness: String,
        readinessMonitors: [String] = [],
        freezeFrame: [String] = [],
        storedDTCs: [String] = [],
        pendingDTCs: [String] = [],
        permanentDTCs: [String] = [],
        liveRows: [String] = []
    ) {
        self.capability = capability
        self.capabilityDetail = capabilityDetail
        self.vin = vin
        self.responderSummary = responderSummary
        self.pidSummary = pidSummary
        self.readiness = readiness
        self.readinessMonitors = readinessMonitors
        self.freezeFrame = freezeFrame
        self.storedDTCs = storedDTCs
        self.pendingDTCs = pendingDTCs
        self.permanentDTCs = permanentDTCs
        self.liveRows = liveRows
    }
}

struct LinkStandardObdView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let snapshot: LinkStandardObdSnapshot

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: LinkDiagnosticLayout.sectionSpacing) {
                LinkLabeledPanel(title: "Diagnostic generation", systemImage: "car.side") {
                    Text(snapshot.capability)
                        .font(theme.typography.headline)
                        .foregroundStyle(theme.accent)
                    Text(snapshot.capabilityDetail)
                        .font(theme.typography.caption)
                        .foregroundStyle(theme.mutedText)
                        .fixedSize(horizontal: false, vertical: true)
                }

                LinkLabeledPanel(title: "Standard OBD-II / EOBD", systemImage: "cpu") {
                    obdRow("VIN", snapshot.vin)
                    Divider().overlay(theme.border)
                    obdRow("Responders", snapshot.responderSummary)
                    Divider().overlay(theme.border)
                    obdRow("Mode 01 live data", snapshot.pidSummary)
                    Divider().overlay(theme.border)
                    obdRow("Readiness", snapshot.readiness)
                }

                if !snapshot.readinessMonitors.isEmpty {
                    LinkLabeledPanel(title: "Readiness monitors", systemImage: "checklist") {
                        ForEach(snapshot.readinessMonitors, id: \.self) { row in
                            Text(row)
                                .font(theme.typography.subheadline)
                                .foregroundStyle(theme.secondaryText)
                        }
                    }
                }

                LinkLabeledPanel(title: "Fault memory", systemImage: "exclamationmark.triangle") {
                    faultGroup("Stored", snapshot.storedDTCs)
                    faultGroup("Pending", snapshot.pendingDTCs)
                    faultGroup("Permanent", snapshot.permanentDTCs)
                }

                LinkLabeledPanel(title: "Freeze frame", systemImage: "camera.metering.matrix") {
                    if snapshot.freezeFrame.isEmpty {
                        Text("No Mode 02 frame-zero context captured.")
                            .font(theme.typography.subheadline)
                            .foregroundStyle(theme.mutedText)
                    } else {
                        ForEach(snapshot.freezeFrame, id: \.self) { row in
                            Text(row)
                                .font(theme.typography.subheadline)
                                .foregroundStyle(theme.secondaryText)
                        }
                    }
                }

                LinkLabeledPanel(title: "Live data", systemImage: "waveform.path.ecg") {
                    if snapshot.liveRows.isEmpty {
                        Text("No advertised standard live parameters yet.")
                            .font(theme.typography.subheadline)
                            .foregroundStyle(theme.mutedText)
                    } else {
                        ForEach(snapshot.liveRows, id: \.self) { row in
                            Text(row)
                                .font(theme.typography.subheadline)
                                .foregroundStyle(theme.secondaryText)
                        }
                    }
                }

                LinkLabeledPanel(title: "Common coverage", systemImage: "square.stack.3d.up") {
                    obdRow("Mode 01", "Supported-PID discovery, readiness and current data")
                    obdRow("Mode 02", "Bounded freeze-frame context")
                    obdRow("Modes 03 / 07 / 0A", "Stored, pending and permanent DTC inventory")
                    obdRow("Mode 09", "Vehicle information including standard VIN where available")
                    obdRow("OBDonUDS", "SAE J1979-2 foundation through LINK")
                }
            }
            .padding(.horizontal, LinkDiagnosticLayout.screenHorizontalPadding)
            .padding(.top, LinkDiagnosticLayout.screenTopPadding)
            .padding(.bottom, LinkDiagnosticLayout.screenBottomPadding)
        }
        .linkDiagnosticScreen("OBD")
    }

    @ViewBuilder
    private func faultGroup(_ title: String, _ values: [String]) -> some View {
        VStack(alignment: .leading, spacing: 4) {
            Text(title)
                .font(theme.typography.captionBold)
                .foregroundStyle(theme.mutedText)
            if values.isEmpty {
                Text("None reported")
                    .font(theme.typography.subheadline)
                    .foregroundStyle(theme.secondaryText)
            } else {
                ForEach(values, id: \.self) { value in
                    Text(value)
                        .font(theme.typography.subheadline)
                        .foregroundStyle(theme.secondaryText)
                }
            }
        }
    }

    private func obdRow(_ label: String, _ value: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 12) {
            Text(label)
                .font(theme.typography.captionBold)
                .foregroundStyle(theme.mutedText)
            Spacer(minLength: 8)
            Text(value)
                .font(theme.typography.subheadline)
                .foregroundStyle(theme.primaryText)
                .multilineTextAlignment(.trailing)
        }
    }
}

#endif
