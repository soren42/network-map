#include "core/json_out.h"
#include "mock_net.h"
#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
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

static void test_json_serialize(void)
{
    nm_graph_t *g = mock_build_sample_graph();
    cJSON *json = nm_json_serialize(g);
    TEST_ASSERT(json != NULL, "serialize returns non-null");

    cJSON *hosts = cJSON_GetObjectItem(json, "hosts");
    TEST_ASSERT(hosts != NULL, "has hosts array");
    TEST_ASSERT(cJSON_IsArray(hosts), "hosts is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(hosts), 8, "8 hosts in array");

    cJSON *edges = cJSON_GetObjectItem(json, "edges");
    TEST_ASSERT(edges != NULL, "has edges array");
    TEST_ASSERT(cJSON_IsArray(edges), "edges is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(edges), 7, "7 edges in array");

    /* Check first host */
    cJSON *h0 = cJSON_GetArrayItem(hosts, 0);
    cJSON *name = cJSON_GetObjectItem(h0, "display_name");
    TEST_ASSERT(name != NULL, "host 0 has display_name");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(name), "my-machine") == 0,
                "host 0 display name");

    cJSON *type = cJSON_GetObjectItem(h0, "type");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(type), "local") == 0,
                "host 0 type is local");

    /* Check server host has os_name */
    cJSON *h4 = cJSON_GetArrayItem(hosts, 4);
    cJSON *os = cJSON_GetObjectItem(h4, "os_name");
    TEST_ASSERT(os != NULL, "server host has os_name");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(os), "Ubuntu 22.04") == 0,
                "server os_name value");

    /* Check server host has services */
    cJSON *svcs = cJSON_GetObjectItem(h4, "services");
    TEST_ASSERT(svcs != NULL, "server host has services");
    TEST_ASSERT(cJSON_IsArray(svcs), "services is array");
    TEST_ASSERT_EQ(cJSON_GetArraySize(svcs), 2, "server has 2 services");

    /* Check IoT host has manufacturer */
    cJSON *h6 = cJSON_GetArrayItem(hosts, 6);
    cJSON *mfr = cJSON_GetObjectItem(h6, "manufacturer");
    TEST_ASSERT(mfr != NULL, "IoT host has manufacturer");
    TEST_ASSERT(strcmp(cJSON_GetStringValue(mfr), "Espressif") == 0,
                "IoT manufacturer value");

    /* Check boundary host has is_boundary */
    cJSON *h7 = cJSON_GetArrayItem(hosts, 7);
    cJSON *bnd = cJSON_GetObjectItem(h7, "is_boundary");
    TEST_ASSERT(bnd != NULL, "boundary host has is_boundary");
    TEST_ASSERT(cJSON_IsTrue(bnd), "is_boundary is true");

    cJSON_Delete(json);
    nm_graph_destroy(g);
}

static void test_json_roundtrip(void)
{
    nm_graph_t *g = mock_build_sample_graph();
    cJSON *json = nm_json_serialize(g);
    char *str = cJSON_Print(json);
    TEST_ASSERT(str != NULL, "print produces string");
    TEST_ASSERT(strlen(str) > 100, "json string is non-trivial");

    /* Parse it back */
    cJSON *parsed = cJSON_Parse(str);
    TEST_ASSERT(parsed != NULL, "can parse back");

    cJSON *hc = cJSON_GetObjectItem(parsed, "host_count");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(hc), 8, "host_count round-trip");

    cJSON *ec = cJSON_GetObjectItem(parsed, "edge_count");
    TEST_ASSERT_EQ((int)cJSON_GetNumberValue(ec), 7, "edge_count round-trip");

    cJSON_Delete(parsed);
    free(str);
    cJSON_Delete(json);
    nm_graph_destroy(g);
}

void test_json_suite(void)
{
    test_json_serialize();
    test_json_roundtrip();
}
