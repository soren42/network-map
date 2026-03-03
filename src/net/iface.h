#ifndef NM_IFACE_H
#define NM_IFACE_H

#include "core/graph.h"

/* Enumerate local network interfaces and add them to the graph.
   Returns number of interfaces found. */
int nm_iface_enumerate(nm_graph_t *g);

#endif /* NM_IFACE_H */
