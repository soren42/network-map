#include "core/edge.h"

void nm_edge_init(nm_edge_t *e, int src, int dst, double weight,
                  nm_edge_type_t type)
{
    e->src_id = src;
    e->dst_id = dst;
    e->weight = weight;
    e->type   = type;
    e->in_mst = 0;
}

const char *nm_edge_type_str(nm_edge_type_t type)
{
    switch (type) {
        case NM_EDGE_LAN:     return "lan";
        case NM_EDGE_ROUTE:   return "route";
        case NM_EDGE_GATEWAY: return "gateway";
    }
    return "unknown";
}
