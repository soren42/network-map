#ifndef NM_TYPES_H
#define NM_TYPES_H

/* Connection medium for hosts and edges */
typedef enum {
    NM_MEDIUM_UNKNOWN = 0,
    NM_MEDIUM_WIRED   = 1,
    NM_MEDIUM_WIFI    = 2,
    NM_MEDIUM_MOCA    = 3
} nm_medium_t;

const char *nm_medium_str(nm_medium_t m);
nm_medium_t nm_medium_from_str(const char *s);

#endif /* NM_TYPES_H */
