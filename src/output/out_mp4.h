#ifndef NM_OUT_MP4_H
#define NM_OUT_MP4_H

#include "core/graph.h"

/* Render graph as rotating 3D MP4 video. Returns 0 on success. */
int nm_out_mp4(const nm_graph_t *g, const char *filename);

#endif /* NM_OUT_MP4_H */
