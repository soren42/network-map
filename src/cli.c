#include "cli.h"
#include "log.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

void nm_cli_defaults(nm_config_t *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->output_flags = NM_OUT_TEXT; /* default output */
    strcpy(cfg->file_base, "intranet");
}

static unsigned int parse_output_formats(const char *str)
{
    unsigned int flags = 0;
    char buf[256];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, ",");
    while (tok) {
        while (*tok == ' ') tok++;
        if (strcmp(tok, "text") == 0)         flags |= NM_OUT_TEXT;
        else if (strcmp(tok, "json") == 0)    flags |= NM_OUT_JSON;
        else if (strcmp(tok, "curses") == 0)  flags |= NM_OUT_CURSES;
        else if (strcmp(tok, "png") == 0)     flags |= NM_OUT_PNG;
        else if (strcmp(tok, "mp4") == 0)     flags |= NM_OUT_MP4;
        else if (strcmp(tok, "html") == 0)    flags |= NM_OUT_HTML;
        else {
            fprintf(stderr, "Unknown output format: %s\n", tok);
            return 0;
        }
        tok = strtok(NULL, ",");
    }
    return flags;
}

void nm_cli_usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS] [BOUNDARY_HOST]\n"
        "\n"
        "Network Map - Local Network Discovery & Visualization\n"
        "\n"
        "Options:\n"
        "  -v                   Increase verbosity (-v to -vvvvv)\n"
        "  -o, --output FMT     Output formats: text,json,curses,png,mp4,html\n"
        "  -f, --file PATH      Output filename base (default: intranet)\n"
        "  -4                   IPv4 only\n"
        "  -6                   IPv6 only\n"
        "  --no-mdns            Disable mDNS discovery\n"
        "  --no-arp             Disable ARP cache reading\n"
        "  --no-nmap            Disable nmap service scanning\n"
        "  --fast               Fast mode (reduce timeouts, skip slow probes)\n"
        "  -n, --nameserver IP  Use custom DNS server for reverse lookups\n"
        "  --from-json FILE     Re-render outputs from a previously exported JSON file\n"
        "  --no-lldp            Disable LLDP discovery\n"
        "  --no-unifi           Disable UniFi API discovery\n"
        "  --unifi-host HOST    UniFi controller hostname/IP\n"
        "  --unifi-user USER    UniFi controller username\n"
        "  --unifi-pass PASS    UniFi controller password\n"
        "  --unifi-site SITE    UniFi site name (default: default)\n"
        "  -h, --help           Show this help\n"
        "  --version            Show version\n"
        "\n"
        "Positional:\n"
        "  BOUNDARY_HOST        Optional upstream host marking network boundary\n"
        "\n", prog);
}

int nm_cli_parse(nm_config_t *cfg, int argc, char **argv)
{
    nm_cli_defaults(cfg);

    static struct option long_opts[] = {
        {"output",      required_argument, NULL, 'o'},
        {"file",        required_argument, NULL, 'f'},
        {"no-mdns",     no_argument,       NULL, 'M'},
        {"no-arp",      no_argument,       NULL, 'A'},
        {"no-nmap",     no_argument,       NULL, 'N'},
        {"fast",        no_argument,       NULL, 'F'},
        {"nameserver",  required_argument, NULL, 'n'},
        {"from-json",   required_argument, NULL, 'J'},
        {"no-lldp",     no_argument,       NULL, 'L'},
        {"no-unifi",    no_argument,       NULL, 'U'},
        {"unifi-host",  required_argument, NULL, 1001},
        {"unifi-user",  required_argument, NULL, 1002},
        {"unifi-pass",  required_argument, NULL, 1003},
        {"unifi-site",  required_argument, NULL, 1004},
        {"help",        no_argument,       NULL, 'h'},
        {"version",     no_argument,       NULL, 'V'},
        {NULL, 0, NULL, 0}
    };

    int opt;
    int v_count = 0;

    /* Reset getopt for testability */
    optind = 1;

    while ((opt = getopt_long(argc, argv, "vo:f:n:46h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'v':
            v_count++;
            break;
        case 'o': {
            unsigned int flags = parse_output_formats(optarg);
            if (flags == 0) return -1;
            cfg->output_flags = flags;
            break;
        }
        case 'f':
            strncpy(cfg->file_base, optarg, sizeof(cfg->file_base) - 1);
            break;
        case '4':
            cfg->ipv4_only = 1;
            cfg->ipv6_only = 0;
            break;
        case '6':
            cfg->ipv6_only = 1;
            cfg->ipv4_only = 0;
            break;
        case 'M':
            cfg->no_mdns = 1;
            break;
        case 'A':
            cfg->no_arp = 1;
            break;
        case 'N':
            cfg->no_nmap = 1;
            break;
        case 'F':
            cfg->fast_mode = 1;
            break;
        case 'n':
            strncpy(cfg->nameserver, optarg, sizeof(cfg->nameserver) - 1);
            break;
        case 'J':
            strncpy(cfg->json_input_path, optarg,
                    sizeof(cfg->json_input_path) - 1);
            cfg->load_from_json = 1;
            break;
        case 'L':
            cfg->no_lldp = 1;
            break;
        case 'U':
            cfg->no_unifi = 1;
            break;
        case 1001:
            strncpy(cfg->unifi_host, optarg, sizeof(cfg->unifi_host) - 1);
            break;
        case 1002:
            strncpy(cfg->unifi_user, optarg, sizeof(cfg->unifi_user) - 1);
            break;
        case 1003:
            strncpy(cfg->unifi_pass, optarg, sizeof(cfg->unifi_pass) - 1);
            break;
        case 1004:
            strncpy(cfg->unifi_site, optarg, sizeof(cfg->unifi_site) - 1);
            break;
        case 'h':
            nm_cli_usage(argv[0]);
            exit(0);
        case 'V':
            printf("network-map %s\n", NM_VERSION);
            exit(0);
        default:
            return -1;
        }
    }

    cfg->verbosity = v_count;
    if (cfg->verbosity > 5) cfg->verbosity = 5;

    /* Positional argument after options = boundary_host */
    if (optind < argc) {
        strncpy(cfg->boundary_host, argv[optind],
                sizeof(cfg->boundary_host) - 1);
        cfg->has_boundary = 1;
    }

    return 0;
}
