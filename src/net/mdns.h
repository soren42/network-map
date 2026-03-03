#ifndef NM_MDNS_H
#define NM_MDNS_H

#include "core/graph.h"

/* Browse for mDNS services and update hosts in graph.
   Returns number of mDNS names resolved. */
int nm_mdns_browse(nm_graph_t *g, int timeout_ms);

#endif /* NM_MDNS_H */
