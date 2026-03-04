#include "core/edge.h"
#include <string.h>

void nm_edge_init(nm_edge_t *e, int src, int dst, double weight,
                  nm_edge_type_t type)
{
    memset(e, 0, sizeof(*e));
    e->src_id = src;
    e->dst_id = dst;
    e->weight = weight;
    e->type   = type;
}

const char *nm_edge_type_str(nm_edge_type_t type)
{
    switch (type) {
        case NM_EDGE_LAN:     return "lan";
        case NM_EDGE_ROUTE:   return "route";
        case NM_EDGE_GATEWAY: return "gateway";
        case NM_EDGE_L2:      return "l2";
        case NM_EDGE_WIFI:    return "wifi";
    }
    return "unknown";
}

nm_edge_type_t nm_edge_type_from_str(const char *str)
{
    if (!str) return NM_EDGE_LAN;
    if (strcmp(str, "lan") == 0)     return NM_EDGE_LAN;
    if (strcmp(str, "route") == 0)   return NM_EDGE_ROUTE;
    if (strcmp(str, "gateway") == 0) return NM_EDGE_GATEWAY;
    if (strcmp(str, "l2") == 0)      return NM_EDGE_L2;
    if (strcmp(str, "wifi") == 0)    return NM_EDGE_WIFI;
    return NM_EDGE_LAN;
}
