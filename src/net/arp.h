#ifndef NM_ARP_H
#define NM_ARP_H

#include "core/graph.h"

/* Read ARP cache and add/update hosts in the graph.
   Returns number of ARP entries found. */
int nm_arp_read(nm_graph_t *g);

#endif /* NM_ARP_H */
