#include "net/ping.h"
#include "net/icmp.h"
#include "log.h"

#include <arpa/inet.h>

int nm_ping_sweep(nm_graph_t *g, struct in_addr base_addr,
                  int prefix_len, int timeout_ms)
{
    if (prefix_len < 24 || prefix_len > 30) {
        LOG_WARN("Ping sweep only supports /24-/30, got /%d", prefix_len);
        return 0;
    }

    uint32_t mask = 0xFFFFFFFFU << (32 - prefix_len);
    uint32_t net = ntohl(base_addr.s_addr) & mask;
    uint32_t host_count = ~mask; /* number of host addresses */

    /* Find first local host for edges */
    int local_id = -1;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == NM_HOST_LOCAL) {
            local_id = i;
            break;
        }
    }

    int found = 0;
    for (uint32_t h = 1; h < host_count; h++) {
        struct in_addr target;
        target.s_addr = htonl(net | h);

        /* Skip if already in graph */
        if (nm_graph_find_by_ipv4(g, target) >= 0) continue;

        double rtt = nm_icmp_ping(target, timeout_ms);
        if (rtt < 0) continue;

        nm_host_t host;
        nm_host_init(&host);
        nm_host_set_ipv4_addr(&host, target);
        host.type = NM_HOST_WORKSTATION;
        host.rtt_ms = rtt;
        int id = nm_graph_add_host(g, &host);

        if (local_id >= 0)
            nm_graph_add_edge(g, local_id, id, rtt > 0 ? rtt : 1.0,
                              NM_EDGE_LAN);
        found++;
    }

    return found;
}
