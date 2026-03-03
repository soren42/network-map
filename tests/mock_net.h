#ifndef NM_MOCK_NET_H
#define NM_MOCK_NET_H
#include "core/graph.h"
/* Build a sample graph for testing.
   Creates: local host, gateway, 3 workstation hosts, 1 server, 1 printer, 1 boundary.
   Returns a graph with 8 hosts and appropriate edges. */
nm_graph_t *mock_build_sample_graph(void);
#endif
