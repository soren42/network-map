#ifndef NM_JSON_OUT_H
#define NM_JSON_OUT_H

#include "core/graph.h"
#include "cJSON.h"

/* Serialize graph to cJSON object. Caller must cJSON_Delete. */
cJSON *nm_json_serialize(const nm_graph_t *g);

/* Deserialize a cJSON object back into a graph. Caller owns the graph. */
nm_graph_t *nm_json_deserialize(const cJSON *root);

/* Load a JSON file from disk, parse, and deserialize into a graph.
   Returns NULL on error (prints to stderr). */
nm_graph_t *nm_json_load_file(const char *path);

#endif /* NM_JSON_OUT_H */
