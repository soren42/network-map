#ifndef NM_GRAPH_H
#define NM_GRAPH_H

#include "core/host.h"
#include "core/edge.h"

typedef struct nm_adj_node {
    int                  edge_idx;
    struct nm_adj_node  *next;
} nm_adj_node_t;

typedef struct {
    nm_host_t       *hosts;
    int              host_count;
    int              host_cap;
    nm_edge_t       *edges;
    int              edge_count;
    int              edge_cap;
    nm_adj_node_t  **adj;
    int              adj_cap;
} nm_graph_t;

nm_graph_t *nm_graph_create(void);
void        nm_graph_destroy(nm_graph_t *g);
int nm_graph_add_host(nm_graph_t *g, const nm_host_t *h);
int nm_graph_find_by_ipv4(const nm_graph_t *g, struct in_addr addr);
int nm_graph_find_by_ipv6(const nm_graph_t *g, const struct in6_addr *addr);
int nm_graph_find_by_mac(const nm_graph_t *g, const unsigned char *mac);
int nm_graph_find_by_iface(const nm_graph_t *g, const char *iface);
int nm_graph_add_edge(nm_graph_t *g, int src_id, int dst_id,
                      double weight, nm_edge_type_t type);
int nm_graph_has_edge(const nm_graph_t *g, int src_id, int dst_id);
int nm_graph_kruskal_mst(nm_graph_t *g);
int nm_graph_bfs_mst(const nm_graph_t *g, int src_id,
                     int *parent, int *depth);

#endif /* NM_GRAPH_H */
