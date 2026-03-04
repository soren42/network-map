#ifndef NM_EDGE_H
#define NM_EDGE_H

#include "core/types.h"

typedef enum {
    NM_EDGE_LAN     = 0,
    NM_EDGE_ROUTE   = 1,
    NM_EDGE_GATEWAY = 2,
    NM_EDGE_L2      = 3,
    NM_EDGE_WIFI    = 4
} nm_edge_type_t;

typedef struct {
    int             src_id;
    int             dst_id;
    double          weight;
    nm_edge_type_t  type;
    int             in_mst;
    /* L2 physical topology fields */
    int             src_port_num;
    int             dst_port_num;
    char            src_port_name[32];
    char            dst_port_name[32];
    int             speed_mbps;
    nm_medium_t     medium;
} nm_edge_t;

void nm_edge_init(nm_edge_t *e, int src, int dst, double weight,
                  nm_edge_type_t type);
const char *nm_edge_type_str(nm_edge_type_t type);
nm_edge_type_t nm_edge_type_from_str(const char *str);

#endif /* NM_EDGE_H */
