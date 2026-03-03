#include "core/json_out.h"
#include "util/strutil.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cJSON *service_to_json(const nm_service_t *s)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "port", s->port);
    cJSON_AddStringToObject(obj, "proto", s->proto);
    if (s->name[0] != '\0')
        cJSON_AddStringToObject(obj, "name", s->name);
    if (s->version[0] != '\0')
        cJSON_AddStringToObject(obj, "version", s->version);
    return obj;
}

static cJSON *host_to_json(const nm_host_t *h)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "id", h->id);
    cJSON_AddStringToObject(obj, "display_name", h->display_name);
    cJSON_AddStringToObject(obj, "type", nm_host_type_str(h->type));
    cJSON_AddNumberToObject(obj, "hop_distance", h->hop_distance);

    if (h->rtt_ms >= 0)
        cJSON_AddNumberToObject(obj, "rtt_ms", h->rtt_ms);

    if (h->has_ipv4)
        cJSON_AddStringToObject(obj, "ipv4", nm_host_ipv4_str(h));

    /* Secondary IPv4 addresses */
    if (h->ipv4_count > 0) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < h->ipv4_count; i++) {
            char buf[NM_IPV4_STR_LEN];
            inet_ntop(AF_INET, &h->ipv4_addrs[i], buf, sizeof(buf));
            cJSON_AddItemToArray(arr, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(obj, "ipv4_addrs", arr);
    }

    if (h->has_ipv6) {
        char buf[NM_IPV6_STR_LEN];
        inet_ntop(AF_INET6, &h->ipv6, buf, sizeof(buf));
        cJSON_AddStringToObject(obj, "ipv6", buf);
    }

    /* Secondary IPv6 addresses */
    if (h->ipv6_count > 0) {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < h->ipv6_count; i++) {
            char buf[NM_IPV6_STR_LEN];
            inet_ntop(AF_INET6, &h->ipv6_addrs[i], buf, sizeof(buf));
            cJSON_AddItemToArray(arr, cJSON_CreateString(buf));
        }
        cJSON_AddItemToObject(obj, "ipv6_addrs", arr);
    }

    if (h->has_mac) {
        char mac[NM_MAC_STR_LEN];
        nm_mac_to_str(h->mac, mac, sizeof(mac));
        cJSON_AddStringToObject(obj, "mac", mac);
    }

    if (!nm_str_empty(h->hostname))
        cJSON_AddStringToObject(obj, "hostname", h->hostname);
    if (!nm_str_empty(h->mdns_name)) {
        /* Output unescaped mDNS name */
        char tmp[NM_HOSTNAME_LEN];
        nm_strlcpy(tmp, h->mdns_name, sizeof(tmp));
        nm_str_unescape_mdns(tmp);
        cJSON_AddStringToObject(obj, "mdns_name", tmp);
    }
    if (!nm_str_empty(h->dns_name))
        cJSON_AddStringToObject(obj, "dns_name", h->dns_name);
    if (!nm_str_empty(h->interfaces))
        cJSON_AddStringToObject(obj, "interfaces", h->interfaces);
    else if (!nm_str_empty(h->iface_name))
        cJSON_AddStringToObject(obj, "interface", h->iface_name);

    /* Extended fields for network-map */
    if (!nm_str_empty(h->os_name))
        cJSON_AddStringToObject(obj, "os_name", h->os_name);
    if (!nm_str_empty(h->manufacturer))
        cJSON_AddStringToObject(obj, "manufacturer", h->manufacturer);

    if (h->service_count > 0) {
        cJSON *svc_arr = cJSON_CreateArray();
        for (int i = 0; i < h->service_count; i++) {
            cJSON_AddItemToArray(svc_arr, service_to_json(&h->services[i]));
        }
        cJSON_AddItemToObject(obj, "services", svc_arr);
    }

    if (h->is_boundary)
        cJSON_AddBoolToObject(obj, "is_boundary", h->is_boundary);

    /* Layout coords */
    cJSON_AddNumberToObject(obj, "x", h->x);
    cJSON_AddNumberToObject(obj, "y", h->y);
    cJSON_AddNumberToObject(obj, "z", h->z);

    return obj;
}

static cJSON *edge_to_json(const nm_edge_t *e)
{
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "src_id", e->src_id);
    cJSON_AddNumberToObject(obj, "dst_id", e->dst_id);
    cJSON_AddNumberToObject(obj, "weight", e->weight);
    cJSON_AddStringToObject(obj, "type", nm_edge_type_str(e->type));
    cJSON_AddBoolToObject(obj, "in_mst", e->in_mst);
    return obj;
}

cJSON *nm_json_serialize(const nm_graph_t *g)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "version", "1.0");
    cJSON_AddNumberToObject(root, "host_count", g->host_count);
    cJSON_AddNumberToObject(root, "edge_count", g->edge_count);

    cJSON *hosts = cJSON_CreateArray();
    for (int i = 0; i < g->host_count; i++) {
        cJSON_AddItemToArray(hosts, host_to_json(&g->hosts[i]));
    }
    cJSON_AddItemToObject(root, "hosts", hosts);

    cJSON *edges = cJSON_CreateArray();
    for (int i = 0; i < g->edge_count; i++) {
        cJSON_AddItemToArray(edges, edge_to_json(&g->edges[i]));
    }
    cJSON_AddItemToObject(root, "edges", edges);

    return root;
}

/* --- Deserialization --- */

static const char *json_str(const cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsString(item))
        return cJSON_GetStringValue(item);
    return NULL;
}

static double json_num(const cJSON *obj, const char *key, double def)
{
    cJSON *item = cJSON_GetObjectItem(obj, key);
    if (item && cJSON_IsNumber(item))
        return cJSON_GetNumberValue(item);
    return def;
}

static void host_from_json(const cJSON *obj, nm_host_t *h)
{
    nm_host_init(h);

    h->id = (int)json_num(obj, "id", -1);
    h->hop_distance = (int)json_num(obj, "hop_distance", -1);
    h->rtt_ms = json_num(obj, "rtt_ms", -1.0);
    h->x = json_num(obj, "x", 0.0);
    h->y = json_num(obj, "y", 0.0);
    h->z = json_num(obj, "z", 0.0);

    const char *s;

    if ((s = json_str(obj, "type")))
        h->type = nm_host_type_from_str(s);

    if ((s = json_str(obj, "display_name")))
        nm_strlcpy(h->display_name, s, sizeof(h->display_name));

    if ((s = json_str(obj, "ipv4")))
        nm_host_set_ipv4(h, s);

    if ((s = json_str(obj, "ipv6")))
        nm_host_set_ipv6(h, s);

    if ((s = json_str(obj, "mac"))) {
        unsigned char mac[6];
        if (nm_str_to_mac(s, mac) == 0)
            nm_host_set_mac(h, mac);
    }

    if ((s = json_str(obj, "hostname")))
        nm_strlcpy(h->hostname, s, sizeof(h->hostname));
    if ((s = json_str(obj, "mdns_name")))
        nm_strlcpy(h->mdns_name, s, sizeof(h->mdns_name));
    if ((s = json_str(obj, "dns_name")))
        nm_strlcpy(h->dns_name, s, sizeof(h->dns_name));
    if ((s = json_str(obj, "os_name")))
        nm_strlcpy(h->os_name, s, sizeof(h->os_name));
    if ((s = json_str(obj, "manufacturer")))
        nm_strlcpy(h->manufacturer, s, sizeof(h->manufacturer));

    if ((s = json_str(obj, "interfaces")))
        nm_strlcpy(h->interfaces, s, sizeof(h->interfaces));
    else if ((s = json_str(obj, "interface")))
        nm_strlcpy(h->iface_name, s, sizeof(h->iface_name));

    cJSON *bnd = cJSON_GetObjectItem(obj, "is_boundary");
    if (bnd && cJSON_IsBool(bnd))
        h->is_boundary = cJSON_IsTrue(bnd);

    /* Secondary IPv4 addresses */
    cJSON *arr = cJSON_GetObjectItem(obj, "ipv4_addrs");
    if (arr && cJSON_IsArray(arr)) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (cJSON_IsString(item)) {
                struct in_addr a;
                if (inet_pton(AF_INET, cJSON_GetStringValue(item), &a) == 1)
                    nm_host_add_ipv4(h, a);
            }
        }
    }

    /* Secondary IPv6 addresses */
    arr = cJSON_GetObjectItem(obj, "ipv6_addrs");
    if (arr && cJSON_IsArray(arr)) {
        cJSON *item;
        cJSON_ArrayForEach(item, arr) {
            if (cJSON_IsString(item)) {
                struct in6_addr a;
                if (inet_pton(AF_INET6, cJSON_GetStringValue(item), &a) == 1)
                    nm_host_add_ipv6(h, &a);
            }
        }
    }

    /* Services */
    cJSON *svcs = cJSON_GetObjectItem(obj, "services");
    if (svcs && cJSON_IsArray(svcs)) {
        cJSON *svc;
        cJSON_ArrayForEach(svc, svcs) {
            int port = (int)json_num(svc, "port", 0);
            const char *proto = json_str(svc, "proto");
            const char *name = json_str(svc, "name");
            const char *version = json_str(svc, "version");
            nm_host_add_service(h, port, proto ? proto : "",
                                name ? name : "", version ? version : "");
        }
    }
}

nm_graph_t *nm_json_deserialize(const cJSON *root)
{
    if (!root) return NULL;

    nm_graph_t *g = nm_graph_create();
    if (!g) return NULL;

    cJSON *hosts = cJSON_GetObjectItem(root, "hosts");
    if (hosts && cJSON_IsArray(hosts)) {
        cJSON *hobj;
        cJSON_ArrayForEach(hobj, hosts) {
            nm_host_t h;
            host_from_json(hobj, &h);
            nm_graph_add_host(g, &h);
        }
    }

    cJSON *edges = cJSON_GetObjectItem(root, "edges");
    if (edges && cJSON_IsArray(edges)) {
        cJSON *eobj;
        cJSON_ArrayForEach(eobj, edges) {
            int src = (int)json_num(eobj, "src_id", 0);
            int dst = (int)json_num(eobj, "dst_id", 0);
            double weight = json_num(eobj, "weight", 1.0);
            const char *type_str = json_str(eobj, "type");
            nm_edge_type_t type = nm_edge_type_from_str(type_str);
            int idx = nm_graph_add_edge(g, src, dst, weight, type);

            cJSON *mst = cJSON_GetObjectItem(eobj, "in_mst");
            if (idx >= 0 && mst && cJSON_IsBool(mst))
                g->edges[idx].in_mst = cJSON_IsTrue(mst);
        }
    }

    return g;
}

nm_graph_t *nm_json_load_file(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open '%s'\n", path);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (len <= 0) {
        fprintf(stderr, "Error: '%s' is empty\n", path);
        fclose(fp);
        return NULL;
    }

    char *buf = malloc((size_t)len + 1);
    if (!buf) {
        fprintf(stderr, "Error: out of memory reading '%s'\n", path);
        fclose(fp);
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)len, fp);
    fclose(fp);
    buf[nread] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        fprintf(stderr, "Error: failed to parse JSON from '%s'\n", path);
        return NULL;
    }

    nm_graph_t *g = nm_json_deserialize(root);
    cJSON_Delete(root);

    if (!g) {
        fprintf(stderr, "Error: failed to deserialize graph from '%s'\n", path);
    }

    return g;
}
