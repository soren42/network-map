#include "core/types.h"
#include <string.h>

const char *nm_medium_str(nm_medium_t m)
{
    switch (m) {
    case NM_MEDIUM_UNKNOWN: return "unknown";
    case NM_MEDIUM_WIRED:   return "wired";
    case NM_MEDIUM_WIFI:    return "wifi";
    case NM_MEDIUM_MOCA:    return "moca";
    }
    return "unknown";
}

nm_medium_t nm_medium_from_str(const char *s)
{
    if (!s) return NM_MEDIUM_UNKNOWN;
    if (strcmp(s, "wired") == 0) return NM_MEDIUM_WIRED;
    if (strcmp(s, "wifi") == 0)  return NM_MEDIUM_WIFI;
    if (strcmp(s, "moca") == 0)  return NM_MEDIUM_MOCA;
    return NM_MEDIUM_UNKNOWN;
}
