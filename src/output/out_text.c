#include "output/out_text.h"
#include "util/alloc.h"
#include "util/strutil.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/* Format services as "port/name,port/name,..." into buf */
static void format_services(const nm_host_t *h, char *buf, size_t buflen)
{
    buf[0] = '\0';
    size_t off = 0;
    for (int i = 0; i < h->service_count; i++) {
        const nm_service_t *s = &h->services[i];
        int n;
        if (i > 0) {
            if (off + 1 >= buflen) break;
            buf[off++] = ',';
            buf[off] = '\0';
        }
        if (!nm_str_empty(s->name)) {
            n = snprintf(buf + off, buflen - off, "%d/%s", s->port, s->name);
        } else {
            n = snprintf(buf + off, buflen - off, "%d", s->port);
        }
        if (n < 0 || (size_t)n >= buflen - off) break;
        off += (size_t)n;
    }
}

static void print_divider(void)
{
    for (int i = 0; i < 188; i++) putchar('-');
    putchar('\n');
}

/* Find the edge connecting parent_id to child_id */
static const nm_edge_t *find_edge(const nm_graph_t *g, int parent_id, int child_id)
{
    for (int i = 0; i < g->edge_count; i++) {
        const nm_edge_t *e = &g->edges[i];
        if (!e->in_mst) continue;
        if ((e->src_id == parent_id && e->dst_id == child_id) ||
            (e->src_id == child_id && e->dst_id == parent_id))
            return e;
    }
    return NULL;
}

/* Format edge annotation (port number, medium) */
static void format_edge_prefix(const nm_edge_t *e, int parent_id,
                                char *buf, size_t buflen)
{
    buf[0] = '\0';
    if (!e) return;

    if (e->type == NM_EDGE_WIFI) {
        snprintf(buf, buflen, "~wifi~ ");
        return;
    }

    /* Show source port (port on the parent/switch side) */
    int port = 0;
    if (e->src_id == parent_id)
        port = e->src_port_num;
    else
        port = e->dst_port_num;

    if (port > 0)
        snprintf(buf, buflen, "Port %d: ", port);
}

/* Format additional host detail (WiFi signal, medium) */
static void format_host_detail(const nm_host_t *h, const nm_edge_t *e,
                                char *buf, size_t buflen)
{
    buf[0] = '\0';
    if (e && e->type == NM_EDGE_WIFI && h->wifi_signal != 0) {
        snprintf(buf, buflen, " (%d dBm)", h->wifi_signal);
        return;
    }
    if (e && e->medium == NM_MEDIUM_MOCA) {
        snprintf(buf, buflen, " [moca]");
        return;
    }
    /* Show medium tag for wired connections */
    if (h->connection_medium == NM_MEDIUM_WIRED &&
        (h->type != NM_HOST_SWITCH && h->type != NM_HOST_AP &&
         h->type != NM_HOST_GATEWAY)) {
        snprintf(buf, buflen, " [wired]");
    }
}

/* Print BFS tree recursively */
static void print_tree(const nm_graph_t *g, const int *parent,
                       const int *depth, int node,
                       const char *prefix, int is_last)
{
    const nm_host_t *h = &g->hosts[node];
    char ipstr[NM_IPV4_STR_LEN] = "-";
    if (h->has_ipv4) {
        inet_ntop(AF_INET, &h->ipv4, ipstr, sizeof(ipstr));
    }

    const char *name = h->display_name;
    if (nm_str_empty(name)) name = ipstr;

    const char *type = nm_host_type_str(h->type);

    /* Edge annotation */
    const nm_edge_t *e = (parent[node] >= 0 && parent[node] != node) ?
                         find_edge(g, parent[node], node) : NULL;
    char edge_prefix[64] = "";
    char host_detail[64] = "";
    format_edge_prefix(e, parent[node], edge_prefix, sizeof(edge_prefix));
    format_host_detail(h, e, host_detail, sizeof(host_detail));

    if (depth[node] == 0) {
        /* Root node */
        printf("%s (%s) [%s]\n", name, ipstr, type);
    } else {
        printf("%s%s%s%s (%s) [%s]%s\n",
               prefix, is_last ? "\\-- " : "|-- ",
               edge_prefix, name, ipstr, type, host_detail);
    }

    /* Collect children of this node */
    int child_count = 0;
    int *children = nm_malloc((size_t)g->host_count * sizeof(int));
    for (int i = 0; i < g->host_count; i++) {
        if (parent[i] == node && i != node) {
            children[child_count++] = i;
        }
    }

    /* Print each child */
    for (int c = 0; c < child_count; c++) {
        char new_prefix[512];
        if (depth[node] == 0) {
            snprintf(new_prefix, sizeof(new_prefix), "%s", "");
        } else {
            snprintf(new_prefix, sizeof(new_prefix), "%s%s",
                     prefix, is_last ? "    " : "|   ");
        }
        print_tree(g, parent, depth, children[c],
                   new_prefix, c == child_count - 1);
    }

    nm_free(children);
}

int nm_out_text(const nm_graph_t *g)
{
    if (!g) return -1;

    /* Header */
    printf("Network Map - %d hosts, %d edges\n", g->host_count, g->edge_count);
    print_divider();

    /* Column headers */
    printf("%-4s %-40s %-16s %-18s %-12s %-24s %-24s %s\n",
           "ID", "Name", "IP", "MAC", "Type", "OS",
           "Manufacturer", "Services");
    print_divider();

    /* Host rows */
    for (int i = 0; i < g->host_count; i++) {
        const nm_host_t *h = &g->hosts[i];

        /* IP address */
        char ipstr[NM_IPV4_STR_LEN] = "-";
        if (h->has_ipv4) {
            inet_ntop(AF_INET, &h->ipv4, ipstr, sizeof(ipstr));
        }

        /* MAC address */
        char macstr[NM_MAC_STR_LEN] = "-";
        if (h->has_mac) {
            nm_mac_to_str(h->mac, macstr, sizeof(macstr));
        }

        /* Display name */
        const char *name = h->display_name;
        if (nm_str_empty(name)) name = "-";

        /* Type */
        const char *type = nm_host_type_str(h->type);

        /* OS */
        const char *os = h->os_name;
        if (nm_str_empty(os)) os = "-";

        /* Manufacturer */
        const char *mfr = h->manufacturer;
        if (nm_str_empty(mfr)) mfr = "-";

        /* Services */
        char svcbuf[256];
        format_services(h, svcbuf, sizeof(svcbuf));
        if (svcbuf[0] == '\0') {
            nm_strlcpy(svcbuf, "-", sizeof(svcbuf));
        }

        printf("%-4d %-40.40s %-16s %-18s %-12s %-24.24s %-24.24s %s\n",
               h->id, name, ipstr, macstr, type, os, mfr, svcbuf);
    }

    /* MST BFS tree */
    if (g->host_count > 0) {
        printf("\n");

        /* Build MST - need a mutable copy of the graph pointer for kruskal */
        nm_graph_kruskal_mst((nm_graph_t *)g);

        /* Find BFS root: prefer gateway, then local */
        int root = 0;
        int found_local = -1;
        for (int i = 0; i < g->host_count; i++) {
            if (g->hosts[i].type == NM_HOST_GATEWAY) {
                root = i;
                found_local = -1; /* gateway wins */
                break;
            }
            if (g->hosts[i].type == NM_HOST_LOCAL && found_local < 0)
                found_local = i;
        }
        if (found_local >= 0 && root == 0 && g->hosts[0].type != NM_HOST_GATEWAY)
            root = found_local;

        int *parent = nm_malloc((size_t)g->host_count * sizeof(int));
        int *depth  = nm_malloc((size_t)g->host_count * sizeof(int));

        nm_graph_bfs_mst(g, root, parent, depth);

        printf("MST BFS Tree:\n");
        print_tree(g, parent, depth, root, "", 1);

        /* Print any disconnected hosts not reached by BFS */
        for (int i = 0; i < g->host_count; i++) {
            if (depth[i] == -1) {
                const nm_host_t *h = &g->hosts[i];
                char ipstr[NM_IPV4_STR_LEN] = "-";
                if (h->has_ipv4) {
                    inet_ntop(AF_INET, &h->ipv4, ipstr, sizeof(ipstr));
                }
                const char *name = h->display_name;
                if (nm_str_empty(name)) name = ipstr;
                printf("(disconnected) %s (%s) [%s]\n",
                       name, ipstr, nm_host_type_str(h->type));
            }
        }

        nm_free(parent);
        nm_free(depth);
    }

    return 0;
}
