#include "util/strutil.h"
#include "util/alloc.h"
#include "log.h"
#include <stdio.h>
#include <string.h>

char *nm_strlcpy(char *dst, const char *src, size_t size)
{
    if (size == 0) return dst;
    size_t i;
    for (i = 0; i < size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
    return dst;
}

char *nm_strlcat(char *dst, const char *src, size_t size)
{
    size_t dlen = strlen(dst);
    if (dlen >= size) return dst;
    nm_strlcpy(dst + dlen, src, size - dlen);
    return dst;
}

void nm_mac_to_str(const unsigned char *mac, char *buf, size_t buflen)
{
    snprintf(buf, buflen, "%02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

int nm_str_to_mac(const char *str, unsigned char *mac)
{
    unsigned int m[6];
    if (sscanf(str, "%x:%x:%x:%x:%x:%x",
               &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        return -1;
    }
    for (int i = 0; i < 6; i++) {
        if (m[i] > 0xFF) return -1;
        mac[i] = (unsigned char)m[i];
    }
    return 0;
}

int nm_str_empty(const char *s)
{
    return !s || s[0] == '\0';
}

void nm_str_unescape_mdns(char *s)
{
    if (!s) return;
    char *r = s, *w = s;
    while (*r) {
        if (r[0] == '\\' && r[1] >= '0' && r[1] <= '9' &&
            r[2] >= '0' && r[2] <= '9' &&
            r[3] >= '0' && r[3] <= '9') {
            int val = (r[1] - '0') * 100 + (r[2] - '0') * 10 + (r[3] - '0');
            if (val > 0 && val < 256) {
                *w++ = (char)val;
                r += 4;
                continue;
            }
        }
        *w++ = *r++;
    }
    *w = '\0';
}

#define READ_ALL_INIT  (64 * 1024)
#define READ_ALL_MAX   (16 * 1024 * 1024)

char *nm_read_all_fp(FILE *fp, size_t *out_len, size_t max_bytes)
{
    if (!fp) return NULL;
    size_t limit = (max_bytes > 0) ? max_bytes : READ_ALL_MAX;
    size_t cap = READ_ALL_INIT;
    size_t len = 0;
    char *buf = nm_malloc(cap);

    while (!feof(fp)) {
        if (len + 4096 > cap) {
            if (cap >= limit) {
                LOG_ERROR("read_all: output exceeds %zu byte limit", limit);
                nm_free(buf);
                return NULL;
            }
            cap *= 2;
            if (cap > limit) cap = limit;
            buf = nm_realloc(buf, cap);
        }
        size_t n = fread(buf + len, 1, cap - len - 1, fp);
        if (n == 0) break;
        len += n;
    }
    buf[len] = '\0';
    if (out_len) *out_len = len;
    return buf;
}
