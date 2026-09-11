from pathlib import Path
p=Path('platform/apple/LinkDiagnosticUI.swift')
s=p.read_text()
old='''    private var central: CBCentralManager?\n    private var adaptersByIdentifier = [String: LinkNearbyAdapter]()\n\n    private var nearbyAdapters: [LinkNearbyAdapter] {\n        adaptersByIdentifier.values\n            .sorted {\n                if $0.rssi != $1.rssi { return $0.rssi > $1.rssi }\n                if $0.name != $1.name { return $0.name < $1.name }\n                return $0.identifier < $1.identifier\n            }\n    }\n'''
new='''    private var central: CBCentralManager?\n    private var adaptersByIdentifier = [String: LinkNearbyAdapter]()\n    private var adapterDiscoveryOrder = [String]()\n\n    private var nearbyAdapters: [LinkNearbyAdapter] {\n        adapterDiscoveryOrder.compactMap { adaptersByIdentifier[$0] }\n    }\n'''
assert s.count(old)==1, s.count(old)
s=s.replace(old,new)
old='''        // The picker shows every peripheral iOS reports, including unnamed\n        // devices and the saved adapter. Adapter-name hints belong to automatic\n        // connection selection; they must not hide devices from manual choice.\n        adaptersByIdentifier[identifier] = LinkNearbyAdapter(\n'''
new='''        // The picker shows every peripheral iOS reports, including unnamed\n        // devices and the saved adapter. Adapter-name hints belong to automatic\n        // connection selection; they must not hide devices from manual choice.\n        // Freeze row order on first discovery. RSSI may update continuously,\n        // but a user must never have a row move underneath their finger.\n        if adaptersByIdentifier[identifier] == nil {\n            adapterDiscoveryOrder.append(identifier)\n        }\n        adaptersByIdentifier[identifier] = LinkNearbyAdapter(\n'''
assert s.count(old)==1
s=s.replace(old,new)
old='''    @objc private func scanAgain() {\n        adaptersByIdentifier.removeAll()\n        tableView.reloadSections(\n'''
new='''    @objc private func scanAgain() {\n        adaptersByIdentifier.removeAll()\n        adapterDiscoveryOrder.removeAll()\n        tableView.reloadSections(\n'''
assert s.count(old)==1
s=s.replace(old,new)
p.write_text(s)

r=Path('tests/apple/run-regressions.sh')
t=r.read_text()
anchor='''grep -Fq 'if (item->pid_valid && item->enabled) ++enabledPollingCount;' "$controller"\n'''
add=anchor+'''grep -Fq 'private var adapterDiscoveryOrder = [String]()' "$ui"\ngrep -Fq 'adapterDiscoveryOrder.compactMap { adaptersByIdentifier[$0] }' "$ui"\ngrep -Fq 'adapterDiscoveryOrder.append(identifier)' "$ui"\ngrep -Fq 'adapterDiscoveryOrder.removeAll()' "$ui"\nif grep -Fq 'if $0.rssi != $1.rssi' "$ui"; then\n    echo 'Apple picker must not reorder rows by live RSSI.' >&2\n    exit 1\nfi\n'''
assert t.count(anchor)==1
t=t.replace(anchor,add)
r.write_text(t)

v=Path('VERSION'); assert v.read_text().strip()=='0.15.14'; v.write_text('0.15.15\n')
h=Path('include/link/version.h'); x=h.read_text(); assert x.count('#define LINK_VERSION_STRING "0.15.14"')==1; h.write_text(x.replace('#define LINK_VERSION_STRING "0.15.14"','#define LINK_VERSION_STRING "0.15.15"'))
