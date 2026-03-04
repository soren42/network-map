#include "config.h"
#include "net/lldp.h"
#include "log.h"

#ifdef HAVE_LLDP

#include "util/alloc.h"
#include "util/strutil.h"
#include "core/host.h"
#include "core/edge.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int nm_lldp_available(void)
{
    FILE *fp = popen("lldpcli -f json show configuration 2>/dev/null", "r");
    if (!fp) return 0;
    char buf[256];
    int got = (fread(buf, 1, sizeof(buf), fp) > 0);
    int status = pclose(fp);
    return (got && status == 0);
}

/* Determine host type from LLDP capability string */
static nm_host_type_t type_from_capabilities(const char *cap)
{
    if (!cap) return NM_HOST_WORKSTATION;
    if (strstr(cap, "Bridge"))  return NM_HOST_SWITCH;
    if (strstr(cap, "Router"))  return NM_HOST_GATEWAY;
    if (strstr(cap, "WLAN"))    return NM_HOST_AP;
    if (strstr(cap, "Station")) return NM_HOST_WORKSTATION;
    return NM_HOST_WORKSTATION;
}

/* Parse a single LLDP neighbor entry */
static int parse_neighbor(nm_graph_t *g, const cJSON *iface_obj,
                          const char *local_iface)
{
    int count = 0;
    cJSON *chassis = cJSON_GetObjectItem(iface_obj, "chassis");
    cJSON *port_obj = cJSON_GetObjectItem(iface_obj, "port");
    if (!chassis) return 0;

    /* Get chassis ID (MAC) - first key in chassis object */
    cJSON *chassis_entry = chassis->child;
    if (!chassis_entry) return 0;

    const char *sys_name = chassis_entry->string;
    cJSON *id_obj = cJSON_GetObjectItem(chassis_entry, "id");
    const char *chassis_id = NULL;
    if (id_obj) {
        cJSON *val = cJSON_GetObjectItem(id_obj, "value");
        if (val && cJSON_IsString(val))
            chassis_id = cJSON_GetStringValue(val);
    }

    /* Capability */
    const char *cap_str = NULL;
    cJSON *cap_arr = cJSON_GetObjectItem(chassis_entry, "capability");
    if (cap_arr && cJSON_IsArray(cap_arr)) {
        /* Collect enabled capabilities */
        cJSON *c;
        cJSON_ArrayForEach(c, cap_arr) {
            cJSON *enabled = cJSON_GetObjectItem(c, "enabled");
            cJSON *ctype = cJSON_GetObjectItem(c, "type");
            if (enabled && cJSON_IsTrue(enabled) &&
                ctype && cJSON_IsString(ctype)) {
                cap_str = cJSON_GetStringValue(ctype);
            }
        }
    }

    /* Port info */
    const char *port_id = NULL;
    const char *port_descr = NULL;
    if (port_obj) {
        cJSON *pid = cJSON_GetObjectItem(port_obj, "id");
        if (pid) {
            cJSON *val = cJSON_GetObjectItem(pid, "value");
            if (val && cJSON_IsString(val))
                port_id = cJSON_GetStringValue(val);
        }
        cJSON *pdesc = cJSON_GetObjectItem(port_obj, "descr");
        if (pdesc && cJSON_IsString(pdesc))
            port_descr = cJSON_GetStringValue(pdesc);
    }

    /* VLAN */
    int vlan_id = 0;
    cJSON *vlan = cJSON_GetObjectItem(iface_obj, "vlan");
    if (vlan) {
        cJSON *vid = NULL;
        if (cJSON_IsArray(vlan))
            vid = cJSON_GetArrayItem(vlan, 0);
        else if (cJSON_IsObject(vlan))
            vid = vlan;
        if (vid) {
            cJSON *v = cJSON_GetObjectItem(vid, "vlan-id");
            if (v && cJSON_IsNumber(v))
                vlan_id = (int)cJSON_GetNumberValue(v);
        }
    }

    /* Find or create host by MAC */
    int host_id = -1;
    unsigned char mac[6];
    if (chassis_id && nm_str_to_mac(chassis_id, mac) == 0) {
        host_id = nm_graph_find_by_mac(g, mac);
    }

    if (host_id < 0) {
        /* Create new host */
        nm_host_t h;
        nm_host_init(&h);
        if (chassis_id && nm_str_to_mac(chassis_id, mac) == 0)
            nm_host_set_mac(&h, mac);
        if (sys_name)
            nm_strlcpy(h.hostname, sys_name, sizeof(h.hostname));
        h.type = type_from_capabilities(cap_str);
        h.connection_medium = NM_MEDIUM_WIRED;
        if (vlan_id > 0)
            h.vlan_id = vlan_id;
        nm_host_compute_display_name(&h);
        host_id = nm_graph_add_host(g, &h);
        LOG_DEBUG("LLDP: new host %d: %s (%s)",
                  host_id, sys_name ? sys_name : "?",
                  chassis_id ? chassis_id : "?");
    } else {
        /* Enrich existing host */
        nm_host_t *h = &g->hosts[host_id];
        if (h->type == NM_HOST_WORKSTATION)
            h->type = type_from_capabilities(cap_str);
        if (vlan_id > 0 && h->vlan_id == 0)
            h->vlan_id = vlan_id;
    }

    /* Find local host by interface name */
    int local_id = nm_graph_find_by_iface(g, local_iface);
    if (local_id < 0) {
        /* Try finding LOCAL host */
        for (int i = 0; i < g->host_count; i++) {
            if (g->hosts[i].type == NM_HOST_LOCAL) {
                local_id = i;
                break;
            }
        }
    }

    /* Create L2 edge */
    if (local_id >= 0 && host_id >= 0 &&
        !nm_graph_has_edge(g, local_id, host_id)) {
        int eidx = nm_graph_add_edge(g, local_id, host_id,
                                     0.5, NM_EDGE_L2);
        if (eidx >= 0) {
            nm_edge_t *e = &g->edges[eidx];
            e->medium = NM_MEDIUM_WIRED;
            if (port_id)
                nm_strlcpy(e->dst_port_name, port_id,
                           sizeof(e->dst_port_name));
            if (port_descr)
                nm_strlcpy(e->src_port_name, port_descr,
                           sizeof(e->src_port_name));
            count++;
        }
    }

    return count;
}

int nm_lldp_discover(nm_graph_t *g)
{
    if (!g) return -1;

    LOG_INFO("LLDP: querying lldpcli");

    FILE *fp = popen("lldpcli -f json show neighbors 2>/dev/null", "r");
    if (!fp) {
        LOG_WARN("LLDP: failed to run lldpcli");
        return -1;
    }

    size_t json_len = 0;
    char *json_str = nm_read_all_fp(fp, &json_len, 0);
    int status = pclose(fp);

    if (!json_str || json_len == 0) {
        LOG_WARN("LLDP: no output from lldpcli");
        nm_free(json_str);
        return 0;
    }

    if (status != 0) {
        LOG_WARN("LLDP: lldpcli exited with status %d", status);
    }

    cJSON *root = cJSON_Parse(json_str);
    nm_free(json_str);

    if (!root) {
        LOG_ERROR("LLDP: failed to parse JSON");
        return -1;
    }

    int total = 0;

    /* Navigate: lldp -> interface -> { iface_name -> [neighbors] } */
    cJSON *lldp = cJSON_GetObjectItem(root, "lldp");
    if (!lldp) lldp = root;

    cJSON *ifaces = cJSON_GetObjectItem(lldp, "interface");
    if (ifaces && cJSON_IsArray(ifaces)) {
        cJSON *iface_entry;
        cJSON_ArrayForEach(iface_entry, ifaces) {
            /* Each entry is { "eth0": { ... neighbor data ... } } */
            cJSON *inner = iface_entry->child;
            if (!inner) continue;
            const char *iface_name = inner->string;
            total += parse_neighbor(g, inner, iface_name);
        }
    } else if (ifaces && cJSON_IsObject(ifaces)) {
        /* Alternate format: interface is an object keyed by iface name */
        cJSON *inner = ifaces->child;
        while (inner) {
            const char *iface_name = inner->string;
            if (cJSON_IsArray(inner)) {
                cJSON *entry;
                cJSON_ArrayForEach(entry, inner) {
                    total += parse_neighbor(g, entry, iface_name);
                }
            } else {
                total += parse_neighbor(g, inner, iface_name);
            }
            inner = inner->next;
        }
    }

    cJSON_Delete(root);
    LOG_INFO("LLDP: discovered %d neighbor(s)", total);
    return total;
}

#else /* !HAVE_LLDP */

int nm_lldp_available(void) { return 0; }
int nm_lldp_discover(nm_graph_t *g) { (void)g; return 0; }

#endif /* HAVE_LLDP */
