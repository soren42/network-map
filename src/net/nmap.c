#include "net/nmap.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initial buffer size for reading nmap output */
#define NMAP_BUF_INIT  (64 * 1024)
#define NMAP_BUF_MAX   (16 * 1024 * 1024)

/* -----------------------------------------------------------
 * Helpers: extract XML attribute values using strstr/sscanf
 * ----------------------------------------------------------- */

/* Extract the value of an XML attribute from a tag region.
   Searches within [start, end) for attr="value" and copies
   the value into dst (up to dst_size-1 chars).
   Returns 0 on success, -1 if attribute not found. */
static int xml_attr(const char *start, const char *end,
                    const char *attr, char *dst, size_t dst_size)
{
    if (!start || !end || !attr || !dst || dst_size == 0)
        return -1;

    dst[0] = '\0';

    /* Build search key: attr=" */
    char key[128];
    snprintf(key, sizeof(key), "%s=\"", attr);

    const char *p = start;
    while (p < end) {
        p = strstr(p, key);
        if (!p || p >= end) return -1;

        p += strlen(key);
        const char *q = strchr(p, '"');
        if (!q || q > end) return -1;

        size_t len = (size_t)(q - p);
        if (len >= dst_size) len = dst_size - 1;
        memcpy(dst, p, len);
        dst[len] = '\0';
        return 0;
    }
    return -1;
}

/* Find the next occurrence of a tag within [start, end).
   Returns pointer to the '<' of the tag, or NULL. */
static const char *xml_find_tag(const char *start, const char *end,
                                const char *tag)
{
    if (!start || !end || !tag) return NULL;

    char open[128];
    snprintf(open, sizeof(open), "<%s", tag);

    const char *p = start;
    while (p < end) {
        p = strstr(p, open);
        if (!p || p >= end) return NULL;

        /* Make sure we matched the full tag name - next char should be
           space, '>', '/', or end of tag delimiter */
        char c = p[strlen(open)];
        if (c == ' ' || c == '>' || c == '/' || c == '\t' || c == '\n')
            return p;
        p += strlen(open);
    }
    return NULL;
}

/* Find the matching closing tag for a block element.
   Given a pointer to "<tag ...>", finds "</tag>".
   Returns pointer past "</tag>", or NULL. */
static const char *xml_find_close(const char *start, const char *end,
                                  const char *tag)
{
    if (!start || !end || !tag) return NULL;

    char close_tag[128];
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *p = strstr(start, close_tag);
    if (!p || p >= end) return NULL;
    return p + strlen(close_tag);
}

/* -----------------------------------------------------------
 * Public API
 * ----------------------------------------------------------- */

int nm_nmap_available(void)
{
    FILE *fp = popen("nmap --version 2>/dev/null", "r");
    if (!fp) return 0;

    char buf[256];
    int found = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, "Nmap") || strstr(buf, "nmap")) {
            found = 1;
        }
    }

    int status = pclose(fp);
    return (found && status == 0) ? 1 : 0;
}

/* Read all output from a FILE* into a malloc'd buffer.
   Returns buffer (NUL-terminated) and sets *out_len.
   Caller must nm_free() the result. Returns NULL on error. */
static char *read_all(FILE *fp, size_t *out_len)
{
    size_t cap = NMAP_BUF_INIT;
    size_t len = 0;
    char *buf = nm_malloc(cap);

    while (!feof(fp)) {
        if (len + 4096 > cap) {
            if (cap >= NMAP_BUF_MAX) {
                LOG_ERROR("nmap: output exceeds %d MB limit",
                          (int)(NMAP_BUF_MAX / (1024 * 1024)));
                nm_free(buf);
                return NULL;
            }
            cap *= 2;
            if (cap > NMAP_BUF_MAX) cap = NMAP_BUF_MAX;
            buf = nm_realloc(buf, cap);
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        if (n == 0) break;
        len += n;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}

/* Parse a single <host>...</host> block and enrich the graph.
   Returns 1 if a host was enriched, 0 otherwise. */
static int parse_host_block(nm_graph_t *g, const char *host_start,
                            const char *host_end)
{
    char ip_str[NM_IPV4_STR_LEN];
    char mac_str[NM_MAC_STR_LEN];
    char vendor[128];
    char os_name[128];

    ip_str[0] = '\0';
    mac_str[0] = '\0';
    vendor[0] = '\0';
    os_name[0] = '\0';

    /* Find all <address> tags - extract IPv4 and MAC+vendor */
    const char *p = host_start;
    while (p < host_end) {
        const char *addr_tag = xml_find_tag(p, host_end, "address");
        if (!addr_tag) break;

        /* Find end of this tag (either /> or >) */
        const char *tag_end = strchr(addr_tag, '>');
        if (!tag_end || tag_end >= host_end) break;

        char addrtype[32];
        char addrval[64];
        addrtype[0] = '\0';
        addrval[0] = '\0';

        xml_attr(addr_tag, tag_end + 1, "addrtype", addrtype, sizeof(addrtype));
        xml_attr(addr_tag, tag_end + 1, "addr", addrval, sizeof(addrval));

        if (strcmp(addrtype, "ipv4") == 0 && addrval[0] != '\0') {
            nm_strlcpy(ip_str, addrval, sizeof(ip_str));
        } else if (strcmp(addrtype, "mac") == 0 && addrval[0] != '\0') {
            nm_strlcpy(mac_str, addrval, sizeof(mac_str));
            xml_attr(addr_tag, tag_end + 1, "vendor", vendor, sizeof(vendor));
        }

        p = tag_end + 1;
    }

    if (ip_str[0] == '\0') {
        LOG_TRACE("nmap: skipping host block without IPv4 address");
        return 0;
    }

    /* Resolve to graph host */
    struct in_addr addr;
    if (inet_pton(AF_INET, ip_str, &addr) != 1) {
        LOG_WARN("nmap: invalid IP in XML: '%s'", ip_str);
        return 0;
    }

    int host_id = nm_graph_find_by_ipv4(g, addr);
    if (host_id < 0) {
        /* Host not yet in graph - add it */
        nm_host_t h;
        nm_host_init(&h);
        nm_host_set_ipv4_addr(&h, addr);
        host_id = nm_graph_add_host(g, &h);
        LOG_DEBUG("nmap: added new host %s (id %d)", ip_str, host_id);
    }

    nm_host_t *host = &g->hosts[host_id];

    /* Apply MAC and vendor */
    if (mac_str[0] != '\0' && !host->has_mac) {
        unsigned char mac[NM_MAC_LEN];
        if (nm_str_to_mac(mac_str, mac) == 0) {
            nm_host_set_mac(host, mac);
        }
    }
    if (vendor[0] != '\0' && nm_str_empty(host->manufacturer)) {
        nm_strlcpy(host->manufacturer, vendor, sizeof(host->manufacturer));
    }

    /* Extract OS from first <osmatch> tag */
    const char *osmatch = xml_find_tag(host_start, host_end, "osmatch");
    if (osmatch) {
        const char *os_tag_end = strchr(osmatch, '>');
        if (os_tag_end && os_tag_end < host_end) {
            xml_attr(osmatch, os_tag_end + 1, "name", os_name, sizeof(os_name));
            if (os_name[0] != '\0' && nm_str_empty(host->os_name)) {
                nm_strlcpy(host->os_name, os_name, sizeof(host->os_name));
                LOG_DEBUG("nmap: %s OS = %s", ip_str, os_name);
            }
        }
    }

    /* Extract services from <port>...<service .../></port> blocks */
    const char *ports_start = xml_find_tag(host_start, host_end, "ports");
    const char *ports_end = ports_start
                            ? xml_find_close(ports_start, host_end, "ports")
                            : NULL;
    if (!ports_end) ports_end = host_end;
    if (!ports_start) ports_start = host_start;

    p = ports_start;
    while (p < ports_end) {
        const char *port_tag = xml_find_tag(p, ports_end, "port");
        if (!port_tag) break;

        /* Find end of <port ...> opening tag */
        const char *port_tag_end = strchr(port_tag, '>');
        if (!port_tag_end || port_tag_end >= ports_end) break;

        /* Find </port> for this block */
        const char *port_close = xml_find_close(port_tag, ports_end, "port");
        if (!port_close) port_close = ports_end;

        /* Extract port number and protocol from <port> tag */
        char portid_str[16];
        char proto[8];
        portid_str[0] = '\0';
        proto[0] = '\0';
        xml_attr(port_tag, port_tag_end + 1, "portid", portid_str,
                 sizeof(portid_str));
        xml_attr(port_tag, port_tag_end + 1, "protocol", proto, sizeof(proto));

        int port_num = 0;
        if (portid_str[0] != '\0') {
            port_num = atoi(portid_str);
        }

        /* Check <state state="open"/> */
        const char *state_tag = xml_find_tag(port_tag, port_close, "state");
        if (state_tag) {
            const char *state_end = strchr(state_tag, '>');
            if (state_end && state_end < port_close) {
                char state_val[32];
                xml_attr(state_tag, state_end + 1, "state", state_val,
                         sizeof(state_val));
                if (strcmp(state_val, "open") != 0) {
                    /* Skip non-open ports */
                    p = port_close;
                    continue;
                }
            }
        }

        /* Extract service info from <service> tag */
        char svc_name[64];
        char svc_product[128];
        char svc_version[64];
        char svc_full[128];
        svc_name[0] = '\0';
        svc_product[0] = '\0';
        svc_version[0] = '\0';
        svc_full[0] = '\0';

        const char *svc_tag = xml_find_tag(port_tag, port_close, "service");
        if (svc_tag) {
            const char *svc_end = strchr(svc_tag, '>');
            if (svc_end && svc_end < port_close) {
                xml_attr(svc_tag, svc_end + 1, "name", svc_name,
                         sizeof(svc_name));
                xml_attr(svc_tag, svc_end + 1, "product", svc_product,
                         sizeof(svc_product));
                xml_attr(svc_tag, svc_end + 1, "version", svc_version,
                         sizeof(svc_version));
            }
        }

        /* Build version string: "product version" */
        if (svc_product[0] != '\0') {
            nm_strlcpy(svc_full, svc_product, sizeof(svc_full));
            if (svc_version[0] != '\0') {
                nm_strlcat(svc_full, " ", sizeof(svc_full));
                nm_strlcat(svc_full, svc_version, sizeof(svc_full));
            }
        }

        if (port_num > 0 && svc_name[0] != '\0') {
            nm_host_add_service(host, port_num,
                                proto[0] ? proto : "tcp",
                                svc_name, svc_full);
            LOG_DEBUG("nmap: %s port %d/%s = %s (%s)",
                      ip_str, port_num, proto, svc_name, svc_full);
        }

        p = port_close;
    }

    /* Re-classify host type based on newly discovered services */
    nm_host_classify(host);

    LOG_INFO("nmap: enriched host %s (id %d)", ip_str, host_id);
    return 1;
}

int nm_nmap_scan_subnet(nm_graph_t *g, const char *cidr)
{
    if (!g || !cidr || cidr[0] == '\0') {
        LOG_ERROR("nmap: invalid arguments");
        return -1;
    }

    /* Validate CIDR looks reasonable (basic sanity check) */
    if (strlen(cidr) > 64) {
        LOG_ERROR("nmap: CIDR string too long");
        return -1;
    }

    /* Build command: nmap -sV -O -oX - <cidr>
       -sV = service/version detection
       -O  = OS detection
       -oX - = XML output to stdout */
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "nmap -sV -O -oX - %s 2>/dev/null", cidr);

    LOG_INFO("nmap: scanning %s ...", cidr);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        LOG_ERROR("nmap: failed to execute: %s", cmd);
        return -1;
    }

    size_t xml_len = 0;
    char *xml = read_all(fp, &xml_len);
    int status = pclose(fp);

    if (!xml) {
        LOG_ERROR("nmap: failed to read output");
        return -1;
    }

    if (status != 0) {
        LOG_WARN("nmap: process exited with status %d", status);
        /* Continue anyway - partial results may be useful */
    }

    if (xml_len == 0) {
        LOG_WARN("nmap: empty output");
        nm_free(xml);
        return 0;
    }

    LOG_DEBUG("nmap: read %zu bytes of XML output", xml_len);

    /* Parse each <host>...</host> block */
    const char *end = xml + xml_len;
    const char *p = xml;
    int enriched = 0;

    while (p < end) {
        const char *host_start = xml_find_tag(p, end, "host");
        if (!host_start) break;

        const char *host_end = xml_find_close(host_start, end, "host");
        if (!host_end) {
            LOG_WARN("nmap: unclosed <host> tag");
            break;
        }

        enriched += parse_host_block(g, host_start, host_end);
        p = host_end;
    }

    nm_free(xml);
    LOG_INFO("nmap: enriched %d hosts from %s", enriched, cidr);
    return enriched;
}
