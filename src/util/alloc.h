#ifndef NM_ALLOC_H
#define NM_ALLOC_H

#include <stddef.h>

/* Checked allocation - aborts on failure */
void *nm_malloc(size_t size);
void *nm_calloc(size_t count, size_t size);
void *nm_realloc(void *ptr, size_t size);
char *nm_strdup(const char *s);
void  nm_free(void *ptr);

#endif /* NM_ALLOC_H */
