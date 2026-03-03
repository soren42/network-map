#ifndef NM_HOST_H
#define NM_HOST_H

#include "util/platform.h"
#include <netinet/in.h>

typedef enum {
    NM_HOST_LOCAL       = 0,
    NM_HOST_GATEWAY     = 1,
    NM_HOST_SERVER      = 2,
    NM_HOST_WORKSTATION = 3,
    NM_HOST_PRINTER     = 4,
    NM_HOST_IOT         = 5,
    NM_HOST_BOUNDARY    = 6
} nm_host_type_t;

#define NM_MAX_ADDRS     8
#define NM_IFACES_LEN    256
#define NM_MAX_SERVICES  32

typedef struct {
    int   port;
    char  proto[8];        /* "tcp" or "udp" */
    char  name[64];        /* e.g. "http", "ssh" */
    char  version[128];    /* e.g. "Apache httpd 2.4.41" */
} nm_service_t;

typedef struct {
    int                  id;
    struct in_addr       ipv4;
    struct in6_addr      ipv6;
    unsigned char        mac[NM_MAC_LEN];
    char                 hostname[NM_HOSTNAME_LEN];
    char                 mdns_name[NM_HOSTNAME_LEN];
    char                 dns_name[NM_HOSTNAME_LEN];
    char                 display_name[NM_HOSTNAME_LEN];
    char                 iface_name[32];
    char                 interfaces[NM_IFACES_LEN];
    nm_host_type_t       type;
    int                  hop_distance;
    double               rtt_ms;
    int                  has_ipv4;
    int                  has_ipv6;
    int                  has_mac;
    /* Extended fields for network-map */
    char                 os_name[128];
    char                 manufacturer[128];
    nm_service_t         services[NM_MAX_SERVICES];
    int                  service_count;
    int                  is_boundary;
    /* Secondary addresses */
    struct in_addr       ipv4_addrs[NM_MAX_ADDRS];
    int                  ipv4_count;
    struct in6_addr      ipv6_addrs[NM_MAX_ADDRS];
    int                  ipv6_count;
    /* Layout coordinates */
    double               x, y, z;
} nm_host_t;

void nm_host_init(nm_host_t *h);
int nm_host_set_ipv4(nm_host_t *h, const char *addr);
int nm_host_set_ipv4_addr(nm_host_t *h, struct in_addr addr);
int nm_host_set_ipv6(nm_host_t *h, const char *addr);
int nm_host_set_mac(nm_host_t *h, const unsigned char *mac);
void nm_host_add_ipv4(nm_host_t *h, struct in_addr addr);
void nm_host_add_ipv6(nm_host_t *h, const struct in6_addr *addr);
void nm_host_add_iface(nm_host_t *h, const char *iface);
/* Add a service to the host, returns 0 on success */
int nm_host_add_service(nm_host_t *h, int port, const char *proto,
                        const char *name, const char *version);
/* Auto-classify host type based on services, MAC OUI, mDNS */
void nm_host_classify(nm_host_t *h);
void nm_host_compute_display_name(nm_host_t *h);
const char *nm_host_ipv4_str(const nm_host_t *h);
const char *nm_host_type_str(nm_host_type_t type);
nm_host_type_t nm_host_type_from_str(const char *str);

#endif /* NM_HOST_H */
