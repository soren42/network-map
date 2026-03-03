#include "net/boundary.h"
#include "core/graph.h"
#include "core/host.h"
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

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

/* Test nm_is_private_ip with known addresses */
static void test_is_private_ip(void)
{
    struct in_addr a;
    inet_pton(AF_INET, "192.168.1.1", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 1, "192.168.1.1 is private");
    inet_pton(AF_INET, "10.0.0.1", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 1, "10.0.0.1 is private");
    inet_pton(AF_INET, "172.16.0.1", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 1, "172.16.0.1 is private");
    inet_pton(AF_INET, "8.8.8.8", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 0, "8.8.8.8 is public");
    inet_pton(AF_INET, "100.64.0.1", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 1, "100.64.0.1 is CGN");
    inet_pton(AF_INET, "169.254.1.1", &a);
    TEST_ASSERT(nm_is_private_ip(a) == 1, "169.254.1.1 is link-local");
}

static void test_boundary_set(void)
{
    nm_graph_t *g = nm_graph_create();
    nm_host_t h;
    nm_host_init(&h);
    nm_host_set_ipv4(&h, "192.168.1.1");
    h.type = NM_HOST_LOCAL;
    nm_graph_add_host(g, &h);

    int bid = nm_boundary_set(g, "10.0.0.1");
    TEST_ASSERT(bid >= 0, "boundary set returns valid id");
    TEST_ASSERT(g->hosts[bid].is_boundary == 1, "boundary flag set");
    TEST_ASSERT_EQ((int)g->hosts[bid].type, (int)NM_HOST_BOUNDARY, "boundary type set");
    nm_graph_destroy(g);
}

void test_boundary_suite(void)
{
    test_is_private_ip();
    test_boundary_set();
}
