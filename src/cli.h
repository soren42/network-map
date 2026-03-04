#ifndef NM_CLI_H
#define NM_CLI_H

#include <netinet/in.h>

/* Output format flags (bitmask) */
#define NM_OUT_TEXT    (1 << 0)
#define NM_OUT_JSON    (1 << 1)
#define NM_OUT_CURSES  (1 << 2)
#define NM_OUT_PNG     (1 << 3)
#define NM_OUT_MP4     (1 << 4)
#define NM_OUT_HTML    (1 << 5)

typedef struct {
    char             boundary_host[256];
    int              has_boundary;
    int              verbosity;
    unsigned int     output_flags;
    char             file_base[256];
    int              ipv4_only;
    int              ipv6_only;
    int              no_mdns;
    int              no_arp;
    int              no_nmap;
    int              fast_mode;
    char             nameserver[64];
    char             json_input_path[256];
    int              load_from_json;
    /* UniFi API configuration */
    char             unifi_host[256];
    char             unifi_user[128];
    char             unifi_pass[128];
    char             unifi_site[64];
    int              no_lldp;
    int              no_unifi;
} nm_config_t;

int nm_cli_parse(nm_config_t *cfg, int argc, char **argv);
void nm_cli_defaults(nm_config_t *cfg);
void nm_cli_usage(const char *prog);

#endif /* NM_CLI_H */
