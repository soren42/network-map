#include "core/host.h"
#include "core/types.h"
#include "util/strutil.h"
#include <stdio.h>
#include <string.h>

extern int g_tests_run, g_tests_passed, g_tests_failed;
#define TEST_ASSERT(cond, msg) do { \
    g_tests_run++; \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
        g_tests_failed++; \
    } else { \
        g_tests_passed++; \
    } \
} while(0)
#define TEST_ASSERT_EQ(a, b, msg) TEST_ASSERT((a) == (b), msg)
#define TEST_ASSERT_STR(a, b, msg) TEST_ASSERT(strcmp((a),(b)) == 0, msg)

static void test_host_init(void)
{
    nm_host_t h;
    nm_host_init(&h);
    TEST_ASSERT(h.id == -1, "init id is -1");
    TEST_ASSERT(h.type == NM_HOST_WORKSTATION, "init type is WORKSTATION");
    TEST_ASSERT(h.hop_distance == -1, "init hop is -1");
    TEST_ASSERT(!h.has_ipv4, "init has no ipv4");
    TEST_ASSERT(!h.has_ipv6, "init has no ipv6");
    TEST_ASSERT(!h.has_mac, "init has no mac");
    TEST_ASSERT_EQ(h.service_count, 0, "init service_count is 0");
    TEST_ASSERT(!h.is_boundary, "init is_boundary is 0");
}

static void test_host_set_ipv4(void)
{
    nm_host_t h;
    nm_host_init(&h);
    int rc = nm_host_set_ipv4(&h, "192.168.1.1");
    TEST_ASSERT(rc == 0, "set ipv4 ok");
    TEST_ASSERT(h.has_ipv4, "has ipv4 flag set");
    TEST_ASSERT_STR(nm_host_ipv4_str(&h), "192.168.1.1", "ipv4 string");

    rc = nm_host_set_ipv4(&h, "invalid");
    TEST_ASSERT(rc != 0, "invalid ipv4 fails");
}

static void test_host_display_name(void)
{
    nm_host_t h;

    /* DNS takes top priority */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.1");
    nm_strlcpy(h.hostname, "myhost", sizeof(h.hostname));
    nm_strlcpy(h.dns_name, "myhost.local", sizeof(h.dns_name));
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "myhost.local", "dns priority over hostname");

    /* Hostname used if no DNS (and not an iface name) */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.1");
    nm_strlcpy(h.hostname, "myhost", sizeof(h.hostname));
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "myhost", "hostname when no dns");

    /* Interface-like hostname skipped in favor of IP */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.1");
    nm_strlcpy(h.hostname, "en0", sizeof(h.hostname));
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "10.0.0.1", "iface hostname skipped");

    /* mDNS if no hostname/dns, with unescape */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.2");
    nm_strlcpy(h.mdns_name, "my\\032printer._http._tcp", sizeof(h.mdns_name));
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "my printer._http._tcp", "mdns unescaped");

    /* DNS if no hostname or mDNS */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.3");
    nm_strlcpy(h.dns_name, "server.example.com", sizeof(h.dns_name));
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "server.example.com", "dns priority");

    /* IP if no names */
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "10.0.0.4");
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "10.0.0.4", "ip fallback");

    /* Unknown if nothing */
    nm_host_init(&h);
    nm_host_compute_display_name(&h);
    TEST_ASSERT_STR(h.display_name, "(unknown)", "unknown fallback");
}

static void test_host_add_service(void)
{
    nm_host_t h;
    nm_host_init(&h);
    TEST_ASSERT_EQ(h.service_count, 0, "initially no services");

    int rc = nm_host_add_service(&h, 80, "tcp", "http", "Apache 2.4");
    TEST_ASSERT_EQ(rc, 0, "add service returns 0");
    TEST_ASSERT_EQ(h.service_count, 1, "service_count is 1");
    TEST_ASSERT_EQ(h.services[0].port, 80, "service port is 80");
    TEST_ASSERT_STR(h.services[0].proto, "tcp", "service proto is tcp");
    TEST_ASSERT_STR(h.services[0].name, "http", "service name is http");
    TEST_ASSERT_STR(h.services[0].version, "Apache 2.4", "service version");

    rc = nm_host_add_service(&h, 22, "tcp", "ssh", NULL);
    TEST_ASSERT_EQ(rc, 0, "add second service ok");
    TEST_ASSERT_EQ(h.service_count, 2, "service_count is 2");
    TEST_ASSERT_EQ(h.services[1].port, 22, "second service port is 22");
    TEST_ASSERT_STR(h.services[1].name, "ssh", "second service name is ssh");
}

static void test_host_classify(void)
{
    nm_host_t h;

    /* Server with port 80 gets SERVER type */
    nm_host_init(&h);
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_SERVER, "port 80 classified as SERVER");

    /* Host with port 631 gets PRINTER type */
    nm_host_init(&h);
    nm_host_add_service(&h, 631, "tcp", "ipp", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_PRINTER, "port 631 classified as PRINTER");

    /* Host with port 1883 gets IOT type */
    nm_host_init(&h);
    nm_host_add_service(&h, 1883, "tcp", "mqtt", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_IOT, "port 1883 classified as IOT");

    /* Host with port 22 gets SERVER type */
    nm_host_init(&h);
    nm_host_add_service(&h, 22, "tcp", "ssh", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_SERVER, "port 22 classified as SERVER");

    /* Host with no services stays WORKSTATION */
    nm_host_init(&h);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_WORKSTATION, "no services stays WORKSTATION");

    /* LOCAL type is not reclassified */
    nm_host_init(&h);
    h.type = NM_HOST_LOCAL;
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_LOCAL, "LOCAL not reclassified");

    /* GATEWAY type is not reclassified */
    nm_host_init(&h);
    h.type = NM_HOST_GATEWAY;
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_GATEWAY, "GATEWAY not reclassified");
}

static void test_host_type_str(void)
{
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_LOCAL), "local", "type local");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_GATEWAY), "gateway", "type gateway");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_SERVER), "server", "type server");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_WORKSTATION), "workstation", "type workstation");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_PRINTER), "printer", "type printer");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_IOT), "iot", "type iot");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_BOUNDARY), "boundary", "type boundary");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_SWITCH), "switch", "type switch");
    TEST_ASSERT_STR(nm_host_type_str(NM_HOST_AP), "ap", "type ap");

    /* Round-trip */
    TEST_ASSERT_EQ((int)nm_host_type_from_str("switch"), (int)NM_HOST_SWITCH,
                   "switch from_str round-trip");
    TEST_ASSERT_EQ((int)nm_host_type_from_str("ap"), (int)NM_HOST_AP,
                   "ap from_str round-trip");
}

static void test_host_medium_str(void)
{
    TEST_ASSERT_STR(nm_medium_str(NM_MEDIUM_UNKNOWN), "unknown", "medium unknown");
    TEST_ASSERT_STR(nm_medium_str(NM_MEDIUM_WIRED), "wired", "medium wired");
    TEST_ASSERT_STR(nm_medium_str(NM_MEDIUM_WIFI), "wifi", "medium wifi");
    TEST_ASSERT_STR(nm_medium_str(NM_MEDIUM_MOCA), "moca", "medium moca");

    TEST_ASSERT_EQ((int)nm_medium_from_str("wired"), (int)NM_MEDIUM_WIRED,
                   "wired from_str");
    TEST_ASSERT_EQ((int)nm_medium_from_str("wifi"), (int)NM_MEDIUM_WIFI,
                   "wifi from_str");
    TEST_ASSERT_EQ((int)nm_medium_from_str("moca"), (int)NM_MEDIUM_MOCA,
                   "moca from_str");
    TEST_ASSERT_EQ((int)nm_medium_from_str("garbage"), (int)NM_MEDIUM_UNKNOWN,
                   "unknown from_str");
    TEST_ASSERT_EQ((int)nm_medium_from_str(NULL), (int)NM_MEDIUM_UNKNOWN,
                   "NULL from_str");
}

static void test_host_classify_switch_ap(void)
{
    nm_host_t h;

    /* SWITCH type is not reclassified */
    nm_host_init(&h);
    h.type = NM_HOST_SWITCH;
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_SWITCH, "SWITCH not reclassified");

    /* AP type is not reclassified */
    nm_host_init(&h);
    h.type = NM_HOST_AP;
    nm_host_add_service(&h, 80, "tcp", "http", NULL);
    nm_host_classify(&h);
    TEST_ASSERT_EQ((int)h.type, (int)NM_HOST_AP, "AP not reclassified");
}

void test_host_suite(void)
{
    test_host_init();
    test_host_set_ipv4();
    test_host_display_name();
    test_host_add_service();
    test_host_classify();
    test_host_type_str();
    test_host_medium_str();
    test_host_classify_switch_ap();
}
