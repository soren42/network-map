#ifndef NM_SCAN_H
#define NM_SCAN_H

#include "core/graph.h"
#include "cli.h"

/* Progress callback */
typedef void (*nm_progress_fn)(const char *msg, void *ctx);
void nm_scan_set_progress(nm_progress_fn fn, void *ctx);

/* Run the full discovery engine. Returns populated graph. */
nm_graph_t *nm_scan_run(const nm_config_t *cfg);

/* Individual scan phases */
int nm_scan_local_interfaces(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_routing_table(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_lan_discovery(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_name_resolution(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_boundary_detect(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_nmap_enrich(nm_graph_t *g, const nm_config_t *cfg);
int nm_scan_ipv6_augment(nm_graph_t *g, const nm_config_t *cfg);

#endif /* NM_SCAN_H */
