#ifndef NM_ICMP_H
#define NM_ICMP_H

#include <netinet/in.h>

/* Send ICMP echo and wait for reply. Returns RTT in ms, or -1 on timeout/error. */
double nm_icmp_ping(struct in_addr target, int timeout_ms);

/* Send ICMP echo with specified TTL. Returns:
   0 = got echo reply (reached target), fills reply_addr and rtt_ms
   1 = got time exceeded (intermediate hop), fills reply_addr and rtt_ms
  -1 = timeout or error */
int nm_icmp_probe(struct in_addr target, int ttl, int timeout_ms,
                  struct in_addr *reply_addr, double *rtt_ms);

/* Compute ICMP checksum */
unsigned short nm_icmp_checksum(const void *data, int len);

#endif /* NM_ICMP_H */
