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

    if (depth[node] == 0) {
        /* Root node */
        printf("%s (%s) [%s]\n", name, ipstr, type);
    } else {
        printf("%s%s%s (%s) [%s]\n",
               prefix, is_last ? "\\-- " : "|-- ",
               name, ipstr, type);
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

        /* Find local host as BFS root */
        int root = 0;
        for (int i = 0; i < g->host_count; i++) {
            if (g->hosts[i].type == NM_HOST_LOCAL) {
                root = i;
                break;
            }
        }

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
