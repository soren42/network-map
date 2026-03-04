#include "core/graph.h"
#include "util/alloc.h"
#include "log.h"
#include <string.h>
#include <stdlib.h>

#define INIT_HOST_CAP 32
#define INIT_EDGE_CAP 64

nm_graph_t *nm_graph_create(void)
{
    nm_graph_t *g = nm_calloc(1, sizeof(*g));
    g->host_cap = INIT_HOST_CAP;
    g->hosts = nm_calloc(g->host_cap, sizeof(nm_host_t));
    g->edge_cap = INIT_EDGE_CAP;
    g->edges = nm_calloc(g->edge_cap, sizeof(nm_edge_t));
    g->adj_cap = INIT_HOST_CAP;
    g->adj = nm_calloc(g->adj_cap, sizeof(nm_adj_node_t *));
    return g;
}

void nm_graph_destroy(nm_graph_t *g)
{
    if (!g) return;
    /* Free adjacency lists */
    for (int i = 0; i < g->adj_cap; i++) {
        nm_adj_node_t *n = g->adj[i];
        while (n) {
            nm_adj_node_t *next = n->next;
            nm_free(n);
            n = next;
        }
    }
    nm_free(g->adj);
    nm_free(g->hosts);
    nm_free(g->edges);
    nm_free(g);
}

static void ensure_adj_cap(nm_graph_t *g, int needed)
{
    if (needed <= g->adj_cap) return;
    int new_cap = g->adj_cap;
    while (new_cap < needed) new_cap *= 2;
    g->adj = nm_realloc(g->adj, (size_t)new_cap * sizeof(nm_adj_node_t *));
    memset(g->adj + g->adj_cap, 0,
           (size_t)(new_cap - g->adj_cap) * sizeof(nm_adj_node_t *));
    g->adj_cap = new_cap;
}

int nm_graph_add_host(nm_graph_t *g, const nm_host_t *h)
{
    if (g->host_count >= g->host_cap) {
        g->host_cap *= 2;
        g->hosts = nm_realloc(g->hosts,
                              (size_t)g->host_cap * sizeof(nm_host_t));
    }
    int id = g->host_count;
    g->hosts[id] = *h;
    g->hosts[id].id = id;
    g->host_count++;
    ensure_adj_cap(g, g->host_count);
    return id;
}

int nm_graph_find_by_ipv4(const nm_graph_t *g, struct in_addr addr)
{
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].has_ipv4 &&
            g->hosts[i].ipv4.s_addr == addr.s_addr) {
            return i;
        }
    }
    return -1;
}

int nm_graph_find_by_ipv6(const nm_graph_t *g, const struct in6_addr *addr)
{
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].has_ipv6 &&
            memcmp(&g->hosts[i].ipv6, addr, sizeof(struct in6_addr)) == 0) {
            return i;
        }
    }
    return -1;
}

int nm_graph_find_by_mac(const nm_graph_t *g, const unsigned char *mac)
{
    /* Skip zero MACs */
    int zero = 1;
    for (int j = 0; j < NM_MAC_LEN; j++)
        if (mac[j] != 0) { zero = 0; break; }
    if (zero) return -1;

    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].has_mac &&
            memcmp(g->hosts[i].mac, mac, NM_MAC_LEN) == 0) {
            return i;
        }
    }
    return -1;
}

int nm_graph_find_by_iface(const nm_graph_t *g, const char *iface)
{
    if (!iface || iface[0] == '\0') return -1;
    for (int i = 0; i < g->host_count; i++) {
        if (strcmp(g->hosts[i].iface_name, iface) == 0)
            return i;
    }
    return -1;
}

static void add_adj(nm_graph_t *g, int host_id, int edge_idx)
{
    nm_adj_node_t *n = nm_malloc(sizeof(*n));
    n->edge_idx = edge_idx;
    n->next = g->adj[host_id];
    g->adj[host_id] = n;
}

int nm_graph_add_edge(nm_graph_t *g, int src_id, int dst_id,
                      double weight, nm_edge_type_t type)
{
    if (src_id < 0 || src_id >= g->host_count ||
        dst_id < 0 || dst_id >= g->host_count) {
        LOG_ERROR("add_edge: invalid host ids %d, %d", src_id, dst_id);
        return -1;
    }
    if (g->edge_count >= g->edge_cap) {
        g->edge_cap *= 2;
        g->edges = nm_realloc(g->edges,
                              (size_t)g->edge_cap * sizeof(nm_edge_t));
    }
    int idx = g->edge_count;
    nm_edge_init(&g->edges[idx], src_id, dst_id, weight, type);
    g->edge_count++;

    add_adj(g, src_id, idx);
    add_adj(g, dst_id, idx);
    return idx;
}

int nm_graph_has_edge(const nm_graph_t *g, int src_id, int dst_id)
{
    if (src_id < 0 || src_id >= g->host_count) return 0;
    nm_adj_node_t *n = g->adj[src_id];
    while (n) {
        nm_edge_t *e = &g->edges[n->edge_idx];
        if ((e->src_id == src_id && e->dst_id == dst_id) ||
            (e->src_id == dst_id && e->dst_id == src_id)) {
            return 1;
        }
        n = n->next;
    }
    return 0;
}

/* --- Union-Find for Kruskal MST --- */

typedef struct {
    int *parent;
    int *rank;
    int  size;
} uf_t;

static uf_t uf_create(int n)
{
    uf_t uf;
    uf.size = n;
    uf.parent = nm_malloc((size_t)n * sizeof(int));
    uf.rank   = nm_calloc((size_t)n, sizeof(int));
    for (int i = 0; i < n; i++) uf.parent[i] = i;
    return uf;
}

static void uf_destroy(uf_t *uf)
{
    nm_free(uf->parent);
    nm_free(uf->rank);
}

static int uf_find(uf_t *uf, int x)
{
    while (uf->parent[x] != x) {
        uf->parent[x] = uf->parent[uf->parent[x]]; /* path compression */
        x = uf->parent[x];
    }
    return x;
}

static int uf_union(uf_t *uf, int a, int b)
{
    int ra = uf_find(uf, a);
    int rb = uf_find(uf, b);
    if (ra == rb) return 0;
    if (uf->rank[ra] < uf->rank[rb]) {
        uf->parent[ra] = rb;
    } else if (uf->rank[ra] > uf->rank[rb]) {
        uf->parent[rb] = ra;
    } else {
        uf->parent[rb] = ra;
        uf->rank[ra]++;
    }
    return 1;
}

/* We need a way to sort edge indices by weight. Use a simple approach. */
static nm_graph_t *g_sort_graph; /* temp for qsort comparator */

/* Effective weight for MST: bias toward L2 physical links */
static double mst_effective_weight(const nm_edge_t *e)
{
    double w = e->weight;
    switch (e->type) {
    case NM_EDGE_L2:   return w * 0.1;   /* strongly prefer wired L2 */
    case NM_EDGE_WIFI: return w * 0.5;   /* prefer WiFi over IP-inferred */
    default:           return w;
    }
}

static int edge_idx_cmp(const void *a, const void *b)
{
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    double wa = mst_effective_weight(&g_sort_graph->edges[ia]);
    double wb = mst_effective_weight(&g_sort_graph->edges[ib]);
    if (wa < wb) return -1;
    if (wa > wb) return  1;
    return 0;
}

int nm_graph_kruskal_mst(nm_graph_t *g)
{
    if (g->edge_count == 0 || g->host_count == 0) return 0;

    /* Clear all in_mst flags */
    for (int i = 0; i < g->edge_count; i++) {
        g->edges[i].in_mst = 0;
    }

    /* Create sorted index array */
    int *sorted = nm_malloc((size_t)g->edge_count * sizeof(int));
    for (int i = 0; i < g->edge_count; i++) sorted[i] = i;

    g_sort_graph = g;
    qsort(sorted, (size_t)g->edge_count, sizeof(int), edge_idx_cmp);

    uf_t uf = uf_create(g->host_count);
    int mst_count = 0;

    for (int i = 0; i < g->edge_count && mst_count < g->host_count - 1; i++) {
        int ei = sorted[i];
        nm_edge_t *e = &g->edges[ei];
        if (uf_union(&uf, e->src_id, e->dst_id)) {
            e->in_mst = 1;
            mst_count++;
        }
    }

    uf_destroy(&uf);
    nm_free(sorted);

    LOG_DEBUG("MST: %d edges from %d total", mst_count, g->edge_count);
    return mst_count;
}

int nm_graph_bfs_mst(const nm_graph_t *g, int src_id,
                     int *parent, int *depth)
{
    if (src_id < 0 || src_id >= g->host_count) return -1;

    for (int i = 0; i < g->host_count; i++) {
        parent[i] = -1;
        depth[i]  = -1;
    }

    /* Simple BFS using a queue (array-based) */
    int *queue = nm_malloc((size_t)g->host_count * sizeof(int));
    int qhead = 0, qtail = 0;

    depth[src_id] = 0;
    parent[src_id] = src_id;
    queue[qtail++] = src_id;

    while (qhead < qtail) {
        int cur = queue[qhead++];
        nm_adj_node_t *n = g->adj[cur];
        while (n) {
            nm_edge_t *e = &g->edges[n->edge_idx];
            if (!e->in_mst) { n = n->next; continue; }
            int neighbor = (e->src_id == cur) ? e->dst_id : e->src_id;
            if (depth[neighbor] == -1) {
                depth[neighbor]  = depth[cur] + 1;
                parent[neighbor] = cur;
                queue[qtail++] = neighbor;
            }
            n = n->next;
        }
    }

    nm_free(queue);
    return 0;
}
