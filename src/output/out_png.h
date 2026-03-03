#ifndef NM_OUT_PNG_H
#define NM_OUT_PNG_H

#include "core/graph.h"

/* Render graph as PNG image. Returns 0 on success. */
int nm_out_png(const nm_graph_t *g, const char *filename);

#endif /* NM_OUT_PNG_H */
