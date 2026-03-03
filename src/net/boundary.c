#include "net/boundary.h"
#include "net/icmp.h"
#include "log.h"
#include "util/alloc.h"
#include "util/strutil.h"

#include <arpa/inet.h>
#include <string.h>

int nm_is_private_ip(struct in_addr addr)
{
    uint32_t ip = ntohl(addr.s_addr);

    /* 10.0.0.0/8 */
    if ((ip & 0xFF000000) == 0x0A000000) return 1;

    /* 172.16.0.0/12 */
    if ((ip & 0xFFF00000) == 0xAC100000) return 1;

    /* 192.168.0.0/16 */
    if ((ip & 0xFFFF0000) == 0xC0A80000) return 1;

    /* 100.64.0.0/10 (RFC 6598 CGN) */
    if ((ip & 0xFFC00000) == 0x64400000) return 1;

    /* 169.254.0.0/16 (RFC 3927 link-local) */
    if ((ip & 0xFFFF0000) == 0xA9FE0000) return 1;

    return 0;
}

int nm_boundary_detect(nm_graph_t *g)
{
    /* Find a gateway to traceroute outward from */
    int gw_id = -1;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == NM_HOST_GATEWAY && g->hosts[i].has_ipv4) {
            gw_id = i;
            break;
        }
    }
    if (gw_id < 0) {
        LOG_DEBUG("boundary: no gateway found");
        return -1;
    }

    /* Pick a well-known public IP to traceroute toward */
    struct in_addr target;
    inet_pton(AF_INET, "8.8.8.8", &target);

    int last_private_id = gw_id;
    int prev_id = gw_id;

    for (int ttl = 2; ttl <= NM_MAX_HOPS; ttl++) {
        struct in_addr reply_addr;
        double rtt = 0;
        int rc = nm_icmp_probe(target, ttl, 1500, &reply_addr, &rtt);
        if (rc < 0) continue;

        if (nm_is_private_ip(reply_addr)) {
            /* Still in private space - add/find this hop */
            int hop_id = nm_graph_find_by_ipv4(g, reply_addr);
            if (hop_id < 0) {
                nm_host_t h;
                nm_host_init(&h);
                nm_host_set_ipv4_addr(&h, reply_addr);
                h.type = NM_HOST_GATEWAY;
                h.hop_distance = ttl;
                h.rtt_ms = rtt;
                hop_id = nm_graph_add_host(g, &h);
            }
            if (prev_id >= 0 && !nm_graph_has_edge(g, prev_id, hop_id)) {
                nm_graph_add_edge(g, prev_id, hop_id,
                                  rtt > 0 ? rtt : 1.0, NM_EDGE_ROUTE);
            }
            last_private_id = hop_id;
            prev_id = hop_id;
        } else {
            /* Hit public IP - boundary is the last private hop */
            break;
        }

        /* If we got echo reply (reached target), stop */
        if (rc == 0) break;
    }

    /* Mark boundary */
    if (last_private_id >= 0 && last_private_id != gw_id) {
        g->hosts[last_private_id].type = NM_HOST_BOUNDARY;
        g->hosts[last_private_id].is_boundary = 1;
        LOG_INFO("boundary: detected at host %d (%s)",
                 last_private_id, nm_host_ipv4_str(&g->hosts[last_private_id]));
        return last_private_id;
    }

    /* Gateway itself is the boundary */
    g->hosts[gw_id].is_boundary = 1;
    /* Don't change gateway type to boundary - keep it as gateway */
    LOG_INFO("boundary: gateway is the NAT boundary");
    return gw_id;
}

int nm_boundary_set(nm_graph_t *g, const char *host_str)
{
    struct in_addr addr;
    if (inet_pton(AF_INET, host_str, &addr) != 1) {
        LOG_ERROR("boundary: invalid IP '%s'", host_str);
        return -1;
    }

    int id = nm_graph_find_by_ipv4(g, addr);
    if (id < 0) {
        nm_host_t h;
        nm_host_init(&h);
        nm_host_set_ipv4_addr(&h, addr);
        h.type = NM_HOST_BOUNDARY;
        h.is_boundary = 1;
        id = nm_graph_add_host(g, &h);
    } else {
        g->hosts[id].type = NM_HOST_BOUNDARY;
        g->hosts[id].is_boundary = 1;
    }

    LOG_INFO("boundary: manually set to %s (host %d)", host_str, id);
    return id;
}
