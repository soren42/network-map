#include "net/nmap.h"
#include <stdio.h>

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

/* Just test nm_nmap_available() returns 0 or 1 */
static void test_nmap_available(void)
{
    int r = nm_nmap_available();
    TEST_ASSERT(r == 0 || r == 1, "nmap_available returns bool");
}

void test_nmap_suite(void)
{
    test_nmap_available();
}
