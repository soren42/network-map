#ifndef NM_OUT_CURSES_H
#define NM_OUT_CURSES_H

#include "core/graph.h"

/* Display graph as color-coded BFS tree in terminal using ncurses.
   Returns 0 on success. */
int nm_out_curses(const nm_graph_t *g);

#endif /* NM_OUT_CURSES_H */
