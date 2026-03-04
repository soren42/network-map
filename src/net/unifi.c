#include "config.h"
#include "net/unifi.h"
#include "log.h"

#ifdef HAVE_CURL

#include "util/alloc.h"
#include "util/strutil.h"
#include "core/host.h"
#include "core/edge.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Maximum response size: 8 MB */
#define UNIFI_MAX_RESPONSE  (8 * 1024 * 1024)

/* ---------- internal helpers ---------------------------------------------- */

/* Run a curl command and return the response body as a malloc'd string.
   cookie_file is the path to the cookie jar (shared across requests).
   Returns NULL on failure. */
static char *curl_request(const char *method, const char *url,
                          const char *cookie_file, const char *post_data)
{
    char cmd[2048];
    int n;

    if (post_data) {
        n = snprintf(cmd, sizeof(cmd),
            "curl -sk -X %s "
            "-H 'Content-Type: application/json' "
            "-b '%s' -c '%s' "
            "-d '%s' "
            "'%s' 2>/dev/null",
            method, cookie_file, cookie_file, post_data, url);
    } else {
        n = snprintf(cmd, sizeof(cmd),
            "curl -sk -X %s "
            "-b '%s' -c '%s' "
            "'%s' 2>/dev/null",
            method, cookie_file, cookie_file, url);
    }

    if (n < 0 || n >= (int)sizeof(cmd)) {
        LOG_ERROR("UniFi: command too long");
        return NULL;
    }

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("UniFi: failed to run curl");
        return NULL;
    }

    size_t len = 0;
    char *body = nm_read_all_fp(fp, &len, UNIFI_MAX_RESPONSE);
    int status = pclose(fp);

    if (!body || len == 0) {
        LOG_WARN("UniFi: empty response from %s %s", method, url);
        nm_free(body);
        return NULL;
    }

    if (status != 0) {
        LOG_WARN("UniFi: curl exited with status %d", status);
    }

    return body;
}

/* Authenticate to the UniFi controller. Returns 0 on success. */
static int unifi_login(const char *host, const char *user,
                       const char *pass, const char *cookie_file)
{
    char url[512];
    snprintf(url, sizeof(url), "https://%s/api/auth/login", host);

    char post[512];
    snprintf(post, sizeof(post),
             "{\"username\":\"%s\",\"password\":\"%s\"}", user, pass);

    char *resp = curl_request("POST", url, cookie_file, post);
    if (!resp) {
        LOG_ERROR("UniFi: login failed - no response");
        return -1;
    }

    /* Check for success - response should be valid JSON without error */
    cJSON *root = cJSON_Parse(resp);
    nm_free(resp);

    if (!root) {
        LOG_ERROR("UniFi: login response is not valid JSON");
        return -1;
    }

    /* Check for error_code in response */
    cJSON *err = cJSON_GetObjectItem(root, "error_code");
    if (err && cJSON_IsNumber(err) && cJSON_GetNumberValue(err) != 0) {
        LOG_ERROR("UniFi: login error code %.0f", cJSON_GetNumberValue(err));
        cJSON_Delete(root);
        return -1;
    }

    cJSON_Delete(root);
    LOG_INFO("UniFi: login successful");
    return 0;
}

/* Determine host type from UniFi device type string */
static nm_host_type_t type_from_device_type(const char *dtype)
{
    if (!dtype) return NM_HOST_WORKSTATION;
    if (strstr(dtype, "usw") || strstr(dtype, "USW"))
        return NM_HOST_SWITCH;
    if (strstr(dtype, "uap") || strstr(dtype, "UAP") ||
        strstr(dtype, "U6") || strstr(dtype, "U7"))
        return NM_HOST_AP;
    if (strstr(dtype, "ugw") || strstr(dtype, "UGW") ||
        strstr(dtype, "udm") || strstr(dtype, "UDM") ||
        strstr(dtype, "udr") || strstr(dtype, "UDR"))
        return NM_HOST_GATEWAY;
    return NM_HOST_WORKSTATION;
}

/* Find or create a host by MAC address */
static int find_or_create_by_mac(nm_graph_t *g, const char *mac_str,
                                 const char *name, nm_host_type_t type)
{
    unsigned char mac[6];
    if (!mac_str || nm_str_to_mac(mac_str, mac) != 0)
        return -1;

    int idx = nm_graph_find_by_mac(g, mac);
    if (idx >= 0) {
        /* Enrich existing host if type is more specific */
        nm_host_t *h = &g->hosts[idx];
        if (h->type == NM_HOST_WORKSTATION &&
            type != NM_HOST_WORKSTATION) {
            h->type = type;
        }
        if (name && nm_str_empty(h->hostname))
            nm_strlcpy(h->hostname, name, sizeof(h->hostname));
        return idx;
    }

    /* Create new host */
    nm_host_t h;
    nm_host_init(&h);
    nm_host_set_mac(&h, mac);
    if (name)
        nm_strlcpy(h.hostname, name, sizeof(h.hostname));
    h.type = type;
    nm_host_compute_display_name(&h);
    return nm_graph_add_host(g, &h);
}

/* Parse UniFi devices (switches, APs, gateway) */
static int parse_devices(nm_graph_t *g, const char *host,
                         const char *site, const char *cookie_file)
{
    char url[512];
    snprintf(url, sizeof(url),
             "https://%s/proxy/network/api/s/%s/stat/device", host, site);

    char *resp = curl_request("GET", url, cookie_file, NULL);
    if (!resp) {
        LOG_WARN("UniFi: failed to fetch devices");
        return -1;
    }

    cJSON *root = cJSON_Parse(resp);
    nm_free(resp);
    if (!root) {
        LOG_ERROR("UniFi: failed to parse device JSON");
        return -1;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data)) {
        LOG_WARN("UniFi: no device data array");
        cJSON_Delete(root);
        return 0;
    }

    int count = 0;
    cJSON *dev;
    cJSON_ArrayForEach(dev, data) {
        cJSON *jmac  = cJSON_GetObjectItem(dev, "mac");
        cJSON *jname = cJSON_GetObjectItem(dev, "name");
        cJSON *jtype = cJSON_GetObjectItem(dev, "type");
        cJSON *jmodel = cJSON_GetObjectItem(dev, "model");
        cJSON *jip   = cJSON_GetObjectItem(dev, "ip");

        const char *mac_str  = cJSON_IsString(jmac)  ? cJSON_GetStringValue(jmac)  : NULL;
        const char *name_str = cJSON_IsString(jname)  ? cJSON_GetStringValue(jname) : NULL;
        const char *type_str = cJSON_IsString(jtype)  ? cJSON_GetStringValue(jtype) : NULL;
        const char *model_str = cJSON_IsString(jmodel) ? cJSON_GetStringValue(jmodel) : NULL;

        nm_host_type_t htype = type_from_device_type(type_str);
        if (htype == NM_HOST_WORKSTATION && model_str)
            htype = type_from_device_type(model_str);

        int dev_id = find_or_create_by_mac(g, mac_str, name_str, htype);
        if (dev_id < 0) continue;

        nm_host_t *h = &g->hosts[dev_id];
        h->connection_medium = NM_MEDIUM_WIRED;

        if (model_str)
            nm_strlcpy(h->unifi_device_type, model_str,
                       sizeof(h->unifi_device_type));

        /* Set IP if available */
        if (jip && cJSON_IsString(jip))
            nm_host_set_ipv4(h, cJSON_GetStringValue(jip));

        /* Parse port_table for switches to find uplink connections */
        cJSON *port_table = cJSON_GetObjectItem(dev, "port_table");
        if (port_table && cJSON_IsArray(port_table) &&
            htype == NM_HOST_SWITCH) {
            cJSON *port;
            cJSON_ArrayForEach(port, port_table) {
                cJSON *pnum = cJSON_GetObjectItem(port, "port_idx");
                cJSON *speed = cJSON_GetObjectItem(port, "speed");
                cJSON *lldp_info = cJSON_GetObjectItem(port, "lldp_table");

                if (!lldp_info || !cJSON_IsArray(lldp_info)) continue;

                cJSON *lldp_entry;
                cJSON_ArrayForEach(lldp_entry, lldp_info) {
                    cJSON *lchassis = cJSON_GetObjectItem(lldp_entry,
                                                          "lldp_chassis_id");
                    cJSON *lport = cJSON_GetObjectItem(lldp_entry,
                                                       "lldp_port_id");

                    if (!lchassis || !cJSON_IsString(lchassis)) continue;

                    int neighbor_id = find_or_create_by_mac(
                        g, cJSON_GetStringValue(lchassis), NULL,
                        NM_HOST_WORKSTATION);
                    if (neighbor_id < 0) continue;

                    if (!nm_graph_has_edge(g, dev_id, neighbor_id)) {
                        int eidx = nm_graph_add_edge(g, dev_id, neighbor_id,
                                                     0.5, NM_EDGE_L2);
                        if (eidx >= 0) {
                            nm_edge_t *e = &g->edges[eidx];
                            e->medium = NM_MEDIUM_WIRED;
                            if (pnum && cJSON_IsNumber(pnum))
                                e->src_port_num = (int)cJSON_GetNumberValue(pnum);
                            if (speed && cJSON_IsNumber(speed))
                                e->speed_mbps = (int)cJSON_GetNumberValue(speed);
                            if (lport && cJSON_IsString(lport))
                                nm_strlcpy(e->dst_port_name,
                                           cJSON_GetStringValue(lport),
                                           sizeof(e->dst_port_name));
                        }
                    }
                }
            }
        }

        count++;
        LOG_DEBUG("UniFi: device %d: %s (%s) type=%s",
                  dev_id, name_str ? name_str : "?",
                  mac_str ? mac_str : "?",
                  nm_host_type_str(htype));
    }

    cJSON_Delete(root);
    LOG_INFO("UniFi: parsed %d device(s)", count);
    return count;
}

/* Parse UniFi clients (wired + WiFi) */
static int parse_clients(nm_graph_t *g, const char *host,
                         const char *site, const char *cookie_file)
{
    char url[512];
    snprintf(url, sizeof(url),
             "https://%s/proxy/network/api/s/%s/stat/sta", host, site);

    char *resp = curl_request("GET", url, cookie_file, NULL);
    if (!resp) {
        LOG_WARN("UniFi: failed to fetch clients");
        return -1;
    }

    cJSON *root = cJSON_Parse(resp);
    nm_free(resp);
    if (!root) {
        LOG_ERROR("UniFi: failed to parse client JSON");
        return -1;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (!data || !cJSON_IsArray(data)) {
        LOG_WARN("UniFi: no client data array");
        cJSON_Delete(root);
        return 0;
    }

    int count = 0;
    cJSON *sta;
    cJSON_ArrayForEach(sta, data) {
        cJSON *jmac     = cJSON_GetObjectItem(sta, "mac");
        cJSON *jhostname = cJSON_GetObjectItem(sta, "hostname");
        cJSON *jname    = cJSON_GetObjectItem(sta, "name");
        cJSON *jip      = cJSON_GetObjectItem(sta, "ip");
        cJSON *jis_wired = cJSON_GetObjectItem(sta, "is_wired");

        const char *mac_str = cJSON_IsString(jmac) ?
                              cJSON_GetStringValue(jmac) : NULL;
        const char *name_str = NULL;
        if (jname && cJSON_IsString(jname))
            name_str = cJSON_GetStringValue(jname);
        else if (jhostname && cJSON_IsString(jhostname))
            name_str = cJSON_GetStringValue(jhostname);

        int client_id = find_or_create_by_mac(g, mac_str, name_str,
                                              NM_HOST_WORKSTATION);
        if (client_id < 0) continue;

        nm_host_t *h = &g->hosts[client_id];

        /* Set IP */
        if (jip && cJSON_IsString(jip))
            nm_host_set_ipv4(h, cJSON_GetStringValue(jip));

        int is_wired = (jis_wired && cJSON_IsTrue(jis_wired));

        if (is_wired) {
            /* Wired client: connect to switch */
            h->connection_medium = NM_MEDIUM_WIRED;

            cJSON *jsw_mac  = cJSON_GetObjectItem(sta, "sw_mac");
            cJSON *jsw_port = cJSON_GetObjectItem(sta, "sw_port");
            cJSON *jvlan    = cJSON_GetObjectItem(sta, "vlan");

            if (jsw_mac && cJSON_IsString(jsw_mac)) {
                unsigned char sw_mac[6];
                if (nm_str_to_mac(cJSON_GetStringValue(jsw_mac), sw_mac) == 0) {
                    memcpy(h->switch_mac, sw_mac, 6);
                    h->has_switch_info = 1;

                    int sw_id = nm_graph_find_by_mac(g, sw_mac);
                    if (sw_id >= 0 && !nm_graph_has_edge(g, sw_id, client_id)) {
                        int eidx = nm_graph_add_edge(g, sw_id, client_id,
                                                     0.5, NM_EDGE_L2);
                        if (eidx >= 0) {
                            nm_edge_t *e = &g->edges[eidx];
                            e->medium = NM_MEDIUM_WIRED;
                            if (jsw_port && cJSON_IsNumber(jsw_port)) {
                                e->src_port_num =
                                    (int)cJSON_GetNumberValue(jsw_port);
                                h->switch_port = e->src_port_num;
                            }
                        }
                    }
                }
            }

            if (jvlan && cJSON_IsNumber(jvlan))
                h->vlan_id = (int)cJSON_GetNumberValue(jvlan);

        } else {
            /* WiFi client: connect to AP */
            h->connection_medium = NM_MEDIUM_WIFI;

            cJSON *jap_mac  = cJSON_GetObjectItem(sta, "ap_mac");
            cJSON *jessid   = cJSON_GetObjectItem(sta, "essid");
            cJSON *jrssi    = cJSON_GetObjectItem(sta, "rssi");
            cJSON *jsignal  = cJSON_GetObjectItem(sta, "signal");

            if (jessid && cJSON_IsString(jessid))
                nm_strlcpy(h->wifi_ssid, cJSON_GetStringValue(jessid),
                           sizeof(h->wifi_ssid));

            /* Use signal if available, fall back to rssi */
            if (jsignal && cJSON_IsNumber(jsignal))
                h->wifi_signal = (int)cJSON_GetNumberValue(jsignal);
            else if (jrssi && cJSON_IsNumber(jrssi))
                h->wifi_signal = (int)cJSON_GetNumberValue(jrssi);

            if (jap_mac && cJSON_IsString(jap_mac)) {
                unsigned char ap_mac[6];
                if (nm_str_to_mac(cJSON_GetStringValue(jap_mac), ap_mac) == 0) {
                    int ap_id = nm_graph_find_by_mac(g, ap_mac);
                    if (ap_id >= 0 &&
                        !nm_graph_has_edge(g, ap_id, client_id)) {
                        int eidx = nm_graph_add_edge(g, ap_id, client_id,
                                                     0.5, NM_EDGE_WIFI);
                        if (eidx >= 0) {
                            nm_edge_t *e = &g->edges[eidx];
                            e->medium = NM_MEDIUM_WIFI;
                        }
                    }
                }
            }
        }

        count++;
        nm_host_compute_display_name(h);
        LOG_DEBUG("UniFi: client %d: %s (%s) %s",
                  client_id, name_str ? name_str : "?",
                  mac_str ? mac_str : "?",
                  is_wired ? "wired" : "wifi");
    }

    cJSON_Delete(root);
    LOG_INFO("UniFi: parsed %d client(s)", count);
    return count;
}

/* ---------- public API ---------------------------------------------------- */

int nm_unifi_available(const nm_config_t *cfg)
{
    if (!cfg) return 0;
    if (nm_str_empty(cfg->unifi_host)) return 0;
    if (nm_str_empty(cfg->unifi_user)) return 0;
    if (nm_str_empty(cfg->unifi_pass)) return 0;

    /* Check curl is in PATH */
    FILE *fp = popen("command -v curl >/dev/null 2>&1 && echo yes", "r");
    if (!fp) return 0;
    char buf[16];
    int got = (fread(buf, 1, sizeof(buf), fp) > 0);
    pclose(fp);
    return got;
}

int nm_unifi_discover(nm_graph_t *g, const nm_config_t *cfg)
{
    if (!g || !cfg) return -1;

    const char *host = cfg->unifi_host;
    const char *user = cfg->unifi_user;
    const char *pass = cfg->unifi_pass;
    const char *site = cfg->unifi_site;

    /* Default site */
    if (nm_str_empty(site))
        site = "default";

    LOG_INFO("UniFi: connecting to %s (site: %s)", host, site);

    /* Create temporary cookie file */
    char cookie_file[] = "/tmp/nm_unifi_XXXXXX";
    int fd = mkstemp(cookie_file);
    if (fd < 0) {
        LOG_ERROR("UniFi: failed to create temp cookie file");
        return -1;
    }
    close(fd);

    int total = 0;

    /* Login */
    if (unifi_login(host, user, pass, cookie_file) != 0) {
        LOG_ERROR("UniFi: authentication failed");
        unlink(cookie_file);
        return -1;
    }

    /* Fetch devices */
    int dev_count = parse_devices(g, host, site, cookie_file);
    if (dev_count > 0)
        total += dev_count;

    /* Fetch clients */
    int sta_count = parse_clients(g, host, site, cookie_file);
    if (sta_count > 0)
        total += sta_count;

    /* Cleanup */
    unlink(cookie_file);

    LOG_INFO("UniFi: discovered %d total entries", total);
    return total;
}

#else /* !HAVE_CURL */

int nm_unifi_available(const nm_config_t *cfg) { (void)cfg; return 0; }
int nm_unifi_discover(nm_graph_t *g, const nm_config_t *cfg)
{
    (void)g; (void)cfg;
    return 0;
}

#endif /* HAVE_CURL */
