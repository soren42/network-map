#ifndef NM_LLDP_H
#define NM_LLDP_H

#include "core/graph.h"

/* Check if lldpcli is available at runtime */
int nm_lldp_available(void);

/* Discover LLDP neighbors and add them to the graph.
   Runs lldpcli -f json show neighbors, parses JSON output.
   Returns number of neighbors discovered, or -1 on error.
   No-op stub when HAVE_LLDP is not defined. */
int nm_lldp_discover(nm_graph_t *g);

#endif /* NM_LLDP_H */
