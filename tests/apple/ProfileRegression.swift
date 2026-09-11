// SPDX-License-Identifier: GPL-3.0-or-later
import Foundation

final class RegressionController: LinkProductDiagnosticsController {
    var testActive = false
    var testVIN = "WDD2073022F123456"
    override var isActive: Bool { testActive }
    override var isReady: Bool { testActive }
    override var standardVINText: String { testVIN }
    override var storedDTCs: [String] { ["P0401"] }
    override var readinessStatusText: String { "captured" }
    override var freezeFrameContext: [String] { ["snapshot"] }
    override func startSimulated() { testActive = true }
    override func disconnect() { testActive = false }
}

@main
struct ProfileRegression {
    @MainActor static func main() {
        let namespace = "link-regression-" + UUID().uuidString
        defer {
            for key in UserDefaults.standard.dictionaryRepresentation().keys
                where key.contains(namespace) {
                UserDefaults.standard.removeObject(forKey: key)
            }
        }
        var flow = LinkDiagnosticFlowConfig()
        flow.init_timeout_ms = 4000
        flow.query_timeout_ms = 8000
        flow.live_timeout_ms = 2000
        let controller = RegressionController(
            productSlug: namespace, flowConfig: flow,
            liveStatusText: "live", simulatedLiveStatusText: "simulated",
            standardVINStatusText: "VIN", simulatedAdapterIdentifier: nil,
            simulatedVIN: nil)
        let model = LinkStandardProductViewModel(
            controller: controller,
            configuration: LinkStandardProductConfiguration(
                productName: "Regression", productNamespace: namespace,
                manufacturerName: "Test", vehicleName: "Test vehicle",
                versionText: "test"))
        let realVIN = "WDD2073022F654321"
        let store = model.vehicleProfileStore
        store.saveProfile(["displayName": "Real vehicle"], forVIN: realVIN)
        precondition(store.selectOfflineVehicle(withVIN: realVIN))
        model.refreshStandardState()
        model.startSimulatedDiagnostics()
        model.refreshStandardState()
        precondition(model.isSimulationActive && model.isReady)
        precondition(model.vehicleVINText == controller.testVIN)
        precondition(store.profile(forVIN: controller.testVIN) == nil,
                     "Simulation must not create a real vehicle profile")
        precondition(store.selectedVehicleVIN == realVIN,
                     "Simulation must not replace the selected real vehicle")
        model.disconnect()
        model.refreshStandardState()
        precondition(model.selectedVehicleVIN == realVIN)

        // A real session still saves standard faults/readiness/freeze-frame.
        controller.testVIN = realVIN
        controller.testActive = true
        model.refreshStandardState()
        precondition(store.profile(forVIN: realVIN)?["storedDTCs"] as? [String] == ["P0401"])
        // This is the same shared patch API used by the Mercedes scan callback.
        store.mergeProfileFields(["modules": [["tx": 2017, "rx": 2025]]], forVIN: realVIN)
        let saved = store.profile(forVIN: realVIN)!
        precondition(saved["storedDTCs"] as? [String] == ["P0401"])
        precondition(saved["readinessStatusText"] as? String == "captured")
        precondition(saved["freezeFrameContext"] as? [String] == ["snapshot"])
        precondition(saved["modules"] != nil)

        // Retained polling choices are controller policy, not scheduler lifetime.
        let pollingController = LinkDiagnosticsController(
            productSlug: namespace + "-polling", flowConfig: flow,
            liveStatusText: "live", simulatedLiveStatusText: "simulated",
            standardVINStatusText: "VIN")
        precondition(pollingController.pollingEnabled(forPID: 0x0C))
        precondition(pollingController.pollingEnabled(forPID: 0x0D))
        pollingController.setPollingEnabled(false, forPID: 0x0C)
        precondition(!pollingController.pollingEnabled(forPID: 0x0C))
        precondition(pollingController.pollingEnabled(forPID: 0x0D),
                     "Changing one PID must not mutate another PID choice")
        pollingController.setPollingEnabled(true, forPID: 0x0C)
        precondition(pollingController.pollingEnabled(forPID: 0x0C))

        print("LINK Apple simulation, profile-patch and polling-policy regressions passed")
    }
}
