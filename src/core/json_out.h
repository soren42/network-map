#ifndef NM_JSON_OUT_H
#define NM_JSON_OUT_H

#include "core/graph.h"
#include "cJSON.h"

/* Serialize graph to cJSON object. Caller must cJSON_Delete. */
cJSON *nm_json_serialize(const nm_graph_t *g);

#endif /* NM_JSON_OUT_H */
