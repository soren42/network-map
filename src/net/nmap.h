#ifndef NM_NMAP_H
#define NM_NMAP_H

#include "core/graph.h"

/* Check if nmap is installed and accessible */
int nm_nmap_available(void);

/* Run nmap scan on a subnet CIDR string (e.g. "192.168.1.0/24").
   Parses XML output to enrich hosts with OS, services, vendor info.
   Returns number of hosts enriched, or -1 on error. */
int nm_nmap_scan_subnet(nm_graph_t *g, const char *cidr);

#endif /* NM_NMAP_H */
