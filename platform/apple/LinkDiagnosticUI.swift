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

struct LinkSavedVehicleProfileSummary: Identifiable {
    let id: String
    let vin: String
    let displayName: String
    let moduleCount: Int
    let responderCount: Int
    let updatedAt: Date?

    init(
        id: String,
        vin: String,
        displayName: String,
        moduleCount: Int,
        responderCount: Int,
        updatedAt: Date?
    ) {
        self.id = id
        self.vin = vin
        self.displayName = displayName
        self.moduleCount = moduleCount
        self.responderCount = responderCount
        self.updatedAt = updatedAt
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
            responderCount: Int(LinkVehicleProfileStandardResponderCount(profile)),
            updatedAt: timestamp.map { Date(timeIntervalSince1970: $0) })
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

    private var nearbyAdapters: [LinkNearbyAdapter] {
        adaptersByIdentifier.values
            .filter { $0.identifier != knownAdapterIdentifier }
            .sorted {
                if $0.rssi != $1.rssi { return $0.rssi > $1.rssi }
                if $0.name != $1.name { return $0.name < $1.name }
                return $0.identifier < $1.identifier
            }
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
            options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    @objc private func scanAgain() {
        adaptersByIdentifier.removeAll()
        tableView.reloadSections(
            IndexSet(integer: nearbySection), with: .automatic)
        startScan()
    }

    @objc private func cancel() {
        central?.stopScan()
        navigationController?.dismiss(animated: true)
    }
}

#endif
