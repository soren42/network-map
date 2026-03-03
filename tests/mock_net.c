#include "mock_net.h"
#include "core/host.h"
#include "core/edge.h"
#include "net/icmp.h"
#include "util/strutil.h"
#include <string.h>

/* ---------- stubs for platform ICMP (not linked in test build) ----------- */

double nm_icmp_ping(struct in_addr target, int timeout_ms)
{
    (void)target; (void)timeout_ms;
    return -1;  /* always timeout in tests */
}

int nm_icmp_probe(struct in_addr target, int ttl, int timeout_ms,
                  struct in_addr *reply_addr, double *rtt_ms)
{
    (void)target; (void)ttl; (void)timeout_ms;
    (void)reply_addr; (void)rtt_ms;
    return -1;  /* always timeout in tests */
}

unsigned short nm_icmp_checksum(const void *data, int len)
{
    (void)data; (void)len;
    return 0;
}

/* ---------- sample graph builder ----------------------------------------- */

nm_graph_t *mock_build_sample_graph(void)
{
    nm_graph_t *g = nm_graph_create();
    nm_host_t h;

    /* Host 0: LOCAL, 192.168.1.100, hostname "my-machine", MAC, iface "en0" */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.100");
    nm_strlcpy(h.hostname, "my-machine", sizeof(h.hostname));
    unsigned char mac0[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};
    nm_host_set_mac(&h, mac0);
    nm_strlcpy(h.iface_name, "en0", sizeof(h.iface_name));
    h.type = NM_HOST_LOCAL;
    nm_graph_add_host(g, &h);

    /* Host 1: GATEWAY, 192.168.1.1, hostname "router", MAC, hop_distance=1 */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.1");
    nm_strlcpy(h.hostname, "router", sizeof(h.hostname));
    unsigned char mac1[] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};
    nm_host_set_mac(&h, mac1);
    h.type = NM_HOST_GATEWAY;
    h.hop_distance = 1;
    nm_graph_add_host(g, &h);

    /* Host 2: PRINTER, 192.168.1.50, hostname "printer", service 631/tcp/ipp */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.50");
    nm_strlcpy(h.hostname, "printer", sizeof(h.hostname));
    h.type = NM_HOST_PRINTER;
    nm_host_add_service(&h, 631, "tcp", "ipp", NULL);
    nm_graph_add_host(g, &h);

    /* Host 3: WORKSTATION, 192.168.1.51, mdns "laptop._http._tcp.local" */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.51");
    nm_strlcpy(h.mdns_name, "laptop._http._tcp.local", sizeof(h.mdns_name));
    h.type = NM_HOST_WORKSTATION;
    nm_graph_add_host(g, &h);

    /* Host 4: SERVER, 192.168.1.52, services: 22/tcp/ssh, 80/tcp/http, os_name "Ubuntu 22.04" */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.52");
    h.type = NM_HOST_SERVER;
    nm_host_add_service(&h, 22, "tcp", "ssh", NULL);
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_strlcpy(h.os_name, "Ubuntu 22.04", sizeof(h.os_name));
    nm_graph_add_host(g, &h);

    /* Host 5: WORKSTATION, 192.168.1.53 */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.53");
    h.type = NM_HOST_WORKSTATION;
    nm_graph_add_host(g, &h);

    /* Host 6: IOT, 192.168.1.54, manufacturer "Espressif", service 1883/tcp/mqtt */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.54");
    h.type = NM_HOST_IOT;
    nm_strlcpy(h.manufacturer, "Espressif", sizeof(h.manufacturer));
    nm_host_add_service(&h, 1883, "tcp", "mqtt", NULL);
    nm_graph_add_host(g, &h);

    /* Host 7: BOUNDARY, 10.0.0.1, is_boundary=1, hop_distance=2 */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.1");
    h.type = NM_HOST_BOUNDARY;
    h.is_boundary = 1;
    h.hop_distance = 2;
    nm_graph_add_host(g, &h);

    /* Edges */
    nm_graph_add_edge(g, 0, 1, 1.5, NM_EDGE_GATEWAY);
    nm_graph_add_edge(g, 0, 2, 0.5, NM_EDGE_LAN);
    nm_graph_add_edge(g, 0, 3, 0.3, NM_EDGE_LAN);
    nm_graph_add_edge(g, 0, 4, 0.8, NM_EDGE_LAN);
    nm_graph_add_edge(g, 0, 5, 0.4, NM_EDGE_LAN);
    nm_graph_add_edge(g, 0, 6, 0.6, NM_EDGE_LAN);
    nm_graph_add_edge(g, 1, 7, 5.0, NM_EDGE_ROUTE);

    /* Compute display names for all hosts */
    for (int i = 0; i < g->host_count; i++) {
        nm_host_compute_display_name(&g->hosts[i]);
    }

    return g;
}
