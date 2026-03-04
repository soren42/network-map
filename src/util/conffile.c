#include "util/conffile.h"
#include "util/strutil.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Trim leading and trailing whitespace in-place, return pointer into buf */
static char *trim(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

/* Set field only if currently empty */
static void set_if_empty(char *dst, size_t dstlen, const char *val)
{
    if (dst[0] == '\0' && val && val[0] != '\0')
        nm_strlcpy(dst, val, dstlen);
}

/* Parse a single INI-style config file into cfg.
   Supports [unifi] section with host, user, pass, site keys.
   Lines starting with # or ; are comments. */
static int parse_file(nm_config_t *cfg, const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp) return 0; /* not found is OK */

    LOG_DEBUG("Loading config from %s", path);

    char line[512];
    char section[64] = "";

    while (fgets(line, sizeof(line), fp)) {
        char *p = trim(line);

        /* Skip empty lines and comments */
        if (*p == '\0' || *p == '#' || *p == ';')
            continue;

        /* Section header */
        if (*p == '[') {
            char *end = strchr(p, ']');
            if (end) {
                *end = '\0';
                nm_strlcpy(section, p + 1, sizeof(section));
            }
            continue;
        }

        /* key = value */
        char *eq = strchr(p, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim(p);
        char *val = trim(eq + 1);

        /* Remove surrounding quotes from value */
        size_t vlen = strlen(val);
        if (vlen >= 2 &&
            ((val[0] == '"' && val[vlen-1] == '"') ||
             (val[0] == '\'' && val[vlen-1] == '\''))) {
            val[vlen-1] = '\0';
            val++;
        }

        if (strcmp(section, "unifi") == 0) {
            if (strcmp(key, "host") == 0)
                set_if_empty(cfg->unifi_host, sizeof(cfg->unifi_host), val);
            else if (strcmp(key, "user") == 0)
                set_if_empty(cfg->unifi_user, sizeof(cfg->unifi_user), val);
            else if (strcmp(key, "pass") == 0)
                set_if_empty(cfg->unifi_pass, sizeof(cfg->unifi_pass), val);
            else if (strcmp(key, "site") == 0)
                set_if_empty(cfg->unifi_site, sizeof(cfg->unifi_site), val);
        }
    }

    fclose(fp);
    return 0;
}

int nm_conffile_load(nm_config_t *cfg)
{
    /* 1. System-wide config */
    parse_file(cfg, "/etc/netmap.conf");

    /* 2. User config: $XDG_CONFIG_HOME/netmap/netmap.conf
       or ~/.config/netmap/netmap.conf */
    {
        char path[512];
        const char *xdg = getenv("XDG_CONFIG_HOME");
        if (xdg && xdg[0] != '\0') {
            snprintf(path, sizeof(path), "%s/netmap/netmap.conf", xdg);
        } else {
            const char *home = getenv("HOME");
            if (home && home[0] != '\0')
                snprintf(path, sizeof(path),
                         "%s/.config/netmap/netmap.conf", home);
            else
                path[0] = '\0';
        }
        if (path[0] != '\0')
            parse_file(cfg, path);
    }

    /* 3. Environment variables (override config files but not CLI) */
    const char *env;
    if ((env = getenv("NETMAP_UNIFI_HOST")) != NULL)
        set_if_empty(cfg->unifi_host, sizeof(cfg->unifi_host), env);
    if ((env = getenv("NETMAP_UNIFI_USER")) != NULL)
        set_if_empty(cfg->unifi_user, sizeof(cfg->unifi_user), env);
    if ((env = getenv("NETMAP_UNIFI_PASS")) != NULL)
        set_if_empty(cfg->unifi_pass, sizeof(cfg->unifi_pass), env);

    /* Default site to "default" if not set */
    if (cfg->unifi_site[0] == '\0')
        nm_strlcpy(cfg->unifi_site, "default", sizeof(cfg->unifi_site));

    return 0;
}
