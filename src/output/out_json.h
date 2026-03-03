#ifndef NM_OUT_JSON_H
#define NM_OUT_JSON_H

#include "core/graph.h"

/* Write graph as JSON to file. Returns 0 on success. */
int nm_out_json(const nm_graph_t *g, const char *filename);

#endif /* NM_OUT_JSON_H */
