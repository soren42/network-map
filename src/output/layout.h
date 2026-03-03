#ifndef NM_LAYOUT_H
#define NM_LAYOUT_H

#include "core/graph.h"

/* Compute 2D radial tree layout. Updates x,y coordinates on hosts.
   Returns 0 on success. */
int nm_layout_radial_2d(nm_graph_t *g);

/* Compute 3D layout with network spine along Z axis.
   Returns 0 on success. */
int nm_layout_3d(nm_graph_t *g);

/* Apply Fruchterman-Reingold force-directed refinement.
   iterations: number of FR passes (50 recommended). */
void nm_layout_force_refine(nm_graph_t *g, int iterations);

#endif /* NM_LAYOUT_H */
