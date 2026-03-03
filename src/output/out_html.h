#ifndef NM_OUT_HTML_H
#define NM_OUT_HTML_H

#include "core/graph.h"

/* Generate self-contained HTML file with Three.js 3D visualization.
   Returns 0 on success. */
int nm_out_html(const nm_graph_t *g, const char *filename);

#endif /* NM_OUT_HTML_H */
