#include "config.h"
#ifdef HAVE_NCURSES

#include "output/out_curses.h"
#include "util/alloc.h"
#include "util/strutil.h"
#include "log.h"

#include <curses.h>
#include <string.h>

/* Color pairs */
#define CP_LOCAL       1
#define CP_GATEWAY     2
#define CP_SERVER      3
#define CP_WORKSTATION 4
#define CP_PRINTER     5
#define CP_IOT         6
#define CP_BOUNDARY    7
#define CP_EDGE        8

static int color_for_type(nm_host_type_t type)
{
    switch (type) {
    case NM_HOST_LOCAL:       return COLOR_PAIR(CP_LOCAL);
    case NM_HOST_GATEWAY:     return COLOR_PAIR(CP_GATEWAY);
    case NM_HOST_SERVER:      return COLOR_PAIR(CP_SERVER);
    case NM_HOST_WORKSTATION: return COLOR_PAIR(CP_WORKSTATION);
    case NM_HOST_PRINTER:     return COLOR_PAIR(CP_PRINTER);
    case NM_HOST_IOT:         return COLOR_PAIR(CP_IOT);
    case NM_HOST_BOUNDARY:    return COLOR_PAIR(CP_BOUNDARY);
    case NM_HOST_SWITCH:      return COLOR_PAIR(CP_SERVER);
    case NM_HOST_AP:          return COLOR_PAIR(CP_LOCAL);
    }
    return 0;
}

static void print_tree(WINDOW *win, const nm_graph_t *g,
                       const int *parent, const int *depth,
                       int node, int row, int indent)
{
    if (row >= LINES - 1) return;

    const nm_host_t *h = &g->hosts[node];
    int attr = color_for_type(h->type) | A_BOLD;

    wattron(win, attr);
    mvwprintw(win, row, indent, "%s", h->display_name);
    wattroff(win, attr);

    /* Print IP and type info */
    if (h->has_ipv4) {
        mvwprintw(win, row, indent + 30, "%-16s", nm_host_ipv4_str(h));
    }
    mvwprintw(win, row, indent + 48, "[%s]", nm_host_type_str(h->type));

    /* Print OS info */
    if (h->os_name[0] != '\0') {
        mvwprintw(win, row, indent + 62, "%s", h->os_name);
    }

    /* Print service count */
    if (h->service_count > 0) {
        mvwprintw(win, row, indent + 82, "(%d services)", h->service_count);
    }

    /* Print children */
    int next_row = row + 1;
    for (int i = 0; i < g->host_count; i++) {
        if (parent[i] == node && i != node) {
            if (next_row < LINES - 1) {
                mvwprintw(win, next_row, indent, "|");
                mvwprintw(win, next_row, indent + 1, "-- ");
            }
            print_tree(win, g, parent, depth, i, next_row, indent + 4);
            next_row++;
            /* Count descendants to skip proper rows */
            for (int j = 0; j < g->host_count; j++) {
                if (parent[j] >= 0 && depth[j] > depth[i])
                    next_row++;
            }
        }
    }
}

int nm_out_curses(const nm_graph_t *g)
{
    if (g->host_count == 0) {
        LOG_WARN("No hosts to display");
        return -1;
    }

    int *parent = nm_calloc((size_t)g->host_count, sizeof(int));
    int *depth = nm_calloc((size_t)g->host_count, sizeof(int));

    int src = 0;
    for (int i = 0; i < g->host_count; i++) {
        if (g->hosts[i].type == NM_HOST_LOCAL) { src = i; break; }
    }
    nm_graph_bfs_mst(g, src, parent, depth);

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(CP_LOCAL,       COLOR_GREEN,   -1);
        init_pair(CP_GATEWAY,     COLOR_YELLOW,  -1);
        init_pair(CP_SERVER,      COLOR_BLUE,    -1);
        init_pair(CP_WORKSTATION, COLOR_CYAN,    -1);
        init_pair(CP_PRINTER,     COLOR_MAGENTA, -1);
        init_pair(CP_IOT,         COLOR_RED,     -1);
        init_pair(CP_BOUNDARY,    COLOR_RED,     -1);
        init_pair(CP_EDGE,        COLOR_WHITE,   -1);
    }

    clear();
    mvprintw(0, 0, "NetworkMap - Network Topology (%d hosts, %d edges)",
             g->host_count, g->edge_count);
    mvprintw(1, 0, "Press 'q' to quit");
    mvprintw(2, 0, "---");

    print_tree(stdscr, g, parent, depth, src, 4, 0);

    refresh();

    int ch;
    while ((ch = getch()) != 'q' && ch != 'Q')
        ;

    endwin();
    nm_free(parent);
    nm_free(depth);
    return 0;
}

#endif /* HAVE_NCURSES */
