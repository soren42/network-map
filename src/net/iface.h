#ifndef NM_IFACE_H
#define NM_IFACE_H

#include "core/graph.h"
#include "cli.h"

/* Enumerate local network interfaces and add them to the graph.
   If cfg->iface_filter is non-empty, only interfaces matching the
   comma-separated list are included.
   Returns number of interfaces found. */
int nm_iface_enumerate(nm_graph_t *g, const nm_config_t *cfg);

#endif /* NM_IFACE_H */
