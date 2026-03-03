#ifndef NM_ROUTE_H
#define NM_ROUTE_H

#include "core/graph.h"

/* Read the system routing table. Adds gateway hosts and edges.
   Returns number of routes found. */
int nm_route_read(nm_graph_t *g);

#endif /* NM_ROUTE_H */
