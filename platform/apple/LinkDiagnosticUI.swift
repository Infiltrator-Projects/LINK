// SPDX-License-Identifier: GPL-3.0-or-later
#if canImport(SwiftUI)
import SwiftUI
import Foundation
import CoreBluetooth
import UIKit

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

struct LinkInterfaceLanguage: Identifiable, Hashable {
    let id: String
    let nativeName: String

    static let all: [LinkInterfaceLanguage] = {
        let count = Int(link_i18n_supported_locale_count())
        return (0..<count).compactMap { index in
            guard let locale = link_i18n_supported_locale(index),
                  let name = link_i18n_supported_locale_name(index) else { return nil }
            return LinkInterfaceLanguage(
                id: String(cString: locale),
                nativeName: String(cString: name))
        }
    }()

    static func canonical(
        _ stored: String,
        aliases: [String: String] = [:],
        fallback: String = "en-AU"
    ) -> String {
        let candidate = aliases[stored] ?? stored
        return all.contains(where: { $0.id == candidate }) ? candidate : fallback
    }

    static func displayName(
        for stored: String,
        aliases: [String: String] = [:],
        fallback: String = "en-AU"
    ) -> String {
        let code = canonical(stored, aliases: aliases, fallback: fallback)
        return all.first(where: { $0.id == code })?.nativeName
            ?? all.first(where: { $0.id == fallback })?.nativeName
            ?? fallback
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
        case .settings: return "Application preferences"
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


struct LinkDiagnosticAboutInfo {
    let productName: String
    let subtitle: String?
    let version: String
    let summary: String?
    let releaseDate: String?
    let authors: [String]
    let copyright: String?
    let website: URL?
    let licenseName: String?
    let licenseText: String?
    let credits: [String]

    init(
        productName: String,
        subtitle: String? = nil,
        version: String,
        summary: String? = nil,
        releaseDate: String? = nil,
        authors: [String] = [],
        copyright: String? = nil,
        website: URL? = nil,
        licenseName: String? = nil,
        licenseText: String? = nil,
        credits: [String] = []
    ) {
        self.productName = productName
        self.subtitle = subtitle
        self.version = version
        self.summary = summary
        self.releaseDate = releaseDate
        self.authors = authors
        self.copyright = copyright
        self.website = website
        self.licenseName = licenseName
        self.licenseText = licenseText
        self.credits = credits
    }
}

private enum LinkDiagnosticAboutDetail: String {
    case credits
    case license
}

struct LinkDiagnosticAboutView<Logo: View>: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let info: LinkDiagnosticAboutInfo
    let logo: Logo
    let onClose: () -> Void
    @State private var detail: LinkDiagnosticAboutDetail?

    init(
        info: LinkDiagnosticAboutInfo,
        onClose: @escaping () -> Void,
        @ViewBuilder logo: () -> Logo
    ) {
        self.info = info
        self.onClose = onClose
        self.logo = logo()
    }

    var body: some View {
        ZStack {
            LinkDiagnosticBackground()
            VStack(spacing: 0) {
                ScrollView {
                    if let detail {
                        detailContent(detail)
                    } else {
                        aboutContent
                    }
                }

                HStack(spacing: 10) {
                    if detail != nil {
                        Button("About") { detail = nil }
                            .buttonStyle(.bordered)
                    }
                    if !info.authors.isEmpty || !info.credits.isEmpty {
                        Button("Credits") { detail = .credits }
                            .buttonStyle(.bordered)
                    }
                    if hasLicense {
                        Button("License") { detail = .license }
                            .buttonStyle(.bordered)
                    }
                    Button("Close") { onClose() }
                        .buttonStyle(.borderedProminent)
                        .tint(theme.accent)
                }
                .frame(maxWidth: .infinity)
                .padding(.horizontal, 16)
                .padding(.vertical, 12)
                .background(theme.panelRaised)
                .overlay(alignment: .top) {
                    Rectangle().fill(theme.border).frame(height: 1)
                }
            }
        }
        .presentationDetents([.medium, .large])
        .presentationDragIndicator(.visible)
    }

    private var aboutContent: some View {
        VStack(spacing: 17) {
            logo.padding(.top, 30)

            VStack(spacing: 4) {
                Text(info.productName)
                    .font(theme.typography.display)
                    .foregroundStyle(theme.primaryText)
                if let subtitle = info.subtitle, !subtitle.isEmpty {
                    Text(subtitle)
                        .font(theme.typography.caption2Bold)
                        .textCase(.uppercase)
                        .tracking(1.4)
                        .foregroundStyle(theme.secondaryText)
                }
            }

            Text("Version \(info.version)")
                .font(theme.typography.subheadline)
                .foregroundStyle(theme.mutedText)

            if let summary = info.summary, !summary.isEmpty {
                Text(summary)
                    .font(theme.typography.body)
                    .multilineTextAlignment(.center)
                    .foregroundStyle(theme.primaryText)
                    .padding(.horizontal, 28)
            }

            if let releaseDate = info.releaseDate, !releaseDate.isEmpty {
                Text("Release date: \(releaseDate)")
                    .font(theme.typography.subheadline)
                    .foregroundStyle(theme.mutedText)
            }

            if let copyright = info.copyright, !copyright.isEmpty {
                Text(copyright)
                    .font(theme.typography.subheadline)
                    .foregroundStyle(theme.mutedText)
            }

            if let website = info.website {
                Link("Project Website", destination: website)
                    .font(theme.typography.bodyBold)
                    .foregroundStyle(theme.accent)
            }
        }
        .frame(maxWidth: .infinity)
    }

    @ViewBuilder
    private func detailContent(_ detail: LinkDiagnosticAboutDetail) -> some View {
        VStack(spacing: 17) {
            logo
                .padding(.top, 24)

            Text(detail == .credits ? "Credits" : "License")
                .font(theme.typography.title2)
                .foregroundStyle(theme.primaryText)

            LinkPanel {
                Text(detail == .credits ? creditsText : licenseText)
                    .font(theme.typography.body)
                    .foregroundStyle(theme.primaryText)
                    .frame(maxWidth: .infinity, alignment: .leading)
            }
            .padding(.horizontal, 16)
            .padding(.bottom, 18)
        }
        .frame(maxWidth: .infinity)
    }

    private var hasLicense: Bool {
        if let text = info.licenseText, !text.isEmpty { return true }
        if let name = info.licenseName, !name.isEmpty { return true }
        return false
    }

    private var creditsText: String {
        var sections: [String] = []
        if !info.authors.isEmpty {
            sections.append(
                (info.authors.count == 1 ? "Author\n" : "Authors\n") +
                info.authors.joined(separator: "\n"))
        }
        if !info.credits.isEmpty {
            sections.append(info.credits.joined(separator: "\n"))
        }
        return sections.joined(separator: "\n\n")
    }

    private var licenseText: String {
        if let text = info.licenseText, !text.isEmpty { return text }
        return info.licenseName ?? ""
    }
}

extension LinkDiagnosticAboutView where Logo == EmptyView {
    init(info: LinkDiagnosticAboutInfo, onClose: @escaping () -> Void) {
        self.init(info: info, onClose: onClose) { EmptyView() }
    }
}

struct LinkDiagnosticAboutButton: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let productName: String
    let copyright: String?
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 8) {
                Text(productName).font(theme.typography.captionBold)
                if let copyright, !copyright.isEmpty {
                    Text(copyright)
                        .font(theme.typography.caption)
                        .foregroundStyle(theme.mutedText)
                }
                Spacer(minLength: 8)
                Label("About", systemImage: "info.circle")
                    .font(theme.typography.caption)
            }
            .foregroundStyle(theme.secondaryText)
            .padding(.horizontal, 16)
            .padding(.vertical, 9)
            .frame(maxWidth: .infinity)
            .background(theme.panelRaised)
            .overlay(alignment: .top) {
                Rectangle().fill(theme.border).frame(height: 1)
            }
        }
        .buttonStyle(.plain)
    }
}



// MARK: - Shared diagnostic presentation models

struct LinkDiagnosticParameter: Identifiable {
    let id: String
    let protocolName: String
    let moduleIdentifier: UInt32
    let parameterIdentifier: UInt32
    let shortName: String
    let title: String
    let suffix: String
    let formattedValue: String
    let value: Double?
    let structuredValue: String?
    let rawHex: String?
    let vehicleSupported: Bool
    let favourite: Bool
    let pollingEnabled: Bool
    let history: [Double]
    let sourceLabel: String?
    let qualityNote: String?
    let dashboardMinimum: Double?
    let dashboardMaximum: Double?

    init(
        id: String,
        protocolName: String,
        moduleIdentifier: UInt32,
        parameterIdentifier: UInt32,
        shortName: String,
        title: String,
        suffix: String,
        formattedValue: String,
        value: Double?,
        structuredValue: String?,
        rawHex: String?,
        vehicleSupported: Bool,
        favourite: Bool,
        pollingEnabled: Bool,
        history: [Double],
        sourceLabel: String?,
        qualityNote: String?,
        dashboardMinimum: Double? = nil,
        dashboardMaximum: Double? = nil
    ) {
        self.id = id
        self.protocolName = protocolName
        self.moduleIdentifier = moduleIdentifier
        self.parameterIdentifier = parameterIdentifier
        self.shortName = shortName
        self.title = title
        self.suffix = suffix
        self.formattedValue = formattedValue
        self.value = value
        self.structuredValue = structuredValue
        self.rawHex = rawHex
        self.vehicleSupported = vehicleSupported
        self.favourite = favourite
        self.pollingEnabled = pollingEnabled
        self.history = history
        self.sourceLabel = sourceLabel
        self.qualityNote = qualityNote
        self.dashboardMinimum = dashboardMinimum
        self.dashboardMaximum = dashboardMaximum
    }

    var isAvailable: Bool { value != nil || !(structuredValue ?? "").isEmpty }
    var isSupported: Bool { vehicleSupported }
    var presentationValue: String {
        if value != nil {
            return formattedValue == "N/A" ? "Decode error" : formattedValue
        }
        if let structuredValue, !structuredValue.isEmpty { return structuredValue }
        if !vehicleSupported { return "Not advertised" }
        if !pollingEnabled { return "Not polled" }
        return "Waiting for sample"
    }
    var hasLiveValue: Bool { pollingEnabled && isAvailable }
    var dashboardDialSupported: Bool {
        guard structuredValue == nil,
              let minimum = dashboardMinimum,
              let maximum = dashboardMaximum,
              minimum.isFinite, maximum.isFinite, maximum > minimum else {
            return false
        }
        return true
    }
    var dashboardFraction: Double? {
        guard let value, dashboardDialSupported,
              let minimum = dashboardMinimum,
              let maximum = dashboardMaximum else { return nil }
        return min(1.0, max(0.0, (value - minimum) / (maximum - minimum)))
    }
    var pidText: String {
        let value = String(parameterIdentifier, radix: 16, uppercase: true)
        return "0x" + (value.count < 2 ? "0\(value)" : value)
    }
    var sourceText: String {
        protocolName.lowercased() == "obd2"
            ? "SAE OBD-II · \(pidText)"
            : "\(protocolName.uppercased()) · \(pidText)"
    }
}

struct LinkDiagnosticModule: Identifiable {
    let id: String
    let name: String
    let designation: String
    let network: String
    let kind: String
    let protocolName: String
    let requestCANIdentifier: UInt32
    let responseCANIdentifier: UInt32
    let extendedID: Bool
    let identityText: String?
    let partNumber: String?
    let softwareNumber: String?
    let hardwareNumber: String?
    let faultStatus: String
    let faultCount: Int
    let faults: [String]
    let evidenceDetails: [String]
    let obdAdvertisedPIDCount: Int
    let livePIDCount: Int

    var addressText: String {
        if extendedID {
            return String(format: "0x%08X → 0x%08X",
                          requestCANIdentifier, responseCANIdentifier)
        }
        return String(format: "0x%03X → 0x%03X",
                      requestCANIdentifier, responseCANIdentifier)
    }

    var faultCountLabel: String {
        if faultCount > 0 { return "\(faultCount) fault\(faultCount == 1 ? "" : "s")" }
        if faultStatus == "Checked · no faults" { return "0 faults" }
        return "faults unknown"
    }
}

struct LinkPIDConfigurationItem: Identifiable {
    let id: String
    let pid: UInt8
    let shortName: String
    let title: String
    let pollingEnabled: Bool
    let favourite: Bool
    let advertised: Bool
}

public enum LinkEvidenceExport {
    public static func prepareTemporaryCSV(_ data: Data, productName: String) async throws -> URL {
        let filename = "\(productName)-diagnostic-evidence-\(UUID().uuidString).csv"
        let url = FileManager.default.temporaryDirectory.appendingPathComponent(filename)
        do {
            try await Task.detached(priority: .utility) {
                try data.write(to: url, options: .atomic)
            }.value
            return url
        } catch {
            try? FileManager.default.removeItem(at: url)
            throw error
        }
    }

    public static func removeTemporaryFile(_ url: URL?) {
        guard let url else { return }
        try? FileManager.default.removeItem(at: url)
    }
}

struct LinkSavedVehicleProfileSummary: Identifiable {
    let id: String
    let vin: String
    let displayName: String
    let moduleCount: Int
    let responderCount: Int
    let updatedAt: Date?
    let adapterIdentifier: String?

    init(
        id: String,
        vin: String,
        displayName: String,
        moduleCount: Int,
        responderCount: Int,
        updatedAt: Date?,
        adapterIdentifier: String? = nil
    ) {
        self.id = id
        self.vin = vin
        self.displayName = displayName
        self.moduleCount = moduleCount
        self.responderCount = responderCount
        self.updatedAt = updatedAt
        self.adapterIdentifier = adapterIdentifier
    }

    init?(
        profile: NSDictionary,
        moduleCount: Int,
        fallbackDisplayName: String
    ) {
        guard let vin = profile["vin"] as? String, vin.count == 17 else {
            return nil
        }
        let displayName = (profile["displayName"] as? String)
            ?? fallbackDisplayName
        let timestamp = (profile["updatedAt"] as? NSNumber)?.doubleValue
        self.init(
            id: vin,
            vin: vin,
            displayName: displayName,
            moduleCount: moduleCount,
            responderCount: Int(LinkVehicleProfileStandardResponderCount(profile as? [AnyHashable: Any])),
            updatedAt: timestamp.map { Date(timeIntervalSince1970: $0) },
            adapterIdentifier: profile["adapterIdentifier"] as? String)
    }
}

struct LinkDiagnosticFault: Identifiable {
    let code: String
    let title: String
    let system: String
    let category: String
    let origin: String
    let source: String
    let state: String
    let definitionKnown: Bool

    var id: String { "\(state):\(code)" }
    var displayText: String { "\(code) — \(title)" }
}

struct LinkVehicleFact: Identifiable {
    let label: String
    let value: String
    var monospaced = false
    var id: String { label }
}

struct LinkInfoRow: View {
    @Environment(\.horizontalSizeClass) private var horizontalSizeClass
    @Environment(\.linkDiagnosticTheme) private var theme

    let label: String
    let value: String
    var monospaced = false

    private var valueText: some View {
        Text(LocalizedStringKey(value))
            .font(monospaced ? theme.typography.subheadline : theme.typography.subheadlineBold)
            .foregroundStyle(theme.primaryText)
            .fixedSize(horizontal: false, vertical: true)
            .textSelection(.enabled)
    }

    var body: some View {
        Group {
            if horizontalSizeClass == .compact {
                VStack(alignment: .leading, spacing: 5) {
                    Text(LocalizedStringKey(label))
                        .font(theme.typography.captionBold)
                        .foregroundStyle(theme.mutedText)
                        .textCase(.uppercase)
                        .tracking(0.45)
                    valueText
                        .multilineTextAlignment(.leading)
                        .frame(maxWidth: .infinity, alignment: .leading)
                }
            } else {
                HStack(alignment: .firstTextBaseline, spacing: 14) {
                    Text(LocalizedStringKey(label))
                        .font(theme.typography.subheadline)
                        .foregroundStyle(theme.mutedText)
                        .fixedSize(horizontal: true, vertical: false)
                    Spacer(minLength: 16)
                    valueText
                        .multilineTextAlignment(.trailing)
                        .frame(maxWidth: 420, alignment: .trailing)
                }
            }
        }
        .padding(.vertical, 6)
    }
}

private struct LinkVehicleFactTile: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let fact: LinkVehicleFact

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text(LocalizedStringKey(fact.label)).textCase(.uppercase)
                .font(theme.typography.caption2Bold)
                .tracking(0.8)
                .foregroundStyle(theme.mutedText)
            Text(fact.value)
                .font(fact.monospaced ? theme.typography.subheadline : theme.typography.subheadlineBold)
                .foregroundStyle(theme.primaryText)
                .lineLimit(3)
                .minimumScaleFactor(0.8)
                .textSelection(.enabled)
        }
        .frame(maxWidth: .infinity, minHeight: 64, alignment: .topLeading)
        .padding(12)
        .background(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .fill(theme.panelRaised))
        .overlay(
            RoundedRectangle(cornerRadius: 12, style: .continuous)
                .stroke(theme.border.opacity(0.75), lineWidth: 1))
    }
}

struct LinkVehicleFactGrid: View {
    let facts: [LinkVehicleFact]
    private let columns = [
        GridItem(.adaptive(minimum: 132, maximum: 260), spacing: 10)
    ]

    var body: some View {
        LazyVGrid(columns: columns, alignment: .leading, spacing: 10) {
            ForEach(facts) { fact in LinkVehicleFactTile(fact: fact) }
        }
    }
}

enum LinkDashboardPresentationMode: String, CaseIterable, Identifiable {
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

struct LinkMetricTile: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let parameter: LinkDiagnosticParameter

    var body: some View {
        VStack(alignment: .leading, spacing: 9) {
            HStack {
                Text(LocalizedStringKey(parameter.shortName)).textCase(.uppercase)
                    .font(theme.typography.caption2Bold)
                    .tracking(0.7)
                    .foregroundStyle(theme.secondaryText)
                Spacer()
                Text(parameter.pidText)
                    .font(theme.typography.caption2)
                    .foregroundStyle(theme.mutedText)
            }
            Text(parameter.presentationValue)
                .font(theme.typography.title2)
                .monospacedDigit()
                .foregroundStyle(parameter.hasLiveValue ? theme.primaryText : theme.mutedText)
                .minimumScaleFactor(0.65)
                .lineLimit(1)
            Text(LocalizedStringKey(parameter.title))
                .font(theme.typography.caption)
                .foregroundStyle(theme.mutedText)
                .lineLimit(2)
            if let source = parameter.sourceLabel {
                Label(source, systemImage: "cpu")
                    .font(theme.typography.caption2Bold)
                    .foregroundStyle(theme.secondaryText)
                    .lineLimit(2)
            }
            if let qualityNote = parameter.qualityNote {
                Text(qualityNote)
                    .font(theme.typography.caption2)
                    .foregroundStyle(theme.warning)
                    .lineLimit(2)
            }
        }
        .frame(maxWidth: .infinity, minHeight: 132, alignment: .topLeading)
        .padding(14)
        .background(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .fill(theme.panelRaised))
        .overlay(
            RoundedRectangle(cornerRadius: 16, style: .continuous)
                .stroke(theme.border, lineWidth: 1))
    }
}

/**
 * LINK-owned connection source chooser shared by product faces.
 *
 * The product supplies the currently selected vehicle and its optional
 * per-vehicle adapter association. LINK presents the known adapter, all
 * nearby BLE candidates and simulation as separate choices; the product then
 * starts the selected source through its shared LINK controller.
 */
enum LinkConnectionSource {
    case automatic
    case simulated
    case peripheral(String)
}

private struct LinkNearbyAdapter {
    let identifier: String
    let name: String
    let rssi: Int
}

final class LinkConnectionPickerViewController: UITableViewController,
    CBCentralManagerDelegate {

    private let vehicleText: String
    private let knownAdapterIdentifier: String?
    private let onSelection: (LinkConnectionSource) -> Void
    private var central: CBCentralManager?
    private var adaptersByIdentifier = [String: LinkNearbyAdapter]()
    private var adapterDiscoveryOrder = [String]()

    private var nearbyAdapters: [LinkNearbyAdapter] {
        adapterDiscoveryOrder.compactMap { adaptersByIdentifier[$0] }
    }

    init(
        vehicleText: String,
        knownAdapterIdentifier: String?,
        onSelection: @escaping (LinkConnectionSource) -> Void
    ) {
        self.vehicleText = vehicleText
        self.knownAdapterIdentifier = knownAdapterIdentifier
        self.onSelection = onSelection
        super.init(style: .insetGrouped)
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        title = "Connect"
        navigationItem.leftBarButtonItem = UIBarButtonItem(
            barButtonSystemItem: .cancel,
            target: self,
            action: #selector(cancel))
        navigationItem.rightBarButtonItem = UIBarButtonItem(
            title: "Scan Again",
            style: .plain,
            target: self,
            action: #selector(scanAgain))
        configureHeader()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated)
        central?.stopScan()
    }

    private func configureHeader() {
        let label = UILabel()
        label.numberOfLines = 0
        label.textColor = .label
        label.font = UIFont.preferredFont(forTextStyle: .footnote)
        label.text = """
        Current vehicle: \(vehicleText)
        Choose the adapter fitted to the vehicle. The live VIN is always read after connection and remains authoritative.
        """

        let width = max(view.bounds.width - 40, 280)
        let size = label.sizeThatFits(
            CGSize(width: width, height: .greatestFiniteMagnitude))
        let container = UIView(frame: CGRect(
            x: 0, y: 0, width: view.bounds.width, height: size.height + 28))
        label.frame = CGRect(x: 20, y: 12, width: width, height: size.height)
        container.addSubview(label)
        tableView.tableHeaderView = container
    }

    private var hasKnownAdapter: Bool {
        knownAdapterIdentifier != nil
    }

    private var nearbySection: Int {
        hasKnownAdapter ? 1 : 0
    }

    private var methodsSection: Int {
        hasKnownAdapter ? 2 : 1
    }

    override func numberOfSections(in tableView: UITableView) -> Int {
        hasKnownAdapter ? 3 : 2
    }

    override func tableView(
        _ tableView: UITableView,
        titleForHeaderInSection section: Int
    ) -> String? {
        if hasKnownAdapter && section == 0 {
            return "Saved for current vehicle"
        }
        if section == nearbySection {
            return "Nearby Bluetooth devices"
        }
        if section == methodsSection {
            return "Other connection methods"
        }
        return nil
    }

    override func tableView(
        _ tableView: UITableView,
        numberOfRowsInSection section: Int
    ) -> Int {
        if hasKnownAdapter && section == 0 { return 1 }
        if section == nearbySection { return max(nearbyAdapters.count, 1) }
        if section == methodsSection { return 2 }
        return 0
    }

    override func tableView(
        _ tableView: UITableView,
        cellForRowAt indexPath: IndexPath
    ) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        cell.textLabel?.textColor = .label
        cell.detailTextLabel?.textColor = .secondaryLabel
        cell.accessoryType = .none

        if hasKnownAdapter && indexPath.section == 0,
           let identifier = knownAdapterIdentifier {
            cell.textLabel?.text = "Saved adapter for this vehicle"
            cell.detailTextLabel?.text = identifier
            cell.imageView?.image = UIImage(systemName: "memorychip")
            cell.accessoryType = .disclosureIndicator
            return cell
        }

        if indexPath.section == nearbySection {
            let devices = nearbyAdapters
            guard !devices.isEmpty else {
                cell.textLabel?.text = central?.state == .poweredOn
                    ? "Scanning for nearby devices…"
                    : "Bluetooth unavailable or waiting…"
                cell.detailTextLabel?.text =
                    "Adapters appear here as iPhone discovers them"
                cell.selectionStyle = .none
                return cell
            }
            let adapter = devices[indexPath.row]
            cell.textLabel?.text = adapter.name
            cell.detailTextLabel?.text =
                "RSSI \(adapter.rssi) dBm · \(adapter.identifier)"
            cell.imageView?.image =
                UIImage(systemName: "dot.radiowaves.left.and.right")
            cell.accessoryType = .disclosureIndicator
            return cell
        }

        if indexPath.section == methodsSection && indexPath.row == 0 {
            cell.textLabel?.text = "Automatic adapter scan"
            cell.detailTextLabel?.text =
                "Use LINK's existing automatic adapter discovery"
            cell.imageView?.image =
                UIImage(systemName: "antenna.radiowaves.left.and.right")
            cell.accessoryType = .disclosureIndicator
        } else {
            cell.textLabel?.text = "Simulated ELM327"
            cell.detailTextLabel?.text = "Test data · no physical vehicle"
            cell.imageView?.image = UIImage(systemName: "testtube.2")
            cell.accessoryType = .disclosureIndicator
        }
        return cell
    }

    override func tableView(
        _ tableView: UITableView,
        didSelectRowAt indexPath: IndexPath
    ) {
        let source: LinkConnectionSource?
        if hasKnownAdapter && indexPath.section == 0,
           let identifier = knownAdapterIdentifier {
            source = .peripheral(identifier)
        } else if indexPath.section == nearbySection {
            let devices = nearbyAdapters
            source = devices.indices.contains(indexPath.row)
                ? .peripheral(devices[indexPath.row].identifier)
                : nil
        } else if indexPath.section == methodsSection {
            source = indexPath.row == 0 ? .automatic : .simulated
        } else {
            source = nil
        }

        guard let source else { return }
        central?.stopScan()
        navigationController?.dismiss(animated: true) { [onSelection = self.onSelection] in
            onSelection(source)
        }
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScan()
        } else {
            central.stopScan()
            tableView.reloadSections(
                IndexSet(integer: nearbySection), with: .automatic)
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let identifier = peripheral.identifier.uuidString
        let advertisedName =
            advertisementData[CBAdvertisementDataLocalNameKey] as? String
        let name = advertisedName?.trimmingCharacters(
            in: .whitespacesAndNewlines)
        let peripheralName = peripheral.name?.trimmingCharacters(
            in: .whitespacesAndNewlines)
        let displayName = !(name ?? "").isEmpty
            ? name!
            : (!(peripheralName ?? "").isEmpty
               ? peripheralName! : "Unnamed Bluetooth device")
        // The picker shows every peripheral iOS reports, including unnamed
        // devices and the saved adapter. Adapter-name hints belong to automatic
        // connection selection; they must not hide devices from manual choice.
        // Freeze row order on first discovery. RSSI may update continuously,
        // but a user must never have a row move underneath their finger.
        if adaptersByIdentifier[identifier] == nil {
            adapterDiscoveryOrder.append(identifier)
        }
        adaptersByIdentifier[identifier] = LinkNearbyAdapter(
            identifier: identifier,
            name: displayName,
            rssi: RSSI.intValue)
        tableView.reloadSections(
            IndexSet(integer: nearbySection), with: .none)
    }

    private func startScan() {
        guard let central, central.state == .poweredOn else { return }
        central.stopScan()
        central.scanForPeripherals(
            withServices: nil,
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: true])
    }

    @objc private func scanAgain() {
        adaptersByIdentifier.removeAll()
        adapterDiscoveryOrder.removeAll()
        tableView.reloadSections(
            IndexSet(integer: nearbySection), with: .automatic)
        startScan()
    }

    @objc private func cancel() {
        central?.stopScan()
        navigationController?.dismiss(animated: true)
    }
}

// MARK: - Shared standard-product application model

struct LinkStandardProductConfiguration {
    let productName: String
    let productNamespace: String
    let manufacturerName: String
    let vehicleName: String
    let versionText: String
    let legacyProfileKey: String?
    let legacySelectedVINKey: String?
    let legacyAdapterMappingKey: String?
    let dashboardSelectionNamespace: String
    let pollingSelectionNamespace: String
    let legacyPollingGlobalKey: String?
    let legacyPollingVehicleKey: String?
    let seedDefaultPollingSelection: Bool
    let defaultPollingPIDs: [UInt8]
    let defaultDashboardStableKeys: [String]
    let standardPIDStableKey: (UInt8) -> String

    init(
        productName: String,
        productNamespace: String,
        manufacturerName: String,
        vehicleName: String,
        versionText: String,
        legacyProfileKey: String? = nil,
        legacySelectedVINKey: String? = nil,
        legacyAdapterMappingKey: String? = nil,
        dashboardSelectionNamespace: String? = nil,
        pollingSelectionNamespace: String? = nil,
        legacyPollingGlobalKey: String? = nil,
        legacyPollingVehicleKey: String? = nil,
        seedDefaultPollingSelection: Bool = true,
        defaultPollingPIDs: [UInt8] = [
            0x0C, 0x0D, 0x05, 0x11, 0x04, 0x0F, 0x10, 0x42
        ],
        defaultDashboardStableKeys: [String] = [],
        standardPIDStableKey: @escaping (UInt8) -> String = {
            String(format: "obd2-01-%02X", $0)
        }
    ) {
        self.productName = productName
        self.productNamespace = productNamespace
        self.manufacturerName = manufacturerName
        self.vehicleName = vehicleName
        self.versionText = versionText
        self.legacyProfileKey = legacyProfileKey
        self.legacySelectedVINKey = legacySelectedVINKey
        self.legacyAdapterMappingKey = legacyAdapterMappingKey
        self.dashboardSelectionNamespace =
            dashboardSelectionNamespace ?? productNamespace
        self.pollingSelectionNamespace =
            pollingSelectionNamespace ?? productNamespace + "-polling"
        self.legacyPollingGlobalKey = legacyPollingGlobalKey
        self.legacyPollingVehicleKey = legacyPollingVehicleKey
        self.seedDefaultPollingSelection = seedDefaultPollingSelection
        self.defaultPollingPIDs = defaultPollingPIDs
        self.defaultDashboardStableKeys = defaultDashboardStableKeys
        self.standardPIDStableKey = standardPIDStableKey
    }
}

/**
 * Complete product-neutral Swift application model for a standard LINK face.
 *
 * A branded repository supplies one configured controller, identity metadata,
 * theme and manufacturer screens. LINK owns connection selection, generic
 * vehicle persistence, SAE capability caching, polling policy, dashboard
 * selection, evidence export and presentation snapshots.
 */
@MainActor
class LinkStandardProductViewModel: NSObject, ObservableObject {
    @Published private(set) var statusText = "Idle"
    @Published private(set) var peripheralName = "No adapter"
    @Published private(set) var adapterIdentifier = "Unknown"
    @Published private(set) var obdProtocolText = "OBD-II protocol not identified"
    @Published private(set) var vehicleVINText = "No vehicle loaded"
    @Published private(set) var faultScanStatusText = "Not scanned"
    @Published private(set) var storedDTCs = [String]()
    @Published private(set) var pendingDTCs = [String]()
    @Published private(set) var permanentDTCs = [String]()
    @Published private(set) var readinessStatusText = "Not read"
    @Published private(set) var readinessMonitorStatus = [String]()
    @Published private(set) var freezeFrameContext = [String]()
    @Published private(set) var diagnosticCapabilityText = "Unknown / probing"
    @Published private(set) var diagnosticCapabilityDetailText = ""
    @Published private(set) var standardResponderSummary = "0 physical responders"
    @Published private(set) var supportedPIDSummary = "0 advertised PIDs"
    @Published private(set) var standardLiveRows = [String]()
    @Published private(set) var diagnosticParameters = [LinkDiagnosticParameter]()
    @Published private(set) var dashboardParameters = [LinkDiagnosticParameter]()
    @Published private(set) var savedVehicleProfiles = [LinkSavedVehicleProfileSummary]()
    @Published private(set) var selectedVehicleVIN: String?
    @Published private(set) var isActive = false
    @Published private(set) var isReady = false
    @Published private(set) var isSimulationActive = false
    @Published private(set) var recordedSampleCount = 0
    @Published private(set) var versionText: String
    @Published private(set) var linkVersionText = "Unknown"
    @Published private(set) var csvExportURL: URL?
    @Published private(set) var isPreparingCSV = false
    @Published private(set) var languageTags = [String]()
    @Published private(set) var languageNames = [String]()
    @Published private(set) var selectedLanguageID = "en-AU"
    @Published private(set) var measurementKeys = [String]()
    @Published private(set) var measurementNames = [String]()
    @Published private(set) var selectedMeasurementID = "metric"

    let productController: LinkProductDiagnosticsController
    let configuration: LinkStandardProductConfiguration
    let vehicleProfileStore: LinkVehicleProfileStore
    let dashboardSelectionStore: LinkPIDSelectionStore
    let pollingSelectionStore: LinkPIDSelectionStore
    private var lastPersistedLiveVIN: String?
    private var lastPersistedReadyVIN: String?
    private var lastCapabilityMergeVIN: String?
    private var productHooksEnabled = false

    var interfaceLocaleIdentifier: String { selectedLanguageID }

    var selectedVehicleDisplayName: String {
        guard let selectedVehicleVIN else { return "No vehicle loaded" }
        return savedVehicleProfiles.first(where: {
            $0.vin == selectedVehicleVIN
        })?.displayName ?? configuration.vehicleName
    }

    init(
        controller: LinkProductDiagnosticsController,
        configuration: LinkStandardProductConfiguration
    ) {
        self.productController = controller
        self.configuration = configuration
        self.versionText = configuration.versionText
        self.vehicleProfileStore = LinkVehicleProfileStore(
            productNamespace: configuration.productNamespace,
            legacyProfileKey: configuration.legacyProfileKey,
            legacySelectedVINKey: configuration.legacySelectedVINKey,
            legacyAdapterMappingKey: configuration.legacyAdapterMappingKey)
        self.dashboardSelectionStore = LinkPIDSelectionStore(
            productNamespace: configuration.dashboardSelectionNamespace,
            legacyGlobalKey: nil,
            legacyVehicleKey: nil)
        self.pollingSelectionStore = LinkPIDSelectionStore(
            productNamespace: configuration.pollingSelectionNamespace,
            legacyGlobalKey: configuration.legacyPollingGlobalKey,
            legacyVehicleKey: configuration.legacyPollingVehicleKey)
        super.init()

        selectedVehicleVIN = vehicleProfileStore.selectedVehicleVIN
        seedDefaultPollingSelection()
        applyStoredPollingPolicy()
        refreshStandardState()
        productHooksEnabled = true
    }

    func connect() {
        clearPreparedExport()
        guard !isActive else { return }
        let currentVehicleText = selectedVehicleVIN.map {
            "\(selectedVehicleDisplayName) · \($0)"
        } ?? "No saved vehicle loaded"
        let knownAdapter = selectedVehicleVIN.flatMap {
            vehicleProfileStore.associatedAdapterIdentifier(forVIN: $0)
        }
        LinkConnectionPresentation.presentPicker(
            vehicleText: currentVehicleText,
            knownAdapterIdentifier: knownAdapter,
            unavailable: { [weak self] in
                self?.statusText =
                    "Unable to open adapter picker · try Connect again"
            }) { [weak self] source in
                self?.beginConnection(source)
            }
    }

    private func beginConnection(_ source: LinkConnectionSource) {
        guard !isActive else { return }
        lastPersistedLiveVIN = nil
        lastPersistedReadyVIN = nil
        lastCapabilityMergeVIN = nil
        switch source {
        case .automatic:
            isSimulationActive = false
            productController.start()
        case .simulated:
            isSimulationActive = true
            productController.startSimulated()
        case .peripheral(let identifier):
            isSimulationActive = false
            productController.start(withPeripheralIdentifier: identifier)
        }
    }

    func disconnect() {
        productController.disconnect()
        isSimulationActive = false
    }

    func startSimulatedDiagnostics() {
        guard !isActive else { return }
        lastPersistedLiveVIN = nil
        lastPersistedReadyVIN = nil
        lastCapabilityMergeVIN = nil
        isSimulationActive = true
        productController.startSimulated()
    }

    func selectSavedVehicle(vin: String) {
        guard !isActive,
              vehicleProfileStore.selectOfflineVehicle(withVIN: vin) else {
            return
        }
        selectedVehicleVIN = vin
        refreshStandardState()
    }

    func localizedText(_ key: String) -> String {
        productController.localizedText(forKey: key)
    }

    func selectLanguage(_ id: String) {
        productController.setSelectedLanguageTag(id)
        refreshStandardState()
    }

    func selectMeasurementSystem(_ id: String) {
        productController.setSelectedMeasurementSystemKey(id)
        refreshStandardState()
    }

    func toggleFavourite(_ parameter: LinkDiagnosticParameter) {
        guard let pid = UInt8(exactly: parameter.parameterIdentifier) else { return }
        productController.setFavourite(
            !productController.favourite(forPID: pid), forPID: pid)
        refreshStandardState()
    }

    func toggleFavourite(stableKey: String) {
        guard let parameter = diagnosticParameters.first(where: {
            $0.id == stableKey
        }) else { return }
        toggleFavourite(parameter)
    }

    func togglePolling(_ parameter: LinkDiagnosticParameter) {
        guard let pid = UInt8(exactly: parameter.parameterIdentifier) else { return }
        let enabled = !productController.pollingEnabled(forPID: pid)
        var enabledKeys = Set(pollingSelectionStore.globalStableKeys)
        if enabled { enabledKeys.insert(parameter.id) }
        else { enabledKeys.remove(parameter.id) }
        pollingSelectionStore.setGlobalStableKeys(Array(enabledKeys).sorted())
        productController.setPollingEnabled(enabled, forPID: pid)
        refreshStandardState()
    }

    func dashboardSelected(_ parameter: LinkDiagnosticParameter) -> Bool {
        dashboardSelectionStore.globalStableKeys.contains(parameter.id)
    }

    func toggleDashboard(_ parameter: LinkDiagnosticParameter) {
        var selected = Set(dashboardSelectionStore.globalStableKeys)
        if selected.contains(parameter.id) { selected.remove(parameter.id) }
        else { selected.insert(parameter.id) }
        dashboardSelectionStore.setGlobalStableKeys(Array(selected).sorted())
        refreshDashboardSelection()
        dashboardParameters = productDashboardParameters(
            standard: dashboardParameters)
    }

    func prepareCSVExport() {
        guard !isPreparingCSV,
              let snapshot = productController.csvDataSnapshot() else { return }
        clearPreparedExport()
        isPreparingCSV = true
        let data = snapshot as Data
        Task { [weak self] in
            guard let self else { return }
            do {
                let url = try await LinkEvidenceExport.prepareTemporaryCSV(
                    data, productName: configuration.productName)
                self.csvExportURL = url
            } catch {
                self.csvExportURL = nil
            }
            self.isPreparingCSV = false
        }
    }

    func refreshStandardState() {
        refreshSavedVehicleProfiles()
        statusText = productController.statusText
        peripheralName = productController.peripheralName ?? "No adapter"
        adapterIdentifier = productController.adapterIdentifier ?? "Unknown"
        obdProtocolText = productController.obdProtocolText
        let active = productController.isActive
        let liveVIN = productController.standardVINText
        let validLiveVIN = liveVIN.count == 17 ? liveVIN : nil
        vehicleVINText = active
            ? (validLiveVIN ?? "Waiting for standard VIN")
            : (selectedVehicleVIN ?? "No vehicle loaded")

        if active, isSimulationActive {
            // Presentation may use the demo VIN; never select it in the real store.
            selectedVehicleVIN = validLiveVIN
        }
        if active, !isSimulationActive, let validLiveVIN,
           lastPersistedLiveVIN != validLiveVIN {
            vehicleProfileStore.recordLiveVIN(validLiveVIN)
            selectedVehicleVIN = validLiveVIN
            saveVehicleProfile(vin: validLiveVIN)
            lastPersistedLiveVIN = validLiveVIN
            refreshSavedVehicleProfiles()
        }
        if active, !isSimulationActive, let validLiveVIN {
            mergeStandardCapabilitiesIfReady(vin: validLiveVIN)
        }

        if active {
            restoreLiveDiagnosticSnapshot()
        } else if let vin = selectedVehicleVIN,
                  let profile = vehicleProfileStore.profile(forVIN: vin) {
            restoreSavedDiagnosticSnapshot(profile)
            if productHooksEnabled {
                productDidRestoreVehicleProfile(profile, vin: vin)
            }
        } else {
            resetOfflineDiagnosticSnapshot()
        }
        languageTags = productController.availableLanguageTags
        languageNames = productController.availableLanguageNames
        selectedLanguageID = productController.selectedLanguageTag
        measurementKeys = productController.availableMeasurementSystemKeys
        measurementNames = productController.availableMeasurementSystemNames
        selectedMeasurementID = productController.selectedMeasurementSystemKey
        linkVersionText = productController.linkVersionText
        isActive = active
        isReady = productController.isReady
        if productHooksEnabled { productDidRefreshStandardState() }
        if active, !isSimulationActive, isReady, let validLiveVIN,
           lastPersistedReadyVIN != validLiveVIN {
            saveVehicleProfile(vin: validLiveVIN, includeDiagnosticSnapshot: true)
            lastPersistedReadyVIN = validLiveVIN
            refreshSavedVehicleProfiles()
        }
        diagnosticParameters = productDiagnosticParameters(
            standard: loadDiagnosticParameters())
        refreshDashboardSelection()
        dashboardParameters = productDashboardParameters(
            standard: dashboardParameters)
        recordedSampleCount = Int(clamping: productController.recordedSampleCount)
    }

    private func refreshSavedVehicleProfiles() {
        savedVehicleProfiles = vehicleProfileStore.savedProfiles.compactMap { profile in
            LinkSavedVehicleProfileSummary(
                profile: profile as NSDictionary,
                moduleCount: productModuleCountForVehicleProfile(profile),
                fallbackDisplayName: configuration.vehicleName)
        }
        selectedVehicleVIN = vehicleProfileStore.selectedVehicleVIN
    }

    private func saveVehicleProfile(
        vin: String,
        includeDiagnosticSnapshot: Bool = false
    ) {
        guard !isSimulationActive else { return }
        var profile = vehicleProfileStore.profile(forVIN: vin) ?? [:]
        if profile["displayName"] == nil {
            profile["displayName"] = "\(configuration.vehicleName) · \(vin)"
        }
        profile["manufacturer"] = configuration.manufacturerName
        profile["obdProtocolText"] = productController.obdProtocolText
        profile["diagnosticCapabilityText"] =
            productController.diagnosticCapabilityText
        if includeDiagnosticSnapshot {
            profile["standardResponderSummary"] =
                productController.standardResponderSummary
            profile["supportedPIDSummary"] = productController.supportedPIDSummary
            profile["standardVINText"] = productController.standardVINText
            profile["standardLiveValueRows"] = productController.standardLiveValueRows
            profile["diagnosticCapabilityDetailText"] =
                productController.diagnosticCapabilityDetailText
            profile["faultScanStatusText"] = productController.faultScanStatusText
            profile["storedDTCs"] = productController.storedDTCs
            profile["pendingDTCs"] = productController.pendingDTCs
            profile["permanentDTCs"] = productController.permanentDTCs
            profile["readinessStatusText"] = productController.readinessStatusText
            profile["readinessMonitorStatus"] =
                productController.readinessMonitorStatus
            profile["freezeFrameContext"] = productController.freezeFrameContext
        }
        productWillSaveVehicleProfile(&profile, vin: vin)
        vehicleProfileStore.saveProfile(profile, forVIN: vin)
    }

    private func restoreLiveDiagnosticSnapshot() {
        faultScanStatusText = productController.faultScanStatusText
        storedDTCs = productController.storedDTCs
        pendingDTCs = productController.pendingDTCs
        permanentDTCs = productController.permanentDTCs
        readinessStatusText = productController.readinessStatusText
        readinessMonitorStatus = productController.readinessMonitorStatus
        freezeFrameContext = productController.freezeFrameContext
        diagnosticCapabilityText = productController.diagnosticCapabilityText
        diagnosticCapabilityDetailText = productController.diagnosticCapabilityDetailText
        standardResponderSummary = productController.standardResponderSummary
        supportedPIDSummary = productController.supportedPIDSummary
        standardLiveRows = productController.standardLiveValueRows
    }

    private func restoreSavedDiagnosticSnapshot(_ profile: [AnyHashable: Any]) {
        obdProtocolText = (profile["obdProtocolText"] as? String)
            ?? "Saved OBD-II protocol unavailable"
        faultScanStatusText = (profile["faultScanStatusText"] as? String)
            ?? "Saved diagnostic state"
        storedDTCs = (profile["storedDTCs"] as? [String]) ?? []
        pendingDTCs = (profile["pendingDTCs"] as? [String]) ?? []
        permanentDTCs = (profile["permanentDTCs"] as? [String]) ?? []
        readinessStatusText = (profile["readinessStatusText"] as? String)
            ?? "Saved readiness state"
        readinessMonitorStatus =
            (profile["readinessMonitorStatus"] as? [String]) ?? []
        freezeFrameContext = (profile["freezeFrameContext"] as? [String]) ?? []
        diagnosticCapabilityText =
            (profile["diagnosticCapabilityText"] as? String)
            ?? "Saved diagnostic capability"
        diagnosticCapabilityDetailText =
            (profile["diagnosticCapabilityDetailText"] as? String) ?? ""
        standardResponderSummary =
            (profile["standardResponderSummary"] as? String)
            ?? "Saved standard responder information"
        supportedPIDSummary = (profile["supportedPIDSummary"] as? String)
            ?? "Saved standard PID information"
        standardLiveRows =
            (profile["standardLiveValueRows"] as? [String])
            ?? (profile["standardLiveRows"] as? [String]) ?? []
    }

    private func resetOfflineDiagnosticSnapshot() {
        obdProtocolText = "OBD-II protocol not identified"
        faultScanStatusText = "Not scanned"
        storedDTCs = []
        pendingDTCs = []
        permanentDTCs = []
        readinessStatusText = "Not read"
        readinessMonitorStatus = []
        freezeFrameContext = []
        diagnosticCapabilityText = "Unknown / probing"
        diagnosticCapabilityDetailText = ""
        standardResponderSummary = "0 physical responders"
        supportedPIDSummary = "0 advertised PIDs"
        standardLiveRows = []
    }

    /** Manufacturer-only profile fields; generic persistence remains in LINK. */
    func productWillSaveVehicleProfile(
        _ profile: inout [AnyHashable: Any],
        vin: String
    ) {}

    /** Restore manufacturer-only fields after LINK restores its snapshot. */
    func productDidRestoreVehicleProfile(
        _ profile: [AnyHashable: Any],
        vin: String
    ) {}

    /** Refresh manufacturer-only presentation after the shared state changes. */
    func productDidRefreshStandardState() {}

    /** Product-specific live values may extend or replace the standard table. */
    func productDiagnosticParameters(
        standard: [LinkDiagnosticParameter]
    ) -> [LinkDiagnosticParameter] { standard }

    /** Product-specific dashboard values may extend the standard selection. */
    func productDashboardParameters(
        standard: [LinkDiagnosticParameter]
    ) -> [LinkDiagnosticParameter] { standard }

    /** Product profiles may report a manufacturer controller inventory. */
    func productModuleCountForVehicleProfile(
        _ profile: [AnyHashable: Any]
    ) -> Int { 0 }

    private func mergeStandardCapabilitiesIfReady(vin: String) {
        guard productController.isReady,
              lastCapabilityMergeVIN != vin,
              let flow = productController.diagnosticFlow() else { return }
        _ = vehicleProfileStore.mergeStandardCapabilities(
            fromDiagnosticFlow: flow, forVIN: vin)
        lastCapabilityMergeVIN = vin
    }

    private func loadDiagnosticParameters() -> [LinkDiagnosticParameter] {
        let count = Int(link_obd2_pid_definition_count())
        var result = [LinkDiagnosticParameter]()
        result.reserveCapacity(count)
        for index in 0..<count {
            guard let definition = link_obd2_pid_definition_at(index) else { continue }
            let metadata = definition.pointee
            guard metadata.mode == 0x01, let name = metadata.name else { continue }
            let pid = metadata.pid
            let supported = productController.supportsPID(pid)
            let pollingEnabled = productController.pollingEnabled(forPID: pid)
            let history = productController.displayRecentValues(
                forPID: pid, limit: 60).map(\.doubleValue)
            let value = history.last
            let unit = productController.displayUnit(forPID: pid)
            let structuredValue = productController.structuredDisplayValue(forPID: pid)
            let rawHex = productController.structuredRawHex(forPID: pid)
            let range = productController.displayRange(forPID: pid)
            let minimum = range.count >= 2 ? range[0].doubleValue : nil
            let maximum = range.count >= 2 ? range[1].doubleValue : nil
            let suffix = unit.isEmpty ? "" : " \(unit)"
            let precision = link_parameter_obd2_definition(pid)
                .map { Int32(min(Int($0.pointee.decimal_places), 9)) } ?? 1
            result.append(LinkDiagnosticParameter(
                id: configuration.standardPIDStableKey(pid),
                protocolName: "OBD2",
                moduleIdentifier: 0,
                parameterIdentifier: UInt32(pid),
                shortName: String(format: "PID %02X", pid),
                title: String(cString: name),
                suffix: unit,
                formattedValue: value.map {
                    String(format: "%.*f%@", precision, $0, suffix)
                } ?? "N/A",
                value: value,
                structuredValue: structuredValue,
                rawHex: rawHex,
                vehicleSupported: supported,
                favourite: productController.favourite(forPID: pid),
                pollingEnabled: pollingEnabled,
                history: history,
                sourceLabel: "SAE OBD-II",
                qualityNote: supported && !pollingEnabled
                    ? "Polling disabled" : nil,
                dashboardMinimum: minimum,
                dashboardMaximum: maximum))
        }
        return result
    }

    private func refreshDashboardSelection() {
        let supported = diagnosticParameters.filter(\.vehicleSupported)
        if !dashboardSelectionStore.hasGlobalSelection {
            let configured = Set(configuration.defaultDashboardStableKeys)
            let configuredDefaults = supported.compactMap { parameter in
                configured.contains(parameter.id) ? parameter.id : nil
            }
            let preferredPIDs: [UInt32] = [
                0x0C, 0x0D, 0x05, 0x11, 0x04, 0x0F
            ]
            let preferred = configuredDefaults.isEmpty
                ? preferredPIDs.compactMap { pid in
                    supported.first(where: {
                        $0.parameterIdentifier == pid
                    })?.id
                }
                : configuredDefaults
            let defaults = preferred.isEmpty
                ? Array(supported.prefix(6).map(\.id)) : preferred
            if !defaults.isEmpty {
                dashboardSelectionStore.setGlobalStableKeys(defaults)
            }
        }
        let selected = Set(dashboardSelectionStore.globalStableKeys)
        let chosen = diagnosticParameters.filter {
            selected.contains($0.id) && $0.vehicleSupported
        }
        dashboardParameters = dashboardSelectionStore.hasGlobalSelection
            ? chosen : Array(supported.prefix(6))
    }

    private func defaultStandardPollingKeys() -> [String] {
        configuration.defaultPollingPIDs.compactMap { pid in
            guard let definition = link_obd2_pid_definition(0x01, pid),
                  (definition.pointee.pid & 0x1F) != 0 else { return nil }
            return configuration.standardPIDStableKey(pid)
        }
    }

    private func seedDefaultPollingSelection() {
        guard configuration.seedDefaultPollingSelection,
              !pollingSelectionStore.hasGlobalSelection else { return }
        pollingSelectionStore.setGlobalStableKeys(defaultStandardPollingKeys())
    }

    private func applyStoredPollingPolicy() {
        let enabledKeys = Set(pollingSelectionStore.globalStableKeys)
        let count = Int(link_obd2_pid_definition_count())
        for index in 0..<count {
            guard let definition = link_obd2_pid_definition_at(index) else { continue }
            let metadata = definition.pointee
            guard metadata.mode == 0x01, (metadata.pid & 0x1F) != 0 else { continue }
            let key = configuration.standardPIDStableKey(metadata.pid)
            productController.setPollingEnabled(
                enabledKeys.contains(key), forPID: metadata.pid)
        }
    }

    private func clearPreparedExport() {
        LinkEvidenceExport.removeTemporaryFile(csvExportURL)
        csvExportURL = nil
    }
}

@MainActor
enum LinkConnectionPresentation {
    static func presentPicker(
        vehicleText: String,
        knownAdapterIdentifier: String?,
        unavailable: @escaping () -> Void,
        selection: @escaping (LinkConnectionSource) -> Void
    ) {
        presentPicker(
            vehicleText: vehicleText,
            knownAdapterIdentifier: knownAdapterIdentifier,
            remainingPresentationAttempts: 3,
            unavailable: unavailable,
            selection: selection)
    }

    private static func presentPicker(
        vehicleText: String,
        knownAdapterIdentifier: String?,
        remainingPresentationAttempts: Int,
        unavailable: @escaping () -> Void,
        selection: @escaping (LinkConnectionSource) -> Void
    ) {
        guard let presenter = presentingViewController() else {
            guard remainingPresentationAttempts > 0 else {
                unavailable()
                return
            }
            Task { @MainActor in
                try? await Task<Never, Never>.sleep(
                    nanoseconds: 100_000_000)
                presentPicker(
                    vehicleText: vehicleText,
                    knownAdapterIdentifier: knownAdapterIdentifier,
                    remainingPresentationAttempts:
                        remainingPresentationAttempts - 1,
                    unavailable: unavailable,
                    selection: selection)
            }
            return
        }
        let picker = LinkConnectionPickerViewController(
            vehicleText: vehicleText,
            knownAdapterIdentifier: knownAdapterIdentifier) { source in
                Task { @MainActor in selection(source) }
            }
        let navigation = UINavigationController(rootViewController: picker)
        navigation.modalPresentationStyle = .pageSheet
        presenter.present(navigation, animated: true)
    }

    private static func presentingViewController() -> UIViewController? {
        guard let scene = UIApplication.shared.connectedScenes
            .compactMap({ $0 as? UIWindowScene })
            .first(where: { $0.activationState == .foregroundActive }),
              let root = scene.windows.first(where: \.isKeyWindow)?.rootViewController else {
            return nil
        }
        return topViewController(root)
    }

    private static func topViewController(
        _ controller: UIViewController
    ) -> UIViewController {
        if let presented = controller.presentedViewController {
            return topViewController(presented)
        }
        if let navigation = controller as? UINavigationController,
           let visible = navigation.visibleViewController {
            return topViewController(visible)
        }
        if let tabs = controller as? UITabBarController,
           let selected = tabs.selectedViewController {
            return topViewController(selected)
        }
        return controller
    }
}

// MARK: - Shared standard-product SwiftUI face

/**
 * Branding and product-owned wording for LINK's standard diagnostic face.
 *
 * Geometry, navigation and standard OBD presentation remain in LINK. Product
 * repositories supply only identity, artwork, colour and truthful boundary
 * wording.
 */
struct LinkStandardProductAppearance {
    let productName: String
    let manufacturerName: String
    let subtitle: String
    let emblemAssetName: String
    let theme: LinkDiagnosticTheme
    let summary: String
    let authors: [String]
    let copyrightShort: String
    let copyrightFull: String
    let website: URL?
    let licenseName: String
    let licenseText: String
    let credits: [String]

    init(
        productName: String,
        manufacturerName: String,
        subtitle: String,
        emblemAssetName: String,
        theme: LinkDiagnosticTheme,
        summary: String,
        authors: [String],
        copyrightShort: String,
        copyrightFull: String,
        website: URL?,
        licenseName: String,
        licenseText: String,
        credits: [String]
    ) {
        self.productName = productName
        self.manufacturerName = manufacturerName
        self.subtitle = subtitle
        self.emblemAssetName = emblemAssetName
        self.theme = theme
        self.summary = summary
        self.authors = authors
        self.copyrightShort = copyrightShort
        self.copyrightFull = copyrightFull
        self.website = website
        self.licenseName = licenseName
        self.licenseText = licenseText
        self.credits = credits
    }
}

/** Complete LINK-owned standard diagnostic application surface. */
struct LinkStandardProductContentView: View {
    @ObservedObject var model: LinkStandardProductViewModel
    let appearance: LinkStandardProductAppearance
    @State private var showingAbout = false

    var body: some View {
        LinkCommandCentreShell(
            showProgress: model.isActive && !model.isReady,
            header: { header },
            progress: { connectionProgress },
            connection: { connectionCard },
            primary: { primaryGrid },
            tools: { EmptyView() })
            .linkDiagnosticTheme(appearance.theme)
            .linkDiagnosticLocalization { model.localizedText($0) }
            .environment(
                \.locale,
                Locale(identifier: model.interfaceLocaleIdentifier))
            .environment(
                \.layoutDirection,
                model.interfaceLocaleIdentifier.hasPrefix("ar")
                    ? .rightToLeft : .leftToRight)
            .safeAreaInset(edge: .bottom, spacing: 0) {
                LinkDiagnosticAboutButton(
                    productName: appearance.productName,
                    copyright: appearance.copyrightShort) {
                        showingAbout = true
                    }
                    .linkDiagnosticTheme(appearance.theme)
            }
            .sheet(isPresented: $showingAbout) {
                LinkDiagnosticAboutView(
                    info: aboutInfo,
                    onClose: { showingAbout = false }) {
                        emblem(size: 82)
                    }
                    .linkDiagnosticTheme(appearance.theme)
                    .preferredColorScheme(.dark)
                    .tint(appearance.theme.accent)
            }
    }

    private var aboutInfo: LinkDiagnosticAboutInfo {
        LinkDiagnosticAboutInfo(
            productName: appearance.productName,
            subtitle: appearance.subtitle,
            version: model.versionText,
            summary: appearance.summary,
            authors: appearance.authors,
            copyright: appearance.copyrightFull,
            website: appearance.website,
            licenseName: appearance.licenseName,
            licenseText: appearance.licenseText,
            credits: appearance.credits)
    }

    private func emblem(size: CGFloat) -> some View {
        Image(appearance.emblemAssetName)
            .resizable()
            .scaledToFit()
            .frame(width: size, height: size)
            .shadow(color: .black.opacity(0.28), radius: 7, x: 0, y: 4)
            .accessibilityHidden(true)
    }

    private var header: some View {
        LinkBrandHeader {
            HStack(spacing: 14) {
                emblem(size: 56)
                VStack(alignment: .leading, spacing: 3) {
                    Text(appearance.productName)
                        .font(.system(size: 29, weight: .bold))
                        .tracking(1.2)
                        .foregroundStyle(appearance.theme.primaryText)
                    Text(appearance.subtitle)
                        .font(.caption2.bold())
                        .tracking(1.2)
                        .foregroundStyle(appearance.theme.accent)
                    Text(
                        "LINK shared engine · \(appearance.manufacturerName)-specific knowledge layered above")
                        .font(.caption)
                        .foregroundStyle(appearance.theme.secondaryText)
                        .lineLimit(1)
                }
            }
        } status: {
            LinkStatusPill(text: model.statusText, active: model.isReady)
        }
    }

    private var connectionCard: some View {
        LinkPanel {
            VStack(alignment: .leading, spacing: 12) {
                HStack(alignment: .firstTextBaseline) {
                    VStack(alignment: .leading, spacing: 3) {
                        Text(model.isActive
                             ? "Diagnostic session" : "Vehicle connection")
                            .font(.headline)
                            .foregroundStyle(appearance.theme.primaryText)
                        Text(model.isActive
                             ? model.statusText : model.selectedVehicleDisplayName)
                            .font(.caption)
                            .foregroundStyle(appearance.theme.secondaryText)
                    }
                    Spacer(minLength: 12)
                    Image(systemName: model.isReady
                          ? "checkmark.circle.fill"
                          : model.isActive
                            ? "dot.radiowaves.left.and.right"
                            : "cable.connector")
                        .foregroundStyle(model.isReady
                            ? appearance.theme.success : appearance.theme.accent)
                }
                Button {
                    model.isActive ? model.disconnect() : model.connect()
                } label: {
                    Label(
                        model.isActive ? "Disconnect" : "Connect to vehicle",
                        systemImage: model.isActive
                            ? "cable.connector.slash" : "cable.connector")
                        .font(.subheadline.weight(.semibold))
                        .foregroundStyle(appearance.theme.primaryText)
                        .frame(maxWidth: .infinity)
                        .padding(.vertical, 11)
                        .background(
                            RoundedRectangle(cornerRadius: 12, style: .continuous)
                                .fill(appearance.theme.accent))
                }
                .buttonStyle(.plain)
                if model.isReady {
                    Text("\(model.vehicleVINText) · \(model.diagnosticCapabilityText)")
                        .font(.caption2.monospaced())
                        .foregroundStyle(appearance.theme.secondaryText)
                }
            }
        }
    }

    private var connectionProgress: some View {
        LinkPanel {
            VStack(alignment: .leading, spacing: 7) {
                Label(
                    "Connecting to vehicle",
                    systemImage: "dot.radiowaves.left.and.right")
                    .font(.headline)
                    .foregroundStyle(appearance.theme.primaryText)
                Text(model.statusText)
                    .font(.subheadline)
                    .foregroundStyle(appearance.theme.secondaryText)
                if model.peripheralName != "No adapter" {
                    Text(model.peripheralName)
                        .font(.caption)
                        .foregroundStyle(appearance.theme.secondaryText)
                }
            }
        }
    }

    private var primaryGrid: some View {
        LinkDiagnosticGrid {
            LinkTaskTile(.vehicle) {
                LinkStandardVehicleView(model: model, appearance: appearance)
            }
            LinkTaskTile(.log) { LinkStandardEvidenceView(model: model) }
            LinkTaskTile(.errors) { LinkStandardFaultsView(model: model) }
            LinkTaskTile(.dashboard) { LinkStandardDashboardView(model: model) }
            LinkTaskTile(.table) { LinkStandardTableView(model: model) }
            LinkTaskTile(.graph) { LinkStandardGraphView(model: model) }
            LinkTaskTile(.tests) { LinkStandardTestsView(model: model) }
            LinkTaskTile(.services) {
                LinkStandardServicesView(model: model, appearance: appearance)
            }
            LinkTaskTile(.settings) {
                LinkStandardSettingsView(model: model, appearance: appearance)
            }
        }
    }
}

private struct LinkStandardVehicleView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel
    let appearance: LinkStandardProductAppearance

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 15) {
                LinkLabeledPanel(title: "Vehicle", systemImage: "car.side.fill") {
                    LinkStandardValueRow(
                        label: "VIN", value: model.vehicleVINText, icon: "number")
                    Divider()
                    LinkStandardValueRow(
                        label: "Diagnostic generation",
                        value: model.diagnosticCapabilityText,
                        icon: "cpu")
                    Divider()
                    LinkStandardValueRow(
                        label: "OBD protocol",
                        value: model.obdProtocolText,
                        icon: "cable.connector")
                }
                LinkLabeledPanel(title: "Saved vehicles", systemImage: "car.2.fill") {
                    if model.savedVehicleProfiles.isEmpty {
                        Text(
                            "No saved \(appearance.manufacturerName) vehicles yet. A successful live VIN creates the profile automatically.")
                            .font(.subheadline)
                            .foregroundStyle(theme.secondaryText)
                    } else {
                        ForEach(model.savedVehicleProfiles) { profile in
                            Button {
                                model.selectSavedVehicle(vin: profile.vin)
                            } label: {
                                HStack {
                                    VStack(alignment: .leading, spacing: 2) {
                                        Text(profile.displayName)
                                            .font(.subheadline.bold())
                                            .foregroundStyle(theme.primaryText)
                                        Text(profile.vin)
                                            .font(.caption2.monospaced())
                                            .foregroundStyle(theme.secondaryText)
                                    }
                                    Spacer()
                                    if profile.vin == model.selectedVehicleVIN {
                                        Image(systemName: "checkmark.circle.fill")
                                            .foregroundStyle(theme.accent)
                                    }
                                }
                            }
                            .buttonStyle(.plain)
                            if profile.id != model.savedVehicleProfiles.last?.id {
                                Divider()
                            }
                        }
                    }
                }
                LinkLabeledPanel(
                    title: "Control units",
                    systemImage: "square.stack.3d.up.fill") {
                        NavigationLink {
                            LinkStandardModulesView(model: model)
                        } label: {
                            HStack {
                                VStack(alignment: .leading, spacing: 3) {
                                    Text("Responder and module inventory")
                                        .font(.headline)
                                        .foregroundStyle(theme.primaryText)
                                    Text(
                                        "LINK standard responders now; \(appearance.manufacturerName)-specific module knowledge remains product-owned.")
                                        .font(.caption)
                                        .foregroundStyle(theme.secondaryText)
                                }
                                Spacer()
                                Image(systemName: "chevron.right")
                                    .foregroundStyle(theme.accent)
                            }
                        }
                        .buttonStyle(.plain)
                    }
            }
            .padding(16)
        }
        .linkDiagnosticScreen("Vehicle")
    }
}

private struct LinkStandardModulesView: View {
    @ObservedObject var model: LinkStandardProductViewModel

    var body: some View {
        ScrollView {
            LinkLabeledPanel(
                title: "Standard responders",
                systemImage: "square.stack.3d.up.fill") {
                    LinkStandardValueRow(
                        label: "Physical responders",
                        value: model.standardResponderSummary,
                        icon: "point.3.connected.trianglepath.dotted")
                    Divider()
                    LinkStandardValueRow(
                        label: "Advertised parameters",
                        value: model.supportedPIDSummary,
                        icon: "waveform.path.ecg")
                    Divider()
                    LinkStandardValueRow(
                        label: "Capability",
                        value: model.diagnosticCapabilityText,
                        icon: "cpu")
                }
                .padding(16)
        }
        .linkDiagnosticScreen("Modules")
    }
}

private struct LinkStandardFaultsView: View {
    @ObservedObject var model: LinkStandardProductViewModel
    private var total: Int {
        model.storedDTCs.count + model.pendingDTCs.count
            + model.permanentDTCs.count
    }

    var body: some View {
        ScrollView {
            LinkLabeledPanel(
                title: "Errors",
                systemImage: "exclamationmark.triangle.fill") {
                    LinkStandardValueRow(
                        label: "Scan state",
                        value: model.faultScanStatusText,
                        icon: "waveform.path.ecg")
                    LinkStandardValueRow(
                        label: "Fault records",
                        value: "\(total)",
                        icon: "exclamationmark.triangle")
                    LinkStandardFaultGroup(title: "Stored", values: model.storedDTCs)
                    LinkStandardFaultGroup(title: "Pending", values: model.pendingDTCs)
                    LinkStandardFaultGroup(
                        title: "Permanent", values: model.permanentDTCs)
                }
                .padding(16)
        }
        .linkDiagnosticScreen("Errors")
    }
}

private struct LinkStandardTableView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel
    private var parameters: [LinkDiagnosticParameter] {
        model.diagnosticParameters.filter(\.vehicleSupported)
    }

    var body: some View {
        ScrollView {
            LinkLabeledPanel(title: "Table", systemImage: "tablecells") {
                if parameters.isEmpty {
                    Text(model.isActive
                         ? "Waiting for advertised standard parameters."
                         : "Connect to populate supported standard live data.")
                        .font(.subheadline)
                        .foregroundStyle(theme.secondaryText)
                } else {
                    ForEach(parameters) { parameter in
                        HStack(alignment: .top, spacing: 10) {
                            VStack(alignment: .leading, spacing: 3) {
                                Text(parameter.title)
                                    .font(.subheadline.bold())
                                    .foregroundStyle(theme.primaryText)
                                Text(
                                    "\(parameter.presentationValue) · \(parameter.sourceText)")
                                    .font(.caption.monospacedDigit())
                                    .foregroundStyle(theme.secondaryText)
                            }
                            Spacer()
                            Button { model.toggleFavourite(parameter) } label: {
                                Image(systemName: parameter.favourite
                                      ? "star.fill" : "star")
                            }
                            .buttonStyle(.plain)
                            .foregroundStyle(theme.accent)
                            Button { model.togglePolling(parameter) } label: {
                                Image(systemName: parameter.pollingEnabled
                                      ? "waveform.path.ecg" : "pause.circle")
                            }
                            .buttonStyle(.plain)
                            .foregroundStyle(parameter.pollingEnabled
                                ? theme.success : theme.warning)
                        }
                        Divider()
                    }
                }
            }
            .padding(16)
        }
        .linkDiagnosticScreen("Table")
    }
}

private struct LinkStandardDashboardView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel
    @AppStorage("link.dashboard.presentationMode")
    private var dashboardModeKey = LinkDashboardPresentationMode.combined.rawValue
    private var mode: Binding<LinkDashboardPresentationMode> {
        Binding(
            get: {
                LinkDashboardPresentationMode(rawValue: dashboardModeKey)
                    ?? .combined
            },
            set: { dashboardModeKey = $0.rawValue })
    }

    var body: some View {
        ScrollView {
            LinkLabeledPanel(
                title: "Dashboard",
                systemImage: "gauge.with.dots.needle.67percent") {
                    LinkDashboardModePicker(selection: mode)
                    if model.dashboardParameters.isEmpty {
                        Text(
                            "No supported live measurements are available for the dashboard yet.")
                            .font(.subheadline)
                            .foregroundStyle(theme.secondaryText)
                    } else {
                        LazyVGrid(
                            columns: [GridItem(.adaptive(minimum: 150), spacing: 12)],
                            spacing: 12) {
                                ForEach(model.dashboardParameters) { parameter in
                                    LinkDashboardMetric(
                                        parameter: parameter,
                                        mode: mode.wrappedValue)
                                }
                            }
                    }
                }
                .padding(16)
        }
        .linkDiagnosticScreen("Dashboard")
    }
}

private struct LinkStandardEvidenceView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel

    var body: some View {
        ScrollView {
            LinkLabeledPanel(
                title: "Diagnostic evidence",
                systemImage: "doc.text.magnifyingglass") {
                    LinkStandardValueRow(
                        label: "Recorded samples",
                        value: "\(model.recordedSampleCount)",
                        icon: "waveform")
                    Button { model.prepareCSVExport() } label: {
                        Label(
                            model.isPreparingCSV ? "Preparing…" : "Prepare evidence CSV",
                            systemImage: "doc.badge.plus")
                            .frame(maxWidth: .infinity)
                            .padding(.vertical, 10)
                    }
                    .buttonStyle(.borderedProminent)
                    .tint(theme.accent)
                    .disabled(model.isPreparingCSV)
                    if let url = model.csvExportURL {
                        ShareLink(item: url) {
                            Label("Share CSV", systemImage: "square.and.arrow.up")
                                .frame(maxWidth: .infinity)
                        }
                        .buttonStyle(.bordered)
                        .tint(theme.accent)
                    }
                }
                .padding(16)
        }
        .linkDiagnosticScreen("Log")
    }
}

private struct LinkStandardGraphView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel
    private var graphable: [LinkDiagnosticParameter] {
        model.diagnosticParameters.filter {
            $0.vehicleSupported && $0.history.count > 1
        }
    }

    var body: some View {
        ScrollView {
            LinkLabeledPanel(title: "Graph", systemImage: "chart.xyaxis.line") {
                if graphable.isEmpty {
                    Text("Collect live samples to populate graphable parameters.")
                        .foregroundStyle(theme.secondaryText)
                } else {
                    ForEach(graphable.prefix(8)) { parameter in
                        LinkStandardValueRow(
                            label: parameter.title,
                            value: "\(parameter.history.count) samples · latest \(parameter.presentationValue)",
                            icon: "waveform")
                        Divider()
                    }
                }
                Text(
                    "LINK telemetry history is retained without inventing synthetic samples.")
                    .font(.caption)
                    .foregroundStyle(theme.secondaryText)
            }
            .padding(16)
        }
        .linkDiagnosticScreen("Graph")
    }
}

private struct LinkStandardTestsView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 15) {
                LinkLabeledPanel(
                    title: "Readiness",
                    systemImage: "checkmark.square.fill") {
                        LinkStandardValueRow(
                            label: "Status",
                            value: model.readinessStatusText,
                            icon: "checklist")
                        ForEach(model.readinessMonitorStatus, id: \.self) {
                            Text($0)
                                .font(.subheadline)
                                .foregroundStyle(theme.primaryText)
                        }
                    }
                LinkLabeledPanel(
                    title: "Freeze-frame context",
                    systemImage: "camera.metering.matrix") {
                        if model.freezeFrameContext.isEmpty {
                            Text("No standard freeze-frame context captured.")
                                .foregroundStyle(theme.secondaryText)
                        } else {
                            ForEach(model.freezeFrameContext, id: \.self) {
                                Text($0)
                                    .font(.subheadline)
                                    .foregroundStyle(theme.primaryText)
                            }
                        }
                    }
            }
            .padding(16)
        }
        .linkDiagnosticScreen("Tests")
    }
}

private struct LinkStandardServicesView: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    @ObservedObject var model: LinkStandardProductViewModel
    let appearance: LinkStandardProductAppearance

    var body: some View {
        ScrollView {
            LinkLabeledPanel(
                title: "Services",
                systemImage: "wrench.and.screwdriver.fill") {
                    Text(model.isActive
                         ? "No verified \(appearance.manufacturerName)-specific service procedure is enabled for this session."
                         : "Connect to evaluate supported service procedures.")
                        .font(.headline)
                        .foregroundStyle(theme.primaryText)
                    Text(
                        "Manufacturer procedures stay in \(appearance.productName); reusable execution and safety machinery stays in LINK.")
                        .font(.caption)
                        .foregroundStyle(theme.secondaryText)
                }
                .padding(16)
        }
        .linkDiagnosticScreen("Services")
    }
}

private struct LinkStandardSettingsView: View {
    @ObservedObject var model: LinkStandardProductViewModel
    let appearance: LinkStandardProductAppearance

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 15) {
                LinkLabeledPanel(
                    title: appearance.productName,
                    systemImage: "gearshape.fill") {
                        LinkStandardValueRow(
                            label: "Version",
                            value: model.versionText,
                            icon: "number.circle")
                        LinkStandardValueRow(
                            label: "Shared engine",
                            value: "LINK \(model.linkVersionText)",
                            icon: "square.stack.3d.up")
                    }
                LinkLabeledPanel(title: "Language", systemImage: "globe") {
                    Picker(
                        "Language",
                        selection: Binding(
                            get: { model.selectedLanguageID },
                            set: { model.selectLanguage($0) })) {
                                ForEach(Array(model.languageTags.indices), id: \.self) {
                                    index in
                                    Text(index < model.languageNames.count
                                         ? model.languageNames[index]
                                         : model.languageTags[index])
                                        .tag(model.languageTags[index])
                                }
                            }
                            .pickerStyle(.menu)
                }
                LinkLabeledPanel(title: "Unit system", systemImage: "ruler") {
                    Picker(
                        "Unit system",
                        selection: Binding(
                            get: { model.selectedMeasurementID },
                            set: { model.selectMeasurementSystem($0) })) {
                                ForEach(
                                    Array(model.measurementKeys.indices), id: \.self) {
                                        index in
                                        Text(index < model.measurementNames.count
                                             ? model.measurementNames[index]
                                             : model.measurementKeys[index])
                                            .tag(model.measurementKeys[index])
                                    }
                            }
                            .pickerStyle(.segmented)
                }
                LinkLabeledPanel(
                    title: "Dashboard measurements",
                    systemImage: "gauge.with.dots.needle.67percent") {
                        let supported = model.diagnosticParameters.filter(
                            \.vehicleSupported)
                        if supported.isEmpty {
                            Text("Connect once to choose supported dashboard measurements.")
                                .font(.subheadline)
                                .foregroundStyle(.secondary)
                        } else {
                            ForEach(supported) { parameter in
                                Button {
                                    model.toggleDashboard(parameter)
                                } label: {
                                    HStack {
                                        VStack(alignment: .leading, spacing: 2) {
                                            Text(parameter.title)
                                                .foregroundStyle(.primary)
                                            Text(parameter.shortName)
                                                .font(.caption)
                                                .foregroundStyle(.secondary)
                                        }
                                        Spacer()
                                        Image(systemName:
                                            model.dashboardSelected(parameter)
                                                ? "checkmark.circle.fill"
                                                : "circle")
                                    }
                                }
                                .buttonStyle(.plain)
                                if parameter.id != supported.last?.id {
                                    Divider()
                                }
                            }
                        }
                    }
            }
            .padding(16)
        }
        .linkDiagnosticScreen("Settings")
    }
}

private struct LinkStandardValueRow: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let label: String
    let value: String
    let icon: String

    var body: some View {
        HStack(alignment: .firstTextBaseline, spacing: 10) {
            Image(systemName: icon)
                .foregroundStyle(theme.accent)
                .frame(width: 18)
            Text(label)
                .font(.caption.bold())
                .foregroundStyle(theme.secondaryText)
            Spacer(minLength: 12)
            Text(value)
                .font(.subheadline)
                .foregroundStyle(theme.primaryText)
                .multilineTextAlignment(.trailing)
        }
    }
}

private struct LinkStandardFaultGroup: View {
    @Environment(\.linkDiagnosticTheme) private var theme
    let title: String
    let values: [String]

    var body: some View {
        VStack(alignment: .leading, spacing: 5) {
            Text(title)
                .font(.caption.bold())
                .foregroundStyle(theme.secondaryText)
            if values.isEmpty {
                Text("None reported")
                    .font(.subheadline)
                    .foregroundStyle(theme.secondaryText)
            } else {
                ForEach(values, id: \.self) {
                    Text($0)
                        .font(.body.monospaced())
                        .foregroundStyle(theme.primaryText)
                }
            }
        }
        .padding(.top, 4)
    }
}

#endif
