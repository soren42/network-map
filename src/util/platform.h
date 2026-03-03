#ifndef NM_PLATFORM_H
#define NM_PLATFORM_H

#include "config.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifdef NM_DARWIN
  #include <netinet/ip.h>
  #include <netinet/ip_icmp.h>
  /* macOS uses struct icmp from <netinet/ip_icmp.h> */
  #define NM_ICMP_TYPE(p)     ((p)->icmp_type)
  #define NM_ICMP_CODE(p)     ((p)->icmp_code)
  #define NM_ICMP_ID(p)       ((p)->icmp_hun.ih_idseq.icd_id)
  #define NM_ICMP_SEQ(p)      ((p)->icmp_hun.ih_idseq.icd_seq)
  #define NM_ICMP_CKSUM(p)    ((p)->icmp_cksum)
  typedef struct icmp nm_icmp_t;
  #define NM_ICMP_ECHO_REQUEST  ICMP_ECHO
  #define NM_ICMP_ECHO_REPLY    ICMP_ECHOREPLY
  #define NM_ICMP_TIME_EXCEEDED ICMP_TIMXCEED
  /* macOS raw socket includes IP header in recv buffer */
  #define NM_ICMP_RECV_HAS_IP_HDR 1
#endif

#ifdef NM_LINUX
  #include <netinet/ip_icmp.h>
  /* Linux uses struct icmphdr */
  #define NM_ICMP_TYPE(p)     ((p)->type)
  #define NM_ICMP_CODE(p)     ((p)->code)
  #define NM_ICMP_ID(p)       ((p)->un.echo.id)
  #define NM_ICMP_SEQ(p)      ((p)->un.echo.sequence)
  #define NM_ICMP_CKSUM(p)    ((p)->checksum)
  typedef struct icmphdr nm_icmp_t;
  #define NM_ICMP_ECHO_REQUEST  ICMP_ECHO
  #define NM_ICMP_ECHO_REPLY    ICMP_ECHOREPLY
  #define NM_ICMP_TIME_EXCEEDED ICMP_TIME_EXCEEDED
  /* Linux raw socket does NOT include IP header in recv */
  #define NM_ICMP_RECV_HAS_IP_HDR 0
#endif

/* Common constants */
#define NM_MAC_LEN       6
#define NM_MAC_STR_LEN   18  /* "aa:bb:cc:dd:ee:ff\0" */
#define NM_IPV4_STR_LEN  INET_ADDRSTRLEN
#define NM_IPV6_STR_LEN  INET6_ADDRSTRLEN
#define NM_HOSTNAME_LEN  256
#define NM_MAX_HOPS      30
#define NM_PING_TIMEOUT_MS 1000
#define NM_PING_PAYLOAD_SIZE 56

#endif /* NM_PLATFORM_H */
