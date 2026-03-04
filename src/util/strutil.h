#ifndef NM_STRUTIL_H
#define NM_STRUTIL_H

#include <stddef.h>
#include <stdio.h>

/* Safe string copy - always NUL-terminates, returns dst */
char *nm_strlcpy(char *dst, const char *src, size_t size);

/* Safe string concatenation - always NUL-terminates */
char *nm_strlcat(char *dst, const char *src, size_t size);

/* Format MAC address as "aa:bb:cc:dd:ee:ff" */
void nm_mac_to_str(const unsigned char *mac, char *buf, size_t buflen);

/* Parse MAC from string, returns 0 on success */
int nm_str_to_mac(const char *str, unsigned char *mac);

/* Check if a string is empty or NULL */
int nm_str_empty(const char *s);

/* Decode mDNS escape sequences (\032 -> space, etc.) in-place */
void nm_str_unescape_mdns(char *s);

/* Read all output from a FILE* into a malloc'd buffer.
   Returns buffer (NUL-terminated) and sets *out_len.
   max_bytes caps the read (0 = 16 MB default).
   Caller must nm_free() the result. Returns NULL on error. */
char *nm_read_all_fp(FILE *fp, size_t *out_len, size_t max_bytes);

#endif /* NM_STRUTIL_H */
