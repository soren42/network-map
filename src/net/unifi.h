#ifndef NM_UNIFI_H
#define NM_UNIFI_H

#include "core/graph.h"
#include "cli.h"

/* Check if UniFi API is available (curl in PATH + credentials configured) */
int nm_unifi_available(const nm_config_t *cfg);

/* Discover devices and clients from a UniFi controller.
   Queries the controller REST API to populate switches, APs,
   and client connectivity (L2/WiFi edges).
   Returns number of devices/clients discovered, or -1 on error.
   No-op stub when HAVE_CURL is not defined. */
int nm_unifi_discover(nm_graph_t *g, const nm_config_t *cfg);

#endif /* NM_UNIFI_H */
