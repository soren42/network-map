#include "core/host.h"
#include "util/strutil.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

void nm_host_init(nm_host_t *h)
{
    memset(h, 0, sizeof(*h));
    h->id = -1;
    h->type = NM_HOST_WORKSTATION;
    h->hop_distance = -1;
    h->rtt_ms = -1.0;
}

int nm_host_set_ipv4(nm_host_t *h, const char *addr)
{
    if (inet_pton(AF_INET, addr, &h->ipv4) != 1) {
        return -1;
    }
    h->has_ipv4 = 1;
    return 0;
}

int nm_host_set_ipv4_addr(nm_host_t *h, struct in_addr addr)
{
    h->ipv4 = addr;
    h->has_ipv4 = 1;
    return 0;
}

int nm_host_set_ipv6(nm_host_t *h, const char *addr)
{
    if (inet_pton(AF_INET6, addr, &h->ipv6) != 1) {
        return -1;
    }
    h->has_ipv6 = 1;
    return 0;
}

int nm_host_set_mac(nm_host_t *h, const unsigned char *mac)
{
    memcpy(h->mac, mac, NM_MAC_LEN);
    h->has_mac = 1;
    return 0;
}

void nm_host_add_ipv4(nm_host_t *h, struct in_addr addr)
{
    /* If primary slot is empty, use it */
    if (!h->has_ipv4) {
        h->ipv4 = addr;
        h->has_ipv4 = 1;
        return;
    }
    /* Skip if duplicate of primary */
    if (h->ipv4.s_addr == addr.s_addr) return;
    /* Skip if duplicate of existing secondary */
    for (int i = 0; i < h->ipv4_count; i++) {
        if (h->ipv4_addrs[i].s_addr == addr.s_addr) return;
    }
    /* Add to secondary list */
    if (h->ipv4_count < NM_MAX_ADDRS)
        h->ipv4_addrs[h->ipv4_count++] = addr;
}

void nm_host_add_ipv6(nm_host_t *h, const struct in6_addr *addr)
{
    if (!h->has_ipv6) {
        memcpy(&h->ipv6, addr, sizeof(struct in6_addr));
        h->has_ipv6 = 1;
        return;
    }
    if (memcmp(&h->ipv6, addr, sizeof(struct in6_addr)) == 0) return;
    for (int i = 0; i < h->ipv6_count; i++) {
        if (memcmp(&h->ipv6_addrs[i], addr, sizeof(struct in6_addr)) == 0) return;
    }
    if (h->ipv6_count < NM_MAX_ADDRS)
        memcpy(&h->ipv6_addrs[h->ipv6_count++], addr, sizeof(struct in6_addr));
}

void nm_host_add_iface(nm_host_t *h, const char *iface)
{
    if (!iface || iface[0] == '\0') return;

    /* Check if already listed */
    if (h->interfaces[0] != '\0') {
        /* Search for exact match in comma-separated list */
        const char *p = h->interfaces;
        size_t ilen = strlen(iface);
        while (*p) {
            if (strncmp(p, iface, ilen) == 0 &&
                (p[ilen] == ',' || p[ilen] == '\0'))
                return; /* already present */
            p = strchr(p, ',');
            if (!p) break;
            p++; /* skip comma */
        }
        nm_strlcat(h->interfaces, ",", sizeof(h->interfaces));
    }
    nm_strlcat(h->interfaces, iface, sizeof(h->interfaces));

    /* Set primary iface_name if empty */
    if (h->iface_name[0] == '\0')
        nm_strlcpy(h->iface_name, iface, sizeof(h->iface_name));
}

int nm_host_add_service(nm_host_t *h, int port, const char *proto,
                        const char *name, const char *version)
{
    if (h->service_count >= NM_MAX_SERVICES)
        return -1;
    nm_service_t *s = &h->services[h->service_count];
    s->port = port;
    nm_strlcpy(s->proto, proto ? proto : "", sizeof(s->proto));
    nm_strlcpy(s->name, name ? name : "", sizeof(s->name));
    nm_strlcpy(s->version, version ? version : "", sizeof(s->version));
    h->service_count++;
    return 0;
}

/* Case-insensitive substring search */
static int str_contains_ci(const char *haystack, const char *needle)
{
    if (!haystack || !needle) return 0;
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return 0;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        int match = 1;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = 0;
                break;
            }
        }
        if (match) return 1;
    }
    return 0;
}

/* Check if any service matches a given port */
static int has_service_port(const nm_host_t *h, int port)
{
    for (int i = 0; i < h->service_count; i++) {
        if (h->services[i].port == port) return 1;
    }
    return 0;
}

void nm_host_classify(nm_host_t *h)
{
    /* Don't reclassify LOCAL, GATEWAY, BOUNDARY, SWITCH, or AP types */
    if (h->type == NM_HOST_LOCAL ||
        h->type == NM_HOST_GATEWAY ||
        h->type == NM_HOST_BOUNDARY ||
        h->type == NM_HOST_SWITCH ||
        h->type == NM_HOST_AP)
        return;

    /* Check mDNS name for printer indicators */
    if (!nm_str_empty(h->mdns_name)) {
        if (str_contains_ci(h->mdns_name, "_printer") ||
            str_contains_ci(h->mdns_name, "_ipp") ||
            str_contains_ci(h->mdns_name, "_pdl-datastream") ||
            str_contains_ci(h->mdns_name, "_scanner")) {
            h->type = NM_HOST_PRINTER;
            return;
        }
        /* Printer brand names in mDNS instance name */
        if (str_contains_ci(h->mdns_name, "hp ") ||
            str_contains_ci(h->mdns_name, "envy") ||
            str_contains_ci(h->mdns_name, "laserjet") ||
            str_contains_ci(h->mdns_name, "deskjet") ||
            str_contains_ci(h->mdns_name, "officejet") ||
            str_contains_ci(h->mdns_name, "canon ") ||
            str_contains_ci(h->mdns_name, "pixma") ||
            str_contains_ci(h->mdns_name, "epson ") ||
            str_contains_ci(h->mdns_name, "brother ") ||
            str_contains_ci(h->mdns_name, "xerox")) {
            h->type = NM_HOST_PRINTER;
            return;
        }
    }

    /* Check hostname for printer brands */
    if (!nm_str_empty(h->hostname)) {
        if (str_contains_ci(h->hostname, "printer") ||
            str_contains_ci(h->hostname, "hp ") ||
            str_contains_ci(h->hostname, "envy") ||
            str_contains_ci(h->hostname, "laserjet") ||
            str_contains_ci(h->hostname, "deskjet") ||
            str_contains_ci(h->hostname, "officejet") ||
            str_contains_ci(h->hostname, "epson") ||
            str_contains_ci(h->hostname, "canon") ||
            str_contains_ci(h->hostname, "brother") ||
            str_contains_ci(h->hostname, "xerox")) {
            h->type = NM_HOST_PRINTER;
            return;
        }
    }

    /* Check manufacturer string for printer brands */
    if (!nm_str_empty(h->manufacturer)) {
        if (str_contains_ci(h->manufacturer, "printer") ||
            str_contains_ci(h->manufacturer, "canon") ||
            str_contains_ci(h->manufacturer, "epson") ||
            str_contains_ci(h->manufacturer, "brother") ||
            str_contains_ci(h->manufacturer, "hewlett") ||
            str_contains_ci(h->manufacturer, "xerox")) {
            h->type = NM_HOST_PRINTER;
            return;
        }
    }

    /* Check services for printer ports */
    if (has_service_port(h, 631) ||   /* IPP */
        has_service_port(h, 9100) ||  /* RAW printing */
        has_service_port(h, 515)) {   /* LPD */
        h->type = NM_HOST_PRINTER;
        return;
    }

    /* Check manufacturer for IoT vendors */
    if (!nm_str_empty(h->manufacturer)) {
        if (str_contains_ci(h->manufacturer, "espressif") ||
            str_contains_ci(h->manufacturer, "shelly") ||
            str_contains_ci(h->manufacturer, "tuya") ||
            str_contains_ci(h->manufacturer, "tasmota") ||
            str_contains_ci(h->manufacturer, "sonoff")) {
            h->type = NM_HOST_IOT;
            return;
        }
    }

    /* Check services for IoT indicators */
    if (has_service_port(h, 5353) ||   /* mDNS */
        has_service_port(h, 1883) ||   /* MQTT */
        has_service_port(h, 8883)) {   /* MQTT over TLS */
        h->type = NM_HOST_IOT;
        return;
    }
    /* High-numbered ephemeral ports suggest IoT/UPnP */
    for (int i = 0; i < h->service_count; i++) {
        if (h->services[i].port >= 49152) {
            h->type = NM_HOST_IOT;
            return;
        }
    }

    /* Check services for server ports */
    if (has_service_port(h, 80) ||    /* HTTP */
        has_service_port(h, 443) ||   /* HTTPS */
        has_service_port(h, 22) ||    /* SSH */
        has_service_port(h, 53) ||    /* DNS */
        has_service_port(h, 25)) {    /* SMTP */
        h->type = NM_HOST_SERVER;
        return;
    }

    /* Default: if has any services, it's a server; otherwise workstation */
    if (h->service_count > 0) {
        h->type = NM_HOST_SERVER;
    } else {
        h->type = NM_HOST_WORKSTATION;
    }
}

/* Check if a name looks like an OS interface name (en0, utun3, bridge100, etc.) */
static int is_iface_name(const char *s)
{
    if (!s || !*s) return 0;
    /* Interface names: short alphanumeric, typically letters then digits */
    size_t len = strlen(s);
    if (len > 16) return 0;
    /* Known prefixes */
    static const char *prefixes[] = {
        "en", "eth", "wlan", "utun", "lo", "bridge", "feth",
        "awdl", "llw", "ap", "ipsec", "gif", "stf", "XHC",
        "anpi", "vmnet", "veth", "docker", "br-", NULL
    };
    for (const char **p = prefixes; *p; p++) {
        size_t plen = strlen(*p);
        if (len >= plen && strncmp(s, *p, plen) == 0)
            return 1;
    }
    return 0;
}

void nm_host_compute_display_name(nm_host_t *h)
{
    /* Priority: dns_name > hostname (if not iface) > mdns > IPv4 > IPv6 */
    if (!nm_str_empty(h->dns_name)) {
        nm_strlcpy(h->display_name, h->dns_name, sizeof(h->display_name));
    } else if (!nm_str_empty(h->hostname) && !is_iface_name(h->hostname)) {
        nm_strlcpy(h->display_name, h->hostname, sizeof(h->display_name));
    } else if (!nm_str_empty(h->mdns_name)) {
        nm_strlcpy(h->display_name, h->mdns_name, sizeof(h->display_name));
        nm_str_unescape_mdns(h->display_name);
    } else if (h->has_ipv4) {
        inet_ntop(AF_INET, &h->ipv4, h->display_name,
                  sizeof(h->display_name));
    } else if (h->has_ipv6) {
        inet_ntop(AF_INET6, &h->ipv6, h->display_name,
                  sizeof(h->display_name));
    } else {
        nm_strlcpy(h->display_name, "(unknown)", sizeof(h->display_name));
    }
}

const char *nm_host_ipv4_str(const nm_host_t *h)
{
    static char buf[NM_IPV4_STR_LEN];
    if (!h->has_ipv4) return "(none)";
    inet_ntop(AF_INET, &h->ipv4, buf, sizeof(buf));
    return buf;
}

const char *nm_host_type_str(nm_host_type_t type)
{
    switch (type) {
        case NM_HOST_LOCAL:       return "local";
        case NM_HOST_GATEWAY:     return "gateway";
        case NM_HOST_SERVER:      return "server";
        case NM_HOST_WORKSTATION: return "workstation";
        case NM_HOST_PRINTER:     return "printer";
        case NM_HOST_IOT:         return "iot";
        case NM_HOST_BOUNDARY:    return "boundary";
        case NM_HOST_SWITCH:      return "switch";
        case NM_HOST_AP:          return "ap";
    }
    return "unknown";
}

nm_host_type_t nm_host_type_from_str(const char *str)
{
    if (!str) return NM_HOST_WORKSTATION;
    if (strcmp(str, "local") == 0)       return NM_HOST_LOCAL;
    if (strcmp(str, "gateway") == 0)     return NM_HOST_GATEWAY;
    if (strcmp(str, "server") == 0)      return NM_HOST_SERVER;
    if (strcmp(str, "workstation") == 0) return NM_HOST_WORKSTATION;
    if (strcmp(str, "printer") == 0)     return NM_HOST_PRINTER;
    if (strcmp(str, "iot") == 0)         return NM_HOST_IOT;
    if (strcmp(str, "boundary") == 0)    return NM_HOST_BOUNDARY;
    if (strcmp(str, "switch") == 0)     return NM_HOST_SWITCH;
    if (strcmp(str, "ap") == 0)         return NM_HOST_AP;
    return NM_HOST_WORKSTATION;
}
