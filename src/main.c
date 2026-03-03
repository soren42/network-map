#include "config.h"
#include "cli.h"
#include "log.h"
#include "core/scan.h"
#include "core/graph.h"
#include "core/json_out.h"
#include "output/out_json.h"
#include "output/out_html.h"
#include "output/out_text.h"
#include "output/layout.h"

#ifdef HAVE_NCURSES
#include "output/out_curses.h"
#endif
#ifdef HAVE_CAIRO
#include "output/out_png.h"
#endif
#if defined(HAVE_FFMPEG) && defined(HAVE_CAIRO)
#include "output/out_mp4.h"
#endif

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv)
{
    nm_config_t cfg;
    if (nm_cli_parse(&cfg, argc, argv) != 0) {
        nm_cli_usage(argv[0]);
        return 1;
    }

    nm_log_set_level(cfg.verbosity);
    LOG_INFO("network-map %s starting", NM_VERSION);

    nm_graph_t *g = NULL;

    if (cfg.load_from_json) {
        g = nm_json_load_file(cfg.json_input_path);
        if (!g) {
            LOG_ERROR("Failed to load graph from '%s'", cfg.json_input_path);
            return 1;
        }
        LOG_INFO("Loaded %d hosts, %d edges from %s",
                 g->host_count, g->edge_count, cfg.json_input_path);
    } else {
        g = nm_scan_run(&cfg);
        if (!g) {
            LOG_ERROR("Discovery failed");
            return 1;
        }
        LOG_INFO("Discovered %d hosts, %d edges", g->host_count, g->edge_count);
    }

    int mst_edges = nm_graph_kruskal_mst(g);
    LOG_INFO("MST: %d edges", mst_edges);

    if (cfg.output_flags & (NM_OUT_PNG | NM_OUT_MP4 | NM_OUT_HTML)) {
        nm_layout_3d(g);
        nm_layout_force_refine(g, 50);
        LOG_INFO("Layout computed");
    }

    char path[512];

    if (cfg.output_flags & NM_OUT_TEXT) {
        nm_out_text(g);
    }

    if (cfg.output_flags & NM_OUT_JSON) {
        snprintf(path, sizeof(path), "%s.json", cfg.file_base);
        nm_out_json(g, path);
    }

#ifdef HAVE_NCURSES
    if (cfg.output_flags & NM_OUT_CURSES) {
        nm_out_curses(g);
    }
#else
    if (cfg.output_flags & NM_OUT_CURSES) {
        LOG_WARN("Curses output not available (ncurses not found)");
    }
#endif

#ifdef HAVE_CAIRO
    if (cfg.output_flags & NM_OUT_PNG) {
        snprintf(path, sizeof(path), "%s.png", cfg.file_base);
        if (!(cfg.output_flags & (NM_OUT_MP4 | NM_OUT_HTML))) {
            nm_layout_radial_2d(g);
            nm_layout_force_refine(g, 50);
        }
        nm_out_png(g, path);
    }
#else
    if (cfg.output_flags & NM_OUT_PNG) {
        LOG_WARN("PNG output not available (cairo not found)");
    }
#endif

#if defined(HAVE_FFMPEG) && defined(HAVE_CAIRO)
    if (cfg.output_flags & NM_OUT_MP4) {
        snprintf(path, sizeof(path), "%s.mp4", cfg.file_base);
        nm_out_mp4(g, path);
    }
#else
    if (cfg.output_flags & NM_OUT_MP4) {
        LOG_WARN("MP4 output not available (ffmpeg/cairo not found)");
    }
#endif

    if (cfg.output_flags & NM_OUT_HTML) {
        snprintf(path, sizeof(path), "%s.html", cfg.file_base);
        nm_out_html(g, path);
    }

    nm_graph_destroy(g);
    LOG_INFO("Done");
    return 0;
}
