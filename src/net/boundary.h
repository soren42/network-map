#ifndef NM_BOUNDARY_H
#define NM_BOUNDARY_H

#include "core/graph.h"
#include <netinet/in.h>

/* Check if an IPv4 address is private (RFC1918/6598/3927) */
int nm_is_private_ip(struct in_addr addr);

/* Detect NAT boundary by tracerouting outward from the gateway.
   Finds the last private hop and marks it as BOUNDARY type.
   Returns boundary host ID, or -1 if not found. */
int nm_boundary_detect(nm_graph_t *g);

/* Manually set a boundary host from CLI argument.
   Returns boundary host ID, or -1 on error. */
int nm_boundary_set(nm_graph_t *g, const char *host_str);

#endif /* NM_BOUNDARY_H */
