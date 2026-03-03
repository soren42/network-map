#include "core/scan.h"
#include "net/iface.h"
#include "net/nm_route.h"
#include "net/arp.h"
#include "net/dns.h"
#include "net/ping.h"
#include "net/icmp.h"
#include "net/icmp6.h"
#include "net/mdns.h"
#include "net/boundary.h"
#include "net/nmap.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <arpa/inet.h>

/* ---------- progress callback ------------------------------------------ */

static nm_progress_fn g_progress_fn;
static void          *g_progress_ctx;

void nm_scan_set_progress(nm_progress_fn fn, void *ctx)
{
    g_progress_fn  = fn;
    g_progress_ctx = ctx;
}

static void scan_progress(const char *fmt, ...)
{
    if (!g_progress_fn) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    g_progress_fn(buf, g_progress_ctx);
}

/* ----------------------------------------------------------------------- */

int nm_scan_local_interfaces(nm_graph_t *g, const nm_config_t *cfg)
{
    (void)cfg;
    LOG_INFO("Phase 1: Local host identification");
    scan_progress("Enumerating local interfaces...");
    int n = nm_iface_enumerate(g);
    if (n < 0) {
        LOG_ERROR("Failed to enumerate network interfaces");
        return -1;
    }
    LOG_INFO("Phase 1: discovered %d local interface(s)", n);
    scan_progress("Found %d local interface(s)", n);
    return n;
}

int nm_scan_routing_table(nm_graph_t *g, const nm_config_t *cfg)
{
    (void)cfg;
    LOG_INFO("Phase 2: Routing table analysis");
    scan_progress("Reading routing table...");
    int n = nm_route_read(g);
    if (n < 0) {
        LOG_WARN("Failed to read routing table");
        return -1;
    }
    LOG_INFO("Phase 2: discovered %d route(s)", n);
    scan_progress("Found %d route(s)", n);
    return n;
}

int nm_scan_lan_discovery(nm_graph_t *g, const nm_config_t *cfg)
{
    LOG_INFO("Phase 3: LAN discovery");
    scan_progress("LAN discovery...");

    if (!cfg->no_arp) {
        int n = nm_arp_read(g);
        if (n < 0) {
            LOG_WARN("Failed to read ARP cache");
        } else {
            LOG_INFO("Phase 3: %d ARP cache entries", n);
            scan_progress("ARP cache: %d entries", n);
        }
    }

    /* Always do subnet ping sweep */
    LOG_INFO("Phase 3: active subnet scan");
    scan_progress("Subnet ping sweep (this may take a while)...");
    for (int i = 0; i < g->host_count; i++) {
        nm_host_t *h = &g->hosts[i];
        if (h->type != NM_HOST_LOCAL) continue;
        /* Scan primary IPv4 subnet */
        if (h->has_ipv4) {
            struct in_addr base;
            base.s_addr = h->ipv4.s_addr & htonl(0xFFFFFF00);
            int n = nm_ping_sweep(g, base, 24, NM_PING_TIMEOUT_MS);
            if (n > 0) {
                LOG_INFO("Subnet scan: %d hosts on %s", n, h->iface_name);
                scan_progress("Subnet scan: %d hosts on %s", n, h->iface_name);
            }
        }
        /* Scan secondary IPv4 subnets */
        for (int j = 0; j < h->ipv4_count; j++) {
            struct in_addr base;
            base.s_addr = h->ipv4_addrs[j].s_addr & htonl(0xFFFFFF00);
            int n = nm_ping_sweep(g, base, 24, NM_PING_TIMEOUT_MS);
            if (n > 0) {
                LOG_INFO("Subnet scan: %d hosts", n);
                scan_progress("Subnet scan: %d more hosts", n);
            }
        }
    }

    LOG_INFO("Phase 3 complete");
    return 0;
}

int nm_scan_name_resolution(nm_graph_t *g, const nm_config_t *cfg)
{
    LOG_INFO("Phase 4: Name resolution");
    scan_progress("Resolving hostnames...");

    int n = nm_dns_resolve_all(g, cfg->nameserver);
    if (n < 0) {
        LOG_WARN("DNS resolution errors");
    } else {
        LOG_INFO("Phase 4: resolved %d hostname(s)", n);
        scan_progress("Resolved %d hostname(s)", n);
    }

    if (!cfg->no_mdns) {
        LOG_INFO("Phase 4: mDNS browse (3s timeout)");
        scan_progress("mDNS discovery (3s)...");
        int m = nm_mdns_browse(g, 3000);
        if (m < 0) {
            LOG_WARN("mDNS discovery failed");
        } else {
            LOG_INFO("Phase 4: %d mDNS name(s)", m);
            scan_progress("mDNS: %d name(s)", m);
        }
    }

    LOG_INFO("Phase 4 complete");
    return 0;
}

int nm_scan_boundary_detect(nm_graph_t *g, const nm_config_t *cfg)
{
    LOG_INFO("Phase 5: NAT boundary detection");
    scan_progress("Detecting NAT boundary...");

    int bid;
    if (cfg->has_boundary) {
        bid = nm_boundary_set(g, cfg->boundary_host);
        if (bid < 0) {
            LOG_WARN("Failed to set boundary host: %s", cfg->boundary_host);
            scan_progress("Boundary host not found: %s", cfg->boundary_host);
            return -1;
        }
        LOG_INFO("Phase 5: boundary set to host %d (%s)", bid,
                 nm_host_ipv4_str(&g->hosts[bid]));
        scan_progress("Boundary: %s", nm_host_ipv4_str(&g->hosts[bid]));
    } else {
        bid = nm_boundary_detect(g);
        if (bid < 0) {
            LOG_INFO("Phase 5: no NAT boundary detected");
            scan_progress("No NAT boundary detected");
            return 0;
        }
        LOG_INFO("Phase 5: detected boundary at host %d (%s)", bid,
                 nm_host_ipv4_str(&g->hosts[bid]));
        scan_progress("Boundary detected: %s",
                      nm_host_ipv4_str(&g->hosts[bid]));
    }

    LOG_INFO("Phase 5 complete");
    return bid;
}

int nm_scan_nmap_enrich(nm_graph_t *g, const nm_config_t *cfg)
{
    if (cfg->no_nmap || cfg->fast_mode) {
        LOG_INFO("Phase 6: skipped (nmap disabled)");
        scan_progress("nmap enrichment: skipped");
        return 0;
    }

    if (!nm_nmap_available()) {
        LOG_WARN("Phase 6: nmap not found in PATH");
        scan_progress("nmap not found - skipping enrichment");
        return 0;
    }

    LOG_INFO("Phase 6: nmap service/OS detection");
    scan_progress("Running nmap scan (this may take a while)...");
    int total = 0;

    for (int i = 0; i < g->host_count; i++) {
        nm_host_t *h = &g->hosts[i];
        if (h->type != NM_HOST_LOCAL || !h->has_ipv4) continue;

        uint32_t net = ntohl(h->ipv4.s_addr) & 0xFFFFFF00;
        char cidr[32];
        snprintf(cidr, sizeof(cidr), "%u.%u.%u.0/24",
                 (net >> 24) & 0xFF, (net >> 16) & 0xFF, (net >> 8) & 0xFF);

        scan_progress("nmap scanning %s...", cidr);
        int n = nm_nmap_scan_subnet(g, cidr);
        if (n > 0) {
            LOG_INFO("nmap: enriched %d hosts on %s", n, cidr);
            total += n;
        }
    }

    LOG_INFO("Phase 6: enriched %d host(s) total", total);
    scan_progress("nmap: enriched %d host(s)", total);
    return total;
}

int nm_scan_ipv6_augment(nm_graph_t *g, const nm_config_t *cfg)
{
    if (cfg->ipv4_only) {
        LOG_INFO("Phase 7: skipped (IPv4-only mode)");
        scan_progress("IPv6 discovery: skipped (IPv4-only)");
        return 0;
    }

    LOG_INFO("Phase 7: IPv6 neighbor discovery");
    scan_progress("IPv6 neighbor discovery...");
    int total = 0;

    for (int i = 0; i < g->host_count; i++) {
        nm_host_t *h = &g->hosts[i];
        if (h->type != NM_HOST_LOCAL) continue;
        if (nm_str_empty(h->interfaces)) continue;

        /* Parse comma-separated interfaces list */
        char buf[NM_IFACES_LEN];
        nm_strlcpy(buf, h->interfaces, sizeof(buf));
        char *saveptr = NULL;
        char *tok = strtok_r(buf, ",", &saveptr);
        while (tok) {
            int n = nm_icmp6_discover_neighbors(tok, g);
            if (n > 0) {
                LOG_INFO("IPv6: %d neighbor(s) on %s", n, tok);
                total += n;
            }
            tok = strtok_r(NULL, ",", &saveptr);
        }
    }

    LOG_INFO("Phase 7: %d total IPv6 neighbor(s)", total);
    scan_progress("IPv6: %d neighbor(s)", total);
    return total;
}

nm_graph_t *nm_scan_run(const nm_config_t *cfg)
{
    if (!cfg) {
        LOG_ERROR("NULL configuration");
        return NULL;
    }

    LOG_INFO("Starting network discovery");

    nm_graph_t *g = nm_graph_create();
    if (!g) {
        LOG_ERROR("Failed to create graph");
        return NULL;
    }

    if (nm_scan_local_interfaces(g, cfg) < 0) {
        LOG_ERROR("Phase 1 failed - cannot continue");
        nm_graph_destroy(g);
        return NULL;
    }

    nm_scan_routing_table(g, cfg);
    nm_scan_lan_discovery(g, cfg);
    nm_scan_name_resolution(g, cfg);
    nm_scan_boundary_detect(g, cfg);
    nm_scan_nmap_enrich(g, cfg);
    nm_scan_ipv6_augment(g, cfg);

    /* Classify hosts and compute display names */
    for (int i = 0; i < g->host_count; i++) {
        nm_host_classify(&g->hosts[i]);
        nm_host_compute_display_name(&g->hosts[i]);
    }

    LOG_INFO("Discovery complete: %d hosts, %d edges",
             g->host_count, g->edge_count);
    scan_progress("Discovery complete: %d hosts, %d edges",
                  g->host_count, g->edge_count);
    return g;
}
