#ifndef NM_CONFFILE_H
#define NM_CONFFILE_H

#include "cli.h"

/* Load configuration from config files and environment variables.
   Only writes fields that are still at their default (empty) value,
   so CLI flags always win. Load order:
     1. /etc/netmap.conf
     2. $XDG_CONFIG_HOME/netmap/netmap.conf or ~/.config/netmap/netmap.conf
     3. Environment: NETMAP_UNIFI_HOST, NETMAP_UNIFI_USER, NETMAP_UNIFI_PASS
   Returns 0 on success, -1 on parse error (warnings only, non-fatal). */
int nm_conffile_load(nm_config_t *cfg);

#endif /* NM_CONFFILE_H */
