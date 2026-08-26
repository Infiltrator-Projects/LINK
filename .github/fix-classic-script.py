from pathlib import Path

p = Path('.github/apply-classic-bluetooth.py')
s = p.read_text()
s = s.replace(r"device[0] == '\0'", r"device[0] == '\\0'")
old = r'''            const sdp_record_t *record = item->data;
            const int port = record != NULL
                ? sdp_get_proto_port(record, RFCOMM_UUID) : -1;
            if (port > 0 && port <= 30) {
                channel = port;
                break;
            }
'''
new = r'''            const sdp_record_t *record = item->data;
            sdp_list_t *protocols = NULL;
            int port = -1;
            if (record != NULL &&
                sdp_get_access_protos(record, &protocols) == 0 &&
                protocols != NULL) {
                port = sdp_get_proto_port(protocols, RFCOMM_UUID);
            }
            if (protocols != NULL) {
                sdp_list_t *group;
                for (group = protocols; group != NULL; group = group->next)
                    sdp_list_free((sdp_list_t *)group->data, NULL);
                sdp_list_free(protocols, NULL);
            }
            if (port > 0 && port <= 30) {
                channel = port;
                break;
            }
'''
if old not in s:
    raise SystemExit('SDP block missing')
s = s.replace(old, new, 1)
p.write_text(s)
